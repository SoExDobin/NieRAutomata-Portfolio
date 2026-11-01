#include "EventManager.h"

#include "Game.h"

HRESULT EventManager::Initialize(uint32 levelCount)
{
	m_LevelCount = levelCount;
	m_Events.resize(m_LevelCount);
	return S_OK;
}
void EventManager::Execute_Events()
{
	auto levIndex = GAME_INSTANCE->Get_CurrentLevelIndex();

	if (levIndex >= m_LevelCount || m_Events[levIndex].empty())
		return;

	vector<EVENT_INFO> currentEvents;
	currentEvents.reserve(m_Events[levIndex].size());

	for (auto iter = m_Events[levIndex].begin(); iter != m_Events[levIndex].end(); )
	{
		currentEvents.push_back(iter->second);
		
		if (iter->second.eType == EVENT_TYPE::ONCE)
			iter = m_Events[levIndex].erase(iter);
		else
			++iter;
	}

	for (const auto& ev : currentEvents)
	{
		if (ev.callback)
			ev.callback();
	}
}
HRESULT EventManager::Add_EventOnce(uint32 levIndex, const wstring& eventTag, const std::function<void()>& callback)
{
	if (!callback || levIndex >= m_LevelCount) return E_FAIL;
	auto& targetMap = m_Events[levIndex];
	
	targetMap.emplace(eventTag, EVENT_INFO{ EVENT_TYPE::ONCE, callback });

	return S_OK;
}

HRESULT EventManager::Add_EventPermanent(uint32 levIndex, const wstring& eventTag, const std::function<void()>& callback)
{
	if (!callback || levIndex >= m_LevelCount) return E_FAIL;
	auto& targetMap = m_Events[levIndex];
	
	targetMap.emplace(eventTag, EVENT_INFO{ EVENT_TYPE::PERMANENT, callback });

	return S_OK;
}

HRESULT EventManager::Remove_Event(uint32 levIndex, const wstring& eventTag)
{
	if (levIndex >= m_LevelCount) return E_FAIL;
	auto& targetMap = m_Events[levIndex];
	
	if (targetMap.find(eventTag) == targetMap.end()) return E_FAIL;
	
	targetMap.erase(eventTag);

	return S_OK;
}

HRESULT EventManager::Clear_Events(uint32 levIndex)
{
	if (levIndex >= m_LevelCount) return E_FAIL;
	m_Events[levIndex].clear();
	return S_OK;
}

HRESULT EventManager::Clear_AllEvents()
{
	for (auto& evtMap : m_Events)
	{
		evtMap.clear();
	}
	return S_OK;
}
Unique<EventManager> EventManager::Create(uint32 levelCount)
{
	Unique<EventManager> pInstance = make_unique<EventManager>();
	if (FAILED(pInstance->Initialize(levelCount)))
	{
		pInstance.reset();
		return nullptr;
	}
	return pInstance;
}