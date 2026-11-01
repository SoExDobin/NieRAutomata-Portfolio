#include "JobManager.h"

#include "SpdLogger.h"
#include "String_Helper.h"

namespace
{
	HRESULT Run_Guarded(const std::function<HRESULT()>& job)
	{
		try
		{
			return job();
		}
		catch (const std::exception& exception)
		{
			LOG_ERROR(L"[JobManager] Job threw an exception : {}", Helper::To_wString(exception.what()));
		}
		catch (...)
		{
			LOG_ERROR(L"[JobManager] Job threw an unknown exception");
		}
		return E_FAIL;
	}
}

JobManager::~JobManager()
{
	On_Destroy();
}

HRESULT JobManager::Initialize(uint32 workerCount)
{
	if (workerCount == 0)
	{
		// hardware_concurrency may report 0 when it cannot tell.
		const uint32 cores = std::thread::hardware_concurrency();
		workerCount = cores > 1 ? cores - 1 : 1;
	}

	m_Workers.reserve(workerCount);
	for (uint32 i = 0; i < workerCount; ++i)
		m_Workers.emplace_back(&JobManager::Run_Worker, this, i);

	LOG_INFO(L"[JobManager] {} worker threads", workerCount);
	return S_OK;
}

void JobManager::On_Destroy()
{
	size_t discarded = 0;
	{
		std::lock_guard lock(m_QueueMutex);
		if (m_Stopping)
			return;
		m_Stopping = true;
		discarded = m_Queue.size();
		m_Queue.clear();
	}
	m_QueueCondition.notify_all();

	for (std::thread& worker : m_Workers)
	{
		if (worker.joinable())
			worker.join();
	}
	m_Workers.clear();

	if (discarded)
		LOG_WARN(L"[JobManager] Discarded {} queued jobs at shutdown", discarded);

	EngineManager::On_Destroy();
}

std::future<HRESULT> JobManager::Submit(std::function<HRESULT()> job)
{
	auto promise = make_shared<std::promise<HRESULT>>();
	std::future<HRESULT> future = promise->get_future();
	if (!job)
	{
		promise->set_value(E_INVALIDARG);
		return future;
	}

	if (!Push_Task([promise, job = std::move(job)] { promise->set_value(Run_Guarded(job)); }))
		promise->set_value(E_ABORT);
	return future;
}

HRESULT JobManager::Parallel_For(uint32 count, const std::function<HRESULT(uint32)>& job)
{
	if (count == 0)
		return S_OK;
	if (!job)
		return E_INVALIDARG;

	// Shared with the helper tasks, which may start after this call has returned and must then find no work.
	struct PARALLEL_STATE
	{
		std::function<HRESULT(uint32)> job;
		uint32 count{};
		std::atomic<uint32> nextIndex{ 0 };
		std::atomic<uint32> doneCount{ 0 };
		std::atomic<HRESULT> result{ S_OK };
		std::mutex doneMutex;
		std::condition_variable doneCondition;
	};
	auto state = make_shared<PARALLEL_STATE>();
	state->job = job;
	state->count = count;

	const auto runIndices = [](PARALLEL_STATE& shared)
		{
			for (uint32 index = shared.nextIndex++; index < shared.count; index = shared.nextIndex++)
			{
				if (SUCCEEDED(shared.result.load()))
				{
					const HRESULT hr = Run_Guarded([&shared, index] { return shared.job(index); });
					HRESULT expected = S_OK;
					if (FAILED(hr))
						shared.result.compare_exchange_strong(expected, hr);
				}
				if (++shared.doneCount == shared.count)
				{
					std::lock_guard lock(shared.doneMutex);
					shared.doneCondition.notify_all();
				}
			}
		};

	const uint32 helperCount = (std::min)(count - 1, static_cast<uint32>(m_Workers.size()));
	for (uint32 i = 0; i < helperCount; ++i)
		Push_Task([state, runIndices] { runIndices(*state); });

	runIndices(*state);

	std::unique_lock lock(state->doneMutex);
	state->doneCondition.wait(lock, [&state] { return state->doneCount.load() == state->count; });
	return state->result.load();
}

void JobManager::Run_Worker(uint32 workerIndex)
{
	SetThreadDescription(GetCurrentThread(), std::format(L"JobWorker {}", workerIndex).c_str());

	// Jobs decode textures through WIC, which needs COM on the calling thread.
	const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(comResult))
		LOG_ERROR(L"[JobManager] Worker {} failed to initialize COM (HRESULT 0x{:08X})", workerIndex, static_cast<uint32>(comResult));

	for (;;)
	{
		std::function<void()> task;
		{
			std::unique_lock lock(m_QueueMutex);
			m_QueueCondition.wait(lock, [this] { return m_Stopping || !m_Queue.empty(); });
			if (m_Stopping)
				break;
			task = std::move(m_Queue.front());
			m_Queue.pop_front();
		}
		task();
	}

	if (SUCCEEDED(comResult))
		CoUninitialize();
}

Bool JobManager::Push_Task(std::function<void()> task)
{
	{
		std::lock_guard lock(m_QueueMutex);
		if (m_Stopping)
			return false;
		m_Queue.push_back(std::move(task));
	}
	m_QueueCondition.notify_one();
	return true;
}

Unique<JobManager> JobManager::Create(uint32 workerCount)
{
	Unique<JobManager> instance = make_unique<JobManager>();
	if (FAILED(instance->Initialize(workerCount)))
		return nullptr;
	return instance;
}
