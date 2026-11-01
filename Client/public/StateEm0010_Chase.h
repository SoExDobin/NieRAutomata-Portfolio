#pragma once
#include "StateEm0010.h"

NS_BEGIN(Client)

class CLIENT_DLL StateEm0010_Chase : public StateEm0010
{
public:
	explicit StateEm0010_Chase(const wstring& tag, const Shared<Em0010>& owner);
	~StateEm0010_Chase() override = default;

private:
	HRESULT Initialize() override;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	void StateExitInvoke() override;

private:
	unordered_set<uint32> m_EnterAnim;
	Bool m_CanAwait{ true };	// melee coming out of AWAIT runs in to strike instead of strafing again

public:
	static Shared<StateEm0010_Chase> Create(const wstring& tag, const Shared<Em0010>& owner);
};

NS_END