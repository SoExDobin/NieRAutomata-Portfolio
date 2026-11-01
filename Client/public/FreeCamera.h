#pragma once
#include "Camera.h"

NS_BEGIN(Client)
	class CLIENT_DLL FreeCamera final : public Camera
{
public:
	typedef struct tagFreeCameraDesc : public tagCameraDesc
	{
		Float mouseSensitive = {};
	} FREE_CAMERA_DESC;

public:
	FreeCamera();
	FreeCamera(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	FreeCamera(const FreeCamera& rhs);
	~FreeCamera() override = default;

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	void On_Destroy() override { Camera::On_Destroy(); }
	void On_Enable() override { Camera::On_Enable(); }
	void On_Disable() override { Camera::On_Disable(); }

public:
	void Priority_Update(Float timeDelta) override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	void Fixed_Update(Float fixedDelta) override;
	HRESULT Render() override;

private:
	Float m_MouseSensitive{};

public:
	static Shared<FreeCamera> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg = nullptr) override;
};

NS_END
