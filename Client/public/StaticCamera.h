#pragma once
#include "Camera.h"

NS_BEGIN(Client)

class CLIENT_DLL StaticCamera final : public Camera
{
public:
	typedef struct tagStaticCameraDesc : public CAMERA_DESC
	{
		
	} STATIC_CAMERA_DESC;

public:
	explicit StaticCamera();
	explicit StaticCamera(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit StaticCamera(const StaticCamera& rhs);
	virtual ~StaticCamera() override = default;
public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	void Priority_Update(Float timeDelta) override;
	void Update(Float timeDelta) override;

public:
	static Shared<StaticCamera> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;
};

NS_END
