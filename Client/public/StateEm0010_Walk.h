#pragma once
#include "StateEm0010.h"

NS_BEGIN(Client)

class CLIENT_DLL StateEm0010_Walk final : public StateEm0010
{
public:
	explicit StateEm0010_Walk(const wstring& tag, const Shared<Em0010>& owner);
	~StateEm0010_Walk() override = default;

private:
	HRESULT Initialize() override;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	void StateExitInvoke() override;

private:
	Bool m_InitializeState{ true };
	unordered_set<uint32> m_EnterAnim;

	Float m_WalkDuration{};
	Float m_WalkElapsed{};

public:
	static Shared<StateEm0010_Walk> Create(const wstring& tag, const Shared<Em0010>& owner);

};

NS_END