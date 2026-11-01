#pragma once
#include "Engine_Define.h"

NS_BEGIN(Engine)

class Timer;

class TimeManager final
{
	NO_COPY(TimeManager)
public:
	explicit TimeManager() = default;
	~TimeManager();

public:
	HRESULT Initialize();
	Float Update_Timers(Bool accumulateFixedTime = true) const;

	Shared<Timer> Get_MainTimer() const;
	Shared<Timer> Get_Timer(const wstring& key) const;
	HRESULT Add_Timer(const wstring& key);
	HRESULT Remove_Timer(const wstring& key);

	Bool Has_FixedUpdate();
	Bool Is_FixedUpdate() const { return m_HasFixedUpdate; }

private:
	Shared<Timer> m_MainTimer = { nullptr };
	unordered_map<wstring, Shared<Timer>> m_Timers;
	Bool m_HasFixedUpdate = { false };

public:
	static Unique<TimeManager> Create();
};

NS_END
