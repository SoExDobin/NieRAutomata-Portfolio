#pragma once
#include "StateEm0010.h"

NS_BEGIN(Client)

// Keeps a distance band around the target and strafes left/right while facing it (AnimationTable MOVE_LEFT/RIGHT).
class CLIENT_DLL StateEm0010_Await final : public StateEm0010
{
public:
	explicit StateEm0010_Await(const wstring& tag, const Shared<Em0010>& owner);
	~StateEm0010_Await() override = default;

private:
	HRESULT Initialize() override;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	void StateExitInvoke() override;

private:
	void Start_Strafe(Bool toRight);
	void Start_BackStep();
	void Update_Stuck(Float timeDelta);
	void On_StrafeTimeout();

private:
	Bool m_StrafeRight{ false };
	Bool m_HasFlipped{ false };
	Bool m_IsBackStepping{ false };
	Float m_StrafeDuration{};
	Float m_StrafeElapsed{};
	Float m_BackStepCooldown{};
	Float m_StuckElapsed{};
	Vector3 m_LastPosition{};

	const Float n_StuckTime{ 0.3f };
	const Float n_StuckDistance{ 0.05f };
	const Float n_BackStepInterval{ 1.5f };
	const Float n_MeleeStrikeDelay{ 0.5f };

public:
	static Shared<StateEm0010_Await> Create(const wstring& tag, const Shared<Em0010>& owner);
};

NS_END
