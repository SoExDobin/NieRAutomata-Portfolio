#include "pch.h"
#include "StateEm0010_Walk.h"

#include "Em0010.h"
#include "Em0010Movement.h"
#include "MonsterStateMachine.h"
#include "Random_Helper.h"

StateEm0010_Walk::StateEm0010_Walk(const wstring& tag, const Shared<Em0010>& owner)
	: StateEm0010{ tag, owner } {}

HRESULT StateEm0010_Walk::Initialize()
{
	return StateEm0010::Initialize();
}

Bool StateEm0010_Walk::StateEnterInvoke()
{
	auto movement = m_Movement.lock();
	auto owner = m_Owner.lock();
	auto body = m_Body.lock();
	if (!movement || !owner || !body) return false;

	if (false == movement->Has_WalkTarget())
	{
		return false;
	}

	m_WalkDuration = static_cast<Float>(Helper::Random_Double(3.0, 7.0));
	m_WalkElapsed = 0.f;

	Vector3 currentPos = owner->Get_Transform()->Get_Position();
	Vector3 targetPos = movement->Get_TargetPosition();
	Vector3 direction = targetPos - currentPos;
	direction.y = 0.f;

	if (direction.LengthSquared() > 0.f)
	{
		direction.Normalize();
	}

	Em0010Movement::EM0010_MOVEMENT_DATA moveData{};
	moveData.direction = direction;
	moveData.isMove = true;
	moveData.canRotation = true;
	moveData.useRootMotionDir = false;
	moveData.rootMotionScale = 1.f;
	movement->Set_MovementData(moveData);

	body->Set_Animation(ETOI(Em0010::EM0010_STATE::WALK_F), 0.2f, true);

	return true;
}

void StateEm0010_Walk::Update(Float timeDelta)
{
	if (Has_Target())
	{
		m_States.lock()->Change_State(MonsterStateMachine::MONSTER_STATE::CHASE);
		return;
	}

	auto em0010 = m_Owner.lock();
    auto em0010Body = m_Body.lock();
    auto movement = m_Movement.lock();

	m_WalkElapsed += timeDelta;
	if (m_WalkElapsed >= m_WalkDuration)
	{
		m_States.lock()->Change_State(MonsterStateMachine::MONSTER_STATE::IDLE);
		return;
	}

	if (movement->Has_ReachedTarget(1.f))
	{
		m_States.lock()->Change_State(MonsterStateMachine::MONSTER_STATE::IDLE);
		return;
	}

	Vector3 currentPos = em0010->Get_Transform()->Get_Position();
	Vector3 targetPos = movement->Get_TargetPosition();
	Vector3 direction = targetPos - currentPos;
	direction.y = 0.f;

	if (direction.LengthSquared() > 0.001f)
	{
		direction.Normalize();
	}

	Em0010Movement::EM0010_MOVEMENT_DATA moveData{};
	moveData.direction = direction;
	moveData.isMove = true;
	moveData.canRotation = true;
	moveData.useRootMotionDir = false;
	moveData.rootMotionScale = 1.f;
	movement->Set_MovementData(moveData);
}

void StateEm0010_Walk::Late_Update(Float timeDelta)
{
	
}

void StateEm0010_Walk::StateExitInvoke()
{
	if (auto movement = m_Movement.lock())
	{
		Em0010Movement::EM0010_MOVEMENT_DATA stopData{};
		stopData.isMove = false;
		stopData.canRotation = false;
		movement->Set_MovementData(stopData);
		movement->Clear_TargetPosition();
	}
}

Shared<StateEm0010_Walk> StateEm0010_Walk::Create(const wstring& tag, const Shared<Em0010>& owner)
{
	auto instance = make_shared<StateEm0010_Walk>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : StateEm0010_Walk");
		return nullptr;
	}

	return instance;
}
