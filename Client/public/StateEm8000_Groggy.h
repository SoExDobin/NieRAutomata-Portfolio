#pragma once
#include "StateEm8000.h"

NS_BEGIN(Client)

// Dizzy after every third broken plate (0300 -> 0301 for Em8000::Get_GroggyTime -> 0302): it slumps and stops whatever
// it was doing; its body takes more damage meanwhile (Em8000::TakeDamage).
class CLIENT_DLL StateEm8000_Groggy final : public StateEm8000
{
public:
	explicit StateEm8000_Groggy(const wstring& tag, const Shared<Em8000>& owner);
	~StateEm8000_Groggy() override = default;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void StateExitInvoke() override;

private:
	Float m_LoopTime{};

public:
	static Shared<StateEm8000_Groggy> Create(const wstring& tag, const Shared<Em8000>& owner);
};

NS_END
