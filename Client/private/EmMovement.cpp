#include "pch.h"
#include "EmMovement.h"
#include "Monster.h"
#include <Game.h>
#include <SpdLogger.h>

EmMovement::EmMovement() : Movement{} {}
EmMovement::EmMovement(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: Movement{ device, context } {
}
EmMovement::EmMovement(const EmMovement& rhs)
	: Movement{ rhs } {
}

HRESULT EmMovement::Initialize_Prototype()
{
	return Movement::Initialize_Prototype();
}

HRESULT EmMovement::Initialize(void* arg)
{
	if (FAILED(Movement::Initialize(arg)))
	{
		LOG_ERROR(L"Failed to Init {}", m_ObjectName);
		return E_FAIL;
	}

	return S_OK;
}

HRESULT EmMovement::Begin()
{
	if (!m_Owner.expired())
	{
		if (!Cast<Monster>(m_Owner.lock()))
		{
			LOG_ERROR(L"Failed To Find Monster Owner");
			return E_FAIL;
		}
	}

	return S_OK;
}


Bool EmMovement::Has_ReachedTarget(Float threshold) const
{
	if (!m_HasWalkTarget) return true;
	if (m_Owner.expired()) return true;

	Vector3 currentPos = m_Owner.lock()->Get_Transform()->Get_Position();
	Vector3 diff = m_TargetPosition - currentPos;
	diff.y = 0.f;

	return diff.Length() <= threshold;
}

void EmMovement::Update_Movement(Float timeDelta)
{
	if (m_Owner.expired()) return;

	Shared<Transform> ownerTransform = m_Owner.lock()->Get_Transform();

	// 나는 클립(followRootHeight)은 높이도 클립을 따르므로 중력과 남은 낙하 속도를 쓰지 않는다.
	const Bool flying = m_CurrentMoveData.followRootHeight;
	if (flying)
	{
		m_Velocity = Vector3::Zero;
	}
	else if (false == m_IsGrounded)
	{
		m_Velocity.y -= m_Gravity * timeDelta;
	}

	Vector3 physicalDelta = m_Velocity * timeDelta; // y이동

	/* 루트 모션은 이번 프레임에 모델이 움직인 거리(모델 공간)라 timeDelta를 다시 곱하지 않는다.
	   수평만 쓰고 수직은 중력이 맡는다(나는 클립은 루트 높이까지). */
	const auto owner = static_pointer_cast<Monster>(m_Owner.lock());
	Vector3 rootDelta = owner->Get_BodyModelTransform().position;
	if (flying)
	{
		physicalDelta.y = rootDelta.y;
	}
	rootDelta.y = 0.f;

	if (m_IsGrounded || flying)
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
			worldMoveDelta = Vector3::Transform(rootDelta, ownerTransform->Get_Quaternion());

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

	// 나는 클립은 충돌 없이 클립 경로 그대로 간다(무대 앞 막는 벽을 넘어 내려온다). 컨트롤러는 다음 이동 때 그 자리로 온다.
	if (flying)
	{
		ownerTransform->Set_Position(nextPosition);
		return;
	}
	Move_Owner(ownerTransform, nextPosition, timeDelta);
}

Shared<EmMovement> EmMovement::Create(const ComPtr<ID3D11Device>& device,
	const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<EmMovement>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : EmMovement");
		return nullptr;
	}

	return prototype;
}

Shared<Component> EmMovement::Clone(void* arg)
{
	auto instance = make_shared<EmMovement>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : EmMovement");
		return nullptr;
	}

	return instance;
}


