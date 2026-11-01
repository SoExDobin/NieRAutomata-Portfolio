#pragma once
#include "Movement.h"

NS_BEGIN(Engine)
class Camera;
class Transform;
NS_END

NS_BEGIN(Client)

class Pl0000;
class Pl0000Input;

class CLIENT_DLL Pl0000Movement final : public Movement
{
public:
	typedef struct tagPl0000MovementDesc : public MOVEMENT_DESC
	{

	} PL0000_MOVEMENT_DESC;

public:
	typedef struct tagPl0000MovementData
	{
		Vector3 direction{0.f, 0.f, 0.f };
		Vector3 lookDirection{ 0.f, 0.f, 0.f };
		
		Bool isMove{ false };
		Bool isJump{ false };
		Bool isAttack{ false };

		Bool canRotation{ true };
		Bool useRootMotionDir{ true };		// 로컬 방향에 의한 tilt 수정
		// 0보다 크면 공중에서도 클립 루트 이동을 그대로 따르고, 클립의 상승에 이 배율을 곱해 쓰며 그동안 중력은 멈춘다(난간 오르기).
		Float rootMotionRise{ 0.f };
	} PL0000_MOVEMENT_DATA;

public:
	void Set_MovementData(const PL0000_MOVEMENT_DATA& data) { m_CurrentMoveData = data; Reset_RootMotionStop(); }
	const PL0000_MOVEMENT_DATA& Get_MovementData() const { return m_CurrentMoveData; }

public:
	explicit Pl0000Movement();
	explicit Pl0000Movement(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Pl0000Movement(const Pl0000Movement& rhs);
	~Pl0000Movement() override = default;
	
public:
	void Reduce_RootMotion(Float lazyAmount = 0.1f) override { m_RootMotionScale = lazyAmount; }
	void Reset_RootMotionStop() override { m_RootMotionScale = 1.0f; }

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg = nullptr) override;
	HRESULT Begin() override;

public:
	void Update_Movement(Float timeDelta) override;

private:
	PL0000_MOVEMENT_DATA m_CurrentMoveData{};
	Vector3 m_LastGroundedRootPositionVelocity{ 0.f, 0.f, 0.f };

private:
	Weak<Pl0000Input> m_Input{};

public:
	static Shared<Pl0000Movement> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<Component> Clone(void* arg = nullptr) override;
};

NS_END
