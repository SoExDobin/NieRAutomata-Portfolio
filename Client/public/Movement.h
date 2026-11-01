#pragma once
#include "ScriptComponent.h"

NS_BEGIN(Engine)
class Transform;
NS_END

NS_BEGIN(Client)

class CLIENT_DLL Movement abstract : public ScriptComponent
{
public:
	typedef struct tagMovement : public COMPONENT_DESC
	{
		Vector3			velocity{ 0.f, 0.f, 0.f };
		Vector3			targetDirection{ 0.f, 0.f, 0.f };
		Float			moveSpeed{ 0.f };
		Float			turnSpeed{ 0.f };
		Float			gravity{ 30.f };
	} MOVEMENT_DESC;

public:
	explicit Movement();
	explicit Movement(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Movement(const Movement& rhs);
	virtual ~Movement() override = default;

public:
	Float Get_Gravity() const { return m_Gravity; }
	void Set_Gravity(Float gravity) { m_Gravity = gravity; }

public:
	Bool Is_Grounded() const { return m_IsGrounded; }
	Float Get_GravityScalar() const { return m_Gravity; }
	void Add_Force(const Vector3& impulse) { m_Velocity += impulse; m_IsGrounded = false; }
	void Reset_VerticalVelocity() { m_Velocity.y = 0.f; m_AscentDuration = 0.f; }
	void Begin_EaseOutAscent(Float height, Float duration);
	Bool Is_Ascending() const { return m_AscentDuration > 0.f; }
	Float Get_VerticalVelocity() const { return m_Velocity.y; }
	void Add_Correction(const Vector3& correction) { m_CorrectionDelta += correction; }
	void Reset_Correction() { m_CorrectionDelta = Vector3::Zero; }

	virtual void Reduce_RootMotion(Float lazyAmount = 0.1f) { m_RootMotionScale = lazyAmount; }
	virtual void Reset_RootMotionStop() { m_RootMotionScale = 1.0f; }

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg = nullptr) override;
	HRESULT Begin() override;
	void On_Destroy() override;
	// A disabled movement leaves no capsule behind to block others; the next move makes a new one on the floor below.
	void On_Disable() override;

public:
	virtual void Update_Movement(Float timeDelta) {};

protected:
	/// 정적 충돌이 있으면 캡슐 컨트롤러로 옮겨 접지와 벽을 처리하고, 없으면 그대로 옮긴다.
	void Move_Owner(const Shared<Transform>& transform, const Vector3& nextPosition, Float timeDelta);
	Float Compute_VerticalDisplacement(Float timeDelta);

private:
	/// 정적 충돌이 있으면 첫 호출에 캡슐 컨트롤러를 만든다. 쓸 수 있으면 true.
	Bool Ensure_Controller(const Shared<Transform>& transform);
	void Move_WithController(const Shared<Transform>& transform, Vector3 displacement, Float timeDelta);
	void Release_Controller();

protected:
	Vector3			m_Velocity{ 0.f, 0.f, 0.f };
	Vector3			m_TargetDirection{ 0.f, 0.f, 0.f };
	Float			m_MoveSpeed{ 0.f };
	Float			m_TurnSpeed{ 0.f };
	Float			m_Gravity{ 30.f };
	Bool			m_IsGrounded{ true };
	Float			m_RootMotionScale{ 1.0f };
	Vector3			m_CorrectionDelta{ 0.f };
	Float			m_AscentHeight{ 0.f };
	Float			m_AscentDuration{ 0.f };
	Float			m_AscentElapsed{ 0.f };

private:
	// PhysX 캐릭터 컨트롤러 핸들. 복제본은 자기 컨트롤러를 따로 만든다.
	uint32			m_ControllerHandle{ 0 };
	Vector3			m_LastFootPosition{ 0.f, 0.f, 0.f };
	Bool			m_ControllerUnavailable{ false };

public:
	Shared<Component> Clone(void* arg = nullptr) PURE;
};

NS_END
