#pragma once
#include "EngineManager.h"

#include <condition_variable>
#include <deque>
#include <future>
#include <thread>

NS_BEGIN(Engine)

/* Worker thread pool owned by Game. Jobs may create resources through ID3D11Device,
   but must not touch the immediate context, which stays on the main thread. */
class JobManager final : public EngineManager
{
	NO_COPY(JobManager)
public:
	explicit JobManager() = default;
	~JobManager() override;

public:
	HRESULT Initialize(uint32 workerCount);
	void On_Destroy() override;

	// Runs the job on a worker. The future holds its HRESULT, E_FAIL if it threw, or E_ABORT if the pool has stopped.
	std::future<HRESULT> Submit(std::function<HRESULT()> job);
	// Runs job(0 .. count - 1) on the workers and the calling thread, and returns once every index has finished.
	// Returns the first failure; indices not yet started when a job fails are skipped.
	// The caller also takes indices, so a job may call Parallel_For again without deadlocking the pool.
	HRESULT Parallel_For(uint32 count, const std::function<HRESULT(uint32)>& job);

private:
	void Run_Worker(uint32 workerIndex);
	Bool Push_Task(std::function<void()> task);

private:
	vector<std::thread> m_Workers;
	std::mutex m_QueueMutex;
	std::condition_variable m_QueueCondition;
	std::deque<std::function<void()>> m_Queue;
	Bool m_Stopping = { false };

public:
	// workerCount 0 uses hardware_concurrency - 1 (at least 1); the calling thread makes up the last core.
	static Unique<JobManager> Create(uint32 workerCount = 0);
};

NS_END
