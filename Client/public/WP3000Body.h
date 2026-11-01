#pragma once
#include "Pl0000Parts.h"

NS_BEGIN(Engine)
	class Light;
NS_END

NS_BEGIN(Client)

class Pl0000Body;
class FireFlashEffect;

class CLIENT_DLL WP3000Body final : public Pl0000Parts
{
public:
	enum class POD_STATE
	{
		IDLE			= 0,
		// wp3000_001f, paired with pl0000_001f (Pl0000 POD_GRAB).
		GLIDE			= 9,
		// Pod_Animation.xlsx: 0020 begins, 0021 is attacking, 0022 ends the attack.
		SHOOT_START		= 10,
		SHOOT_LOOP		= 11,
		SHOOT_END		= 12,
		// 0040 splits the body into two halves, 0041 holds it open while the beam fires once, 0042 closes it.
		LASER_START		= 19,
		LASER_LOOP		= 20,
		LASER_END		= 21,
	};

public:
	explicit WP3000Body();
	explicit WP3000Body(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit WP3000Body(const WP3000Body& rhs);
	~WP3000Body() override = default;

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	void On_Destroy() override;
	void On_Disable() override;

public:
	void Priority_Update(Float timeDelta) override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	void Fixed_Update(Float fixedDelta) override;
	HRESULT Render() override;
	void Submit_RenderGroup() override;

public:
	void Set_Pl0000Body(const Shared<Pl0000Body>& body);
	void Set_FireFlash(const Shared<FireFlashEffect>& flash);
	// While gliding the pod plays the clip paired with the player's and hangs above the player's hand instead of shooting.
	void Begin_Glide();
	void End_Glide();
	// The laser state opens the pod's head and closes it again; the beam may fire only while the head is open.
	void Begin_Laser();
	void End_Laser();
	Bool Is_LaserReady() const { return m_PodState == POD_STATE::LASER_LOOP; }
	// Bullets and the laser leave from here, in front of the muzzle sigil.
	const Vector3& Get_MuzzlePosition() const { return m_MuzzlePosition; }
	// The antenna on the pod's top (wp3000 bone5); the communication window's light comes out of it.
	const Vector3& Get_AntennaPosition() const { return m_AntennaPosition; }
	// From the muzzle to the target the owner locked with G, otherwise the bullets' line (the camera's look, raised a little).
	Vector3 Get_AimDirection() const;

private:
	HRESULT Bind_ShaderResources();
	HRESULT Ready_Components();

private:
	void Pod_Fire(Float timeDelta);
	// The pod's own lamp: a point light just below it, brighter the darker the active sun and ambient (the manhole).
	void Update_Lamp();

private:
	Float m_FireRateTimer = 0.f;
	Float m_FireRate = 0.075f;
	// Shift toggles the pod's shooting on and off instead of being held.
	Bool m_ShootToggled = false;

private:
	int32 m_RootBoneIndex = -1;
	Float m_HoverTime = 0.f;
	Float m_HoverAmplitude = 0.2f;   // 상하 진폭
	Float m_HoverSpeed = 2.0f;       // 상하 이동 속도
	Float m_FollowSpeed = 5.0f;      // 플레이어를 따라가는 보간 속도
	Vector3 m_TargetOffset = Vector3(0.8f, 1.5f, 0.f); // 옵셋 (우측, 위, 뒤)
	POD_STATE m_PodState = POD_STATE::IDLE;
	// The player's hand that holds the pod while gliding, and how far above it the pod root sits.
	int32 m_GripBoneIndex = -1;
	Float m_GripHeight = 0.f;
	// 0 at the shoulder spot, 1 on the hand; moves over m_GripBlendTime so grabbing and letting go travel instead of snapping.
	Float m_GripBlend = 0.f;
	Float m_GripBlendTime = 0.3f;

	Weak<Pl0000Body> m_Pl0000Body{};
	Weak<FireFlashEffect> m_FireFlash{};
	Vector3 m_MuzzlePosition{};
	int32 m_AntennaBoneIndex = -1;
	Vector3 m_AntennaPosition{};
	Shared<Light> m_Lamp{ nullptr };

public:
	static Shared<WP3000Body> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;
};

NS_END
