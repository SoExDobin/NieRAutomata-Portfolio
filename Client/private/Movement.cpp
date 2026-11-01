#include "pch.h"
#include "Movement.h"

#include <Game.h>

namespace
{
	// 캐릭터 캡슐. 전체 높이 = 원통 높이 + 반지름 * 2 = 1.7m. 조작감에 맞춰 조정하는 값이다.
	constexpr Float CapsuleRadius = 0.35f;
	constexpr Float CapsuleHeight = 1.0f;
	// 컨트롤러가 걸어 올라가는 턱 높이이자, 접지 중 아래로 눌러 두는 거리.
	constexpr Float StepAndGroundSnap = 0.4f;
	// 스폰할 때 발밑 바닥을 찾는 깊이. 놀이공원 돔 안처럼 수십 m 높게 배치된 경우까지 덮는다.
	constexpr Float SpawnGroundProbeDepth = 200.f;
}

Movement::Movement() : ScriptComponent{} {}
Movement::Movement(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: ScriptComponent{ device, context } {}
Movement::Movement(const Movement& rhs)
	: ScriptComponent{rhs} {}

HRESULT Movement::Initialize_Prototype()
{
	return ScriptComponent::Initialize_Prototype();
}

HRESULT Movement::Initialize(void* arg)
{
	if (nullptr != arg)
	{
		MOVEMENT_DESC& desc = *static_cast<MOVEMENT_DESC*>(arg);
		m_MoveSpeed = desc.moveSpeed;
		m_TurnSpeed = desc.turnSpeed;
		m_TargetDirection = desc.targetDirection;
		m_Velocity = desc.velocity;
		m_Gravity = desc.gravity;
	}

	return ScriptComponent::Initialize(arg);
}

HRESULT Movement::Begin()
{
	return ScriptComponent::Begin();
}

void Movement::On_Destroy()
{
	Release_Controller();
	ScriptComponent::On_Destroy();
}

void Movement::On_Disable()
{
	Release_Controller();
	ScriptComponent::On_Disable();
}

void Movement::Release_Controller()
{
	if (0 != m_ControllerHandle)
	{
		GAME_INSTANCE->Release_CharacterController(m_ControllerHandle);
		m_ControllerHandle = 0;
	}
}

void Movement::Begin_EaseOutAscent(Float height, Float duration)
{
	if (height <= 0.f || duration <= 0.f) return;
	m_Velocity.y = 0.f;
	m_IsGrounded = false;
	m_AscentHeight = height;
	m_AscentDuration = duration;
	m_AscentElapsed = 0.f;
}

Float Movement::Compute_VerticalDisplacement(Float timeDelta)
{
	if (m_AscentDuration > 0.f)
	{
		const Float previous = m_AscentElapsed / m_AscentDuration;
		m_AscentElapsed = min(m_AscentElapsed + timeDelta, m_AscentDuration);
		const Float current = m_AscentElapsed / m_AscentDuration;
		const Float previousEase = 1.f - (1.f - previous) * (1.f - previous);
		const Float currentEase = 1.f - (1.f - current) * (1.f - current);
		if (m_AscentElapsed >= m_AscentDuration) m_AscentDuration = 0.f;
		return m_AscentHeight * (currentEase - previousEase);
	}

	if (!m_IsGrounded) m_Velocity.y -= m_Gravity * timeDelta;
	return m_Velocity.y * timeDelta;
}

void Movement::Move_Owner(const Shared<Transform>& transform, const Vector3& nextPosition, Float timeDelta)
{
	if (Ensure_Controller(transform))
	{
		Move_WithController(transform, nextPosition - transform->Get_Position(), timeDelta);
		return;
	}

	// 정적 충돌이 없는 레벨. 접지와 벽은 그 레벨에 COL이 올라와야 생긴다.
	transform->Set_Position(nextPosition);
}

Bool Movement::Ensure_Controller(const Shared<Transform>& transform)
{
	if (0 != m_ControllerHandle)
		return true;

	// 설 곳이 없는 레벨에서 컨트롤러를 만들면 끝없이 떨어지기만 한다.
	if (m_ControllerUnavailable || !GAME_INSTANCE->Has_StaticCollision())
		return false;

	CHARACTER_CONTROLLER_DESC desc{};
	desc.radius = CapsuleRadius;
	desc.height = CapsuleHeight;
	desc.stepOffset = StepAndGroundSnap;

	// 스폰 높이는 배치한 값에 맡기지 않고 바로 아래 바닥에 발을 붙인다. 캡슐 높이만큼 위에서 쏴서
	// 바닥에 묻힌 스폰도 꺼낸다. 아래에 바닥이 없으면 배치한 자리에서 중력으로 떨어진다.
	Vector3 foot = transform->Get_Position();
	const Float probeLift = CapsuleHeight + CapsuleRadius * 2.f;
	Vector3 ground{};
	if (GAME_INSTANCE->Raycast_Physics(foot + Vector3{ 0.f, probeLift, 0.f }, -Vector3::UnitY, probeLift + SpawnGroundProbeDepth, ground))
	{
		foot.y = ground.y;
		transform->Set_Position(foot);
	}

	// 첫 업데이트에서 만든다. Begin()은 Ready_Components 안에서 불려서 그때는 authored 위치가 아직 없다.
	m_ControllerHandle = GAME_INSTANCE->Create_CharacterController(desc, foot);
	if (0 == m_ControllerHandle)
	{
		m_ControllerUnavailable = true;   // 같은 실패를 매 프레임 반복하지 않는다.
		return false;
	}

	m_LastFootPosition = foot;
	return true;
}

void Movement::Move_WithController(const Shared<Transform>& transform, Vector3 displacement, Float timeDelta)
{
	// 이동 로직 밖에서 옮겨졌다면(에디터 기즈모, 리스폰) 그 자리로 보낸다. 쓸어서 가면 중간 벽에 걸린다.
	const Vector3 current = transform->Get_Position();
	if (Vector3::DistanceSquared(current, m_LastFootPosition) > 1e-6f)
		GAME_INSTANCE->Set_ControllerFootPosition(m_ControllerHandle, current);

	// 접지 중에는 중력이 적분되지 않으므로 아래로 눌러 둔다. 없으면 접지가 한 프레임씩
	// 풀렸다 잡혔다 하고, 내리막에서는 허공으로 걸어 나간다.
	if (m_IsGrounded && displacement.y <= 0.f)
		displacement.y -= StepAndGroundSnap;

	const uint32 collision = GAME_INSTANCE->Move_CharacterController(m_ControllerHandle, displacement, timeDelta);

	const Vector3 foot = GAME_INSTANCE->Get_ControllerFootPosition(m_ControllerHandle);
	transform->Set_Position(foot);
	m_LastFootPosition = foot;

	// 상승 시작 프레임에는 바닥 접촉이 DOWN으로 함께 보고될 수 있다.
	// 실제 하강 이동에서만 착지로 처리해야 상승 곡선이 중간에 끊기지 않는다.
	if ((collision & CONTROLLER_COLLISION_DOWN) && displacement.y <= 0.f)
	{
		m_Velocity = Vector3::Zero;
		m_AscentDuration = 0.f;
		m_IsGrounded = true;
	}
	else
	{
		m_IsGrounded = false;
	}

	// 상승 중 천장에 닿으면 점프를 끊는다.
	if ((collision & CONTROLLER_COLLISION_UP) && (m_Velocity.y > 0.f || m_AscentDuration > 0.f))
	{
		m_Velocity.y = 0.f;
		m_AscentDuration = 0.f;
	}
}

