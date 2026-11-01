#pragma once
#include "State2B.h"
#include "Pl0000.h"

NS_BEGIN(Client)

class CLIENT_DLL State2B_Idle final : public State2B
{
public:
	explicit State2B_Idle(const wstring& tag, const Shared<Pl0000>& owner);
	~State2B_Idle() override = default;

private:
	HRESULT Initialize() override;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	void StateExitInvoke() override;

private:
	unordered_set<uint32> m_EnterAnim;
	Bool m_InitializeState{ true };

public:
	static Shared<State2B_Idle> Create(const wstring& tag, const Shared<Pl0000>& owner);
};

NS_END