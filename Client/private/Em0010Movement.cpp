#include "pch.h"
#include "Em0010Movement.h"

#include <Game.h>

#include "Em0010.h"
#include <SpdLogger.h>

Em0010Movement::Em0010Movement() : Movement{} {}
Em0010Movement::Em0010Movement(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: Movement{device, context} {}
Em0010Movement::Em0010Movement(const Em0010Movement& rhs)
	: Movement{rhs} {}

HRESULT Em0010Movement::Initialize_Prototype()
{
	return Movement::Initialize_Prototype();
}

HRESULT Em0010Movement::Initialize(void* arg)
{
	if (FAILED(Movement::Initialize(arg)))
	{
		LOG_ERROR(L"Failed to Init {}", m_ObjectName);
		return E_FAIL;
	}

	return S_OK;
}

HRESULT Em0010Movement::Begin()
{
	if (!m_Owner.expired())
	{
		if (!Cast<Monster>(m_Owner.lock()))
		{
			LOG_ERROR(L"Failed To Find Monster Owner");
			return E_FAIL;
		}

		// The owner is attached by now (Initialize runs before Add_Component sets it), so its level is known.
		// One event per owner: a shared tag kept only the first of the monsters loaded or spawned in one frame.
		const Shared<GameObject> owner = m_Owner.lock();
		GAME_INSTANCE->Add_Instance_Event(owner->Get_LevelIndex(),
			L"Set_RootPos_" + std::to_wstring(owner->Get_RuntimeObjectId()), [this]()
			{
				m_RootPosition = m_Owner.lock()->Get_Transform()->Get_Position();
			});
	}

	return S_OK;
}


Bool Em0010Movement::Has_ReachedTarget(Float threshold) const
{
	if (!m_HasWalkTarget) return true;
	if (m_Owner.expired()) return true;

	Vector3 currentPos = m_Owner.lock()->Get_Transform()->Get_Position();
	Vector3 diff = m_TargetPosition - currentPos;
	diff.y = 0.f;

	return diff.Length() <= threshold;
}

void Em0010Movement::Update_Movement(Float timeDelta)
{
	if (m_Owner.expired()) return;

	Shared<Transform> ownerTransform = m_Owner.lock()->Get_Transform();

	Vector3 physicalDelta = m_Velocity * timeDelta; // y이동
	physicalDelta.y = Compute_VerticalDisplacement(timeDelta);

	/* 루트 모션은 이번 프레임에 모델이 움직인 거리(모델 공간)라 timeDelta를 다시 곱하지 않는다.
	   수평만 쓰고 수직은 중력이 맡는다. */
	const auto owner = static_pointer_cast<Monster>(m_Owner.lock());
	// 모델 크기 기준이라 몸을 키운 개체(Emb001)는 그 배율만큼 더 간다.
	Vector3 rootDelta = owner->Get_BodyModelTransform().position * ownerTransform->Get_Scale().x;
	rootDelta.y = 0.f;

	if (m_IsGrounded)
	{
		// 공중에서 이어 쓰도록 속도로 남긴다. 거리로 남기면 공중 이동이 그 프레임 길이에 따라 달라진다.
		m_LastGroundedRootPositionVelocity = timeDelta > 0.f ? rootDelta / timeDelta : Vector3::Zero;
	}
	else
	{
		rootDelta = m_CurrentMoveData.isMove ? m_LastGroundedRootPositionVelocity * timeDelta : Vector3::Zero;
	}

	if (m_CurrentMoveData.canRotation)
	{
		// 기본 이동 방향
		Vector3 targetRotDir = m_CurrentMoveData.direction;

		// 명시된 look이 있으면 덮음
		if (m_CurrentMoveData.lookDirection.LengthSquared() > 0.f)
		{
			targetRotDir = m_CurrentMoveData.lookDirection;
		}

		if (targetRotDir.LengthSquared() > 0.f)
		{
			Float targetYaw = atan2f(targetRotDir.x, targetRotDir.z);
			Quaternion targetQuat = Quaternion::CreateFromYawPitchRoll(targetYaw, 0.f, 0.f);

			Float turnDelta = m_TurnSpeed * timeDelta;
			Quaternion currentQuat = ownerTransform->Get_Quaternion();
			Quaternion nextQuat = Quaternion::Slerp(currentQuat, targetQuat, turnDelta);
			ownerTransform->Set_Rotation(nextQuat);
		}
	}

	Vector3 worldMoveDelta{}; // x이동

	if (m_CurrentMoveData.isMove || m_CurrentMoveData.isAttack)
	{
		if (m_CurrentMoveData.useRootMotionDir)
		{
			worldMoveDelta = Vector3::Transform(rootDelta,ownerTransform->Get_Quaternion());

			worldMoveDelta *= m_CurrentMoveData.rootMotionScale;
		}
		else
		{
			worldMoveDelta = m_CurrentMoveData.direction * rootDelta.Length() * m_CurrentMoveData.rootMotionScale;
		}
	}

	worldMoveDelta *= m_RootMotionScale;

	Vector3 nextPosition = ownerTransform->Get_Position() + worldMoveDelta + physicalDelta;

	nextPosition += m_CorrectionDelta;
	Reset_Correction();

	Move_Owner(ownerTransform, nextPosition, timeDelta);
}

Shared<Em0010Movement> Em0010Movement::Create(const ComPtr<ID3D11Device>& device,
                                              const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<Em0010Movement>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Em0010Movement");
		return nullptr;
	}

	return prototype;
}

Shared<Component> Em0010Movement::Clone(void* arg)
{
	auto instance = make_shared<Em0010Movement>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : Em0010Movement");
		return nullptr;
	}

	return instance;
}

