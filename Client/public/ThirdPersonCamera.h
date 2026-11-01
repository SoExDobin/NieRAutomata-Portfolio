#pragma once
#include "Camera.h"

NS_BEGIN(Client)

class CLIENT_DLL ThirdPersonCamera final : public Camera
{
public:
	typedef struct tagThirdPersonCameraDesc : public CAMERA_DESC
	{
		Float distance{};		// 타겟으로부터의 기본 거리
		Float minDistance{};	// 줌 인 제한 거리
		Float maxDistance{};	// 줌 아웃 제한 거리
		Vector3 offset{};		// 타겟 위치에서의 오프셋 (예: 캐릭터의 머리 높이)
		Float mouseSensitive{};
		Float wheelSensitive{};
	} THIRD_PERSON_CAMERA_DESC;

public:
	explicit ThirdPersonCamera();
	explicit ThirdPersonCamera(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit ThirdPersonCamera(const ThirdPersonCamera& rhs);
	~ThirdPersonCamera() override = default;

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	void On_Destroy() override;

public:
	void Priority_Update(Float timeDelta) override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	void Fixed_Update(Float fixedDelta) override;

public:
	// A cutscene (the opera boss's entrance) places the camera itself while it holds it. On release the orbit carries on
	// from the angle the cutscene left, so the camera eases back behind the target instead of cutting.
	void Hold(Bool hold);
	Bool Is_Held() const { return m_IsHeld; }

protected:
	Float		m_Distance{};			// 현재 카메라와 타겟 사이의 거리
	Float		m_TargetDistance{};		// 목표로 하는 카메라 거리 (줌 입력 시 변함)
	Float		m_MinDistance{};		// 줌 인 최소 제한 거리
	Float		m_MaxDistance{};		// 줌 아웃 최대 제한 거리
	Vector3		m_Offset{};				// 타겟 좌표에 더해질 오프셋 좌표

	Float		m_OrbitX{};
	Float		m_OrbitY{};
	Float		m_MouseSensitive{};
	Float		m_WheelSensitive{};

	Float		m_Friction{};
	Float		m_OrbitVelocityX{};
	Float		m_OrbitVelocityY{};

	Bool		m_IsHeld{ false };

public:
	static Shared<ThirdPersonCamera> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg = nullptr) override;
	
};

NS_END
