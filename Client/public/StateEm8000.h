#pragma once
#include "State.h"
#include "Em8000.h"

NS_BEGIN(Client)

class Em8000Body;
class EmMovement;

class CLIENT_DLL StateEm8000 abstract : public State
{
public:
	using STATE = Em8000::EM8000_STATE;
	using ANIM = Em8000::EM8000_ANIM;
	using ATTACK = Em8000::ATTACK;

public:
	explicit StateEm8000(const wstring& tag, const Shared<Em8000>& owner);
	virtual ~StateEm8000() override;

protected:
	virtual HRESULT Initialize();

protected:
	Bool Has_Target() const;
	Float Get_DistanceToTarget() const;
	Vector3 Get_DirectionToTarget() const;
	// The angle in degrees between where the boss faces and the player.
	Float Get_AngleToTarget() const;
	void Change_State(STATE state) const;
	void Play(ANIM anim, Bool loop, Float blend = 0.2f) const;
	Bool Is_Playing(ANIM anim) const;
	Bool Is_Finished(ANIM anim) const;
	// Movement for the current step: stand still (optionally turning to the player), or follow the clip's root motion.
	void Hold_Still() const;
	void Hold_FacingTarget() const;
	void Follow_RootMotion(Bool steerToTarget) const;
	// Some clips turn the body round in their root rotation, which stays in the pose (2051, 0555, 0557, 0703, 0705). When
	// one ends the boss takes the way the body faces (body -> eye), so the next clip, which starts unturned from the same
	// pose, follows on without a blend.
	void Turn_ToBody() const;
	// The same for the ball, whose eye does not show the way it faces: the boss turns by the root bone's turn off its bind
	// pose (0547's U-turn, 054D's spin end).
	void Turn_ToRoot() const;

protected:
	Weak<Em8000> m_Owner{};
	Weak<Em8000Body> m_Body{};
	Weak<EmMovement> m_Movement{};
};

NS_END
