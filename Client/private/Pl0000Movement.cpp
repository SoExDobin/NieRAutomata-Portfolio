#include "pch.h"
#include "Pl0000Movement.h"

#include <Game.h>
#include <SpdLogger.h>
#include "Camera.h"
#include "Pl0000Input.h"
#include "Pl0000.h"

Pl0000Movement::Pl0000Movement() : Movement{} {}
Pl0000Movement::Pl0000Movement(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: Movement{device, context} {}
Pl0000Movement::Pl0000Movement(const Pl0000Movement& rhs) : Movement{rhs} {}

HRESULT Pl0000Movement::Initialize_Prototype()
{
	return Movement::Initialize_Prototype();
}

HRESULT Pl0000Movement::Initialize(void* arg)
{
	if (FAILED(Movement::Initialize(arg)))
	{
		LOG_ERROR(L"Failed to Init {}", m_ObjectName);
		return E_FAIL;
	}

	return S_OK;
}

HRESULT Pl0000Movement::Begin()
{
	if (!m_Owner.expired())
	{
		if (!Cast<Pl0000>(m_Owner.lock()))
		{
			LOG_ERROR(L"Failed To Find Pl0000 Owner");
			return E_FAIL;
		}

		m_Input = m_Owner.lock()->Get_Component<Pl0000Input>();
		if (m_Input.expired())
		{
			LOG_ERROR(L"Failed To Find Pl0000Input");
			return E_FAIL;
		}
	}

	return S_OK;
}

void Pl0000Movement::Update_Movement(Float timeDelta)
{
	if (m_Owner.expired() || m_Input.expired()) return;

	Shared<Transform> ownerTransform = m_Owner.lock()->Get_Transform();

	/* 루트 모션은 이번 프레임에 모델이 움직인 거리(모델 공간)라 timeDelta를 다시 곱하지 않는다.
	   수평만 쓰고 수직은 중력·점프가 맡는다. 난간 오르기만 클립의 상승을 난간 높이에 맞춰 쓴다. */
	const auto owner = static_pointer_cast<Pl0000>(m_Owner.lock());
	Vector3 rootDelta = owner->Get_BodyModelTransform().position;
	const Bool followsClipRise = m_CurrentMoveData.rootMotionRise > 0.f;

	Vector3 physicsDelta{};
	if (followsClipRise)
	{
		m_Velocity = Vector3::Zero;
		physicsDelta.y = rootDelta.y * m_CurrentMoveData.rootMotionRise;
	}
	else
	{
		physicsDelta = m_Velocity * timeDelta;
		physicsDelta.y = Compute_VerticalDisplacement(timeDelta);
	}
	rootDelta.y = 0.f;

	if (m_IsGrounded)
	{
		// 공중에서 이어 쓰도록 속도로 남긴다. 거리로 남기면 공중 이동이 그 프레임 길이에 따라 달라진다.
		m_LastGroundedRootPositionVelocity = timeDelta > 0.f ? rootDelta / timeDelta : Vector3::Zero;
	}
	else if (!(m_CurrentMoveData.isAttack && m_CurrentMoveData.useRootMotionDir) && !followsClipRise)
	{
		// 공중 공격이 클립 루트 이동을 쓰겠다고 한 경우만 그대로 두고, 나머지는 지상에서 남긴 속도를 이어 쓴다.
		rootDelta = m_CurrentMoveData.isMove ? m_LastGroundedRootPositionVelocity * timeDelta : Vector3::Zero;
	}

	if (m_CurrentMoveData.canRotation)
	{
		// 기본적으로는 이동하는 방향(direction)을 회전 타겟으로 잡습니다.
		Vector3 targetRotDir = m_CurrentMoveData.direction;
		// 만약 명시적인 시선 방향(lookDirection) 정보가 들어왔다면 로직을 덮어씌웁니다.
		if (m_CurrentMoveData.lookDirection.LengthSquared() > 0.f)
		{
			targetRotDir = m_CurrentMoveData.lookDirection;
		}
		// 최종적으로 바라볼 방향 벡터가 유효할 때만 회전시킵니다.
		if (targetRotDir.LengthSquared() > 0.f)
		{
			Float targetYaw = atan2f(targetRotDir.x, targetRotDir.z);
			Quaternion targetQuat = Quaternion::CreateFromYawPitchRoll(targetYaw, 0.f, 0.f);
			Float turnSpeed = m_TurnSpeed * timeDelta;
			Quaternion currentQuat = ownerTransform->Get_Quaternion();
			Quaternion nextQuat = Quaternion::Slerp(currentQuat, targetQuat, turnSpeed);
			ownerTransform->Set_Rotation(nextQuat);
		}
	}
	
	Vector3 worldMoveDelta{};
	if (m_CurrentMoveData.useRootMotionDir)
	{
		worldMoveDelta = Vector3::Transform(rootDelta, ownerTransform->Get_Quaternion());
	}
	else
	{
		worldMoveDelta = m_CurrentMoveData.direction * rootDelta.Length();
	}

	worldMoveDelta *= m_RootMotionScale;

	Vector3 nextPosition = ownerTransform->Get_Position() + worldMoveDelta + physicsDelta;
	nextPosition += m_CorrectionDelta;
	Reset_Correction();

	Move_Owner(ownerTransform, nextPosition, timeDelta);
}

Shared<Pl0000Movement> Pl0000Movement::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<Pl0000Movement>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Pl0000Movement");
		return nullptr;
	}

	return prototype;
}

Shared<Component> Pl0000Movement::Clone(void* arg)
{
	auto instance = make_shared<Pl0000Movement>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : Pl0000Movement");
		return nullptr;
	}

	return instance;
}

