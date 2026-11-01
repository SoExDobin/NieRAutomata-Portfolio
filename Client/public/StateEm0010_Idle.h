#pragma once
#include "StateEm0010.h"

NS_BEGIN(Client)

class CLIENT_DLL StateEm0010_Idle final : public StateEm0010
{
public:
	explicit StateEm0010_Idle(const wstring& tag, const Shared<Em0010>& owner);
	~StateEm0010_Idle() override = default;

private:
	HRESULT Initialize() override;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	void StateExitInvoke() override;

private:
	void Update_Wander(Float timeDelta);
	// A passive gun machine (park visitor) raises its gun, fires a firework shell into the sky and lowers it.
	void Update_Salute(Float timeDelta);

private:
	Bool m_InitializeState{ true };
	unordered_set<uint32> m_EnterAnim;

	Vector3 m_RootPosition{};
	Float m_WanderTimer{};
	Bool m_Saluting{ false };
	Bool m_SaluteFired{ false };
	Float m_SaluteTime{};		// in the current shoot clip

public:
	static Shared<StateEm0010_Idle> Create(const wstring& tag, const Shared<Em0010>& owner);
};

NS_END