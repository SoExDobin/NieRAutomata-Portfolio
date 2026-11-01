#pragma once
#include "State2B.h"

NS_BEGIN(Client)

class Pl0000;

class CLIENT_DLL State2B_Sprint final : public State2B
{
public:
	explicit State2B_Sprint(const wstring& tag, const Shared<Pl0000>& owner);
	~State2B_Sprint() override = default;

private:
	HRESULT Initialize() override;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	void StateExitInvoke() override;

private:
	unordered_set<uint32> m_EnterAnim;
	unordered_set<uint32> m_ToSprint;
	Vector3 m_LockedDirection{};

public:
	static Shared<State2B_Sprint> Create(const wstring& tag, const Shared<Pl0000>& owner);
};

NS_END

