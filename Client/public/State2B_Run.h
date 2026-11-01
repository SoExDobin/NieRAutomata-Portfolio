#pragma once
#include "State2B.h"

NS_BEGIN(Client)

class CLIENT_DLL State2B_Run final : public State2B
{
public:
	explicit State2B_Run(const wstring& tag, const Shared<Pl0000>& owner);
	~State2B_Run() override = default;

private:
	HRESULT Initialize();

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	void StateExitInvoke() override;

private:
	unordered_set<uint32> m_EnterAnim;

public:
	static Shared<State2B_Run> Create(const wstring& tag, const Shared<Pl0000>& owner);
};

NS_END

