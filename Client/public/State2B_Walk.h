#pragma once
#include "State2B.h"

NS_BEGIN(Client)

class Pl0000;

class CLIENT_DLL State2B_Walk final : public State2B
{
public:
	explicit State2B_Walk(const wstring& tag, const Shared<Pl0000>& owner);
	~State2B_Walk() override = default;

private:
	HRESULT Initialize();

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	void StateExitInvoke() override;

public:
	static Shared<State2B_Walk> Create(const wstring& tag, const Shared<Pl0000>& owner);

};

NS_END

