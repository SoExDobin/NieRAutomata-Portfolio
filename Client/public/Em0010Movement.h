#pragma once
#include "Movement.h"

NS_BEGIN(Engine)
class Transform;
NS_END

NS_BEGIN(Client)

class Em0010;

// Root-motion movement of the small machines (Em0010, Em0000): the owner Monster hands it the body's root motion.
class CLIENT_DLL Em0010Movement final : public Movement
{
public:
	typedef struct tagEm0010MovementDesc : public MOVEMENT_DESC
	{

	} EM0010_MOVEMENT_DESC;

public:
	typedef struct tagEm0010MovementData
	{
		Vector3 direction{ 0.f, 0.f, 0.f };
		Vector3 lookDirection{ 0.f, 0.f, 0.f };

		Bool isMove{ false };
		Bool isAttack{ false };
		Bool canRotation{ true };
		Bool useRootMotionDir{ true };		// 로컬 방향에 의한 tilt 수정
		Float rootMotionScale{ 1.f };
	} EM0010_MOVEMENT_DATA;

public:
	void Set_MovementData(const EM0010_MOVEMENT_DATA& data) { m_CurrentMoveData = data; Reset_RootMotionStop(); }
	const EM0010_MOVEMENT_DATA& Get_MovementData() const { return m_CurrentMoveData; }

public:
	Vector3 Get_RootPosition() const { return m_RootPosition; }
	void Set_RootPosition(const Vector3& rootPos) { m_RootPosition = rootPos; }

public:
	void Set_TargetPosition(const Vector3& targetPos) {
		m_TargetPosition = targetPos; m_HasWalkTarget = true;}
	Vector3 Get_TargetPosition() const { return m_TargetPosition; }
	void Clear_TargetPosition() { m_HasWalkTarget = false; }
	Bool Has_WalkTarget() const { return m_HasWalkTarget; }
	Bool Has_ReachedTarget(Float threshold = 1.0f) const;

public:
	explicit Em0010Movement();
	explicit Em0010Movement(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Em0010Movement(const Em0010Movement& rhs);
	~Em0010Movement() override = default;

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg = nullptr) override;
	HRESULT Begin() override;

public:
	void Update_Movement(Float timeDelta) override;

private:
	EM0010_MOVEMENT_DATA m_CurrentMoveData{};
	Vector3 m_LastGroundedRootPositionVelocity{ 0.f, 0.f, 0.f };

private:
	Vector3 m_RootPosition{};
	Vector3 m_TargetPosition{};
	Bool m_HasWalkTarget{ false };

public:
	static Shared<Em0010Movement> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<Component> Clone(void* arg = nullptr) override;
};

NS_END
