#pragma once
#include "EngineManager.h"

NS_BEGIN(Engine)

class EventManager final : public EngineManager
{
	NO_COPY(EventManager)
private:
	struct EVENT_INFO
	{
		EVENT_TYPE eType;
		std::function<void()> callback;
	};

public:
	explicit EventManager() = default;
	~EventManager() override = default;

public:
	HRESULT Initialize(uint32 levelCount);

	void Execute_Events();

	HRESULT Add_EventOnce(uint32 levIndex, const wstring& eventTag, const std::function<void()>& callback);
	HRESULT Add_EventPermanent(uint32 levIndex, const wstring& eventTag, const std::function<void()>& callback);
	HRESULT Remove_Event(uint32 levIndex, const wstring& eventTag);

	HRESULT Clear_Events(uint32 levIndex);
	HRESULT Clear_AllEvents();
private:
	uint32 m_LevelCount{};
	vector<unordered_multimap<wstring, EVENT_INFO>> m_Events;

public:
	static Unique<EventManager> Create(uint32 levelCount);
};

NS_END