#include "pch.h"
#include "StaticCamera.h"
#include "ClientSettingManager.h"

StaticCamera::StaticCamera() : Camera{} {}
StaticCamera::StaticCamera(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: Camera{device, context} {}
StaticCamera::StaticCamera(const StaticCamera& rhs)
	: Camera{rhs} {}

HRESULT StaticCamera::Initialize_Prototype()
{
	return Camera::Initialize_Prototype();
}

HRESULT StaticCamera::Initialize(void* arg)
{
	STATIC_CAMERA_DESC desc{};
	desc.eye = Vector4{ 0.f, 5.f, -10.f, 1.f };
	desc.at = Vector4{ 0.f, 0.f, 0.f, 1.f };
	desc.up = Vector4{ 0.f, 1.f, 0.f, 0.f };
	desc.fovY = XMConvertToRadians(60.0f);
	desc.aspect = static_cast<Float>(ClientSettingManager::g_EngineDesc.viewportWidth / ClientSettingManager::g_EngineDesc.viewportHeight);
	desc.nearPlane = 0.1f;
	desc.farPlane = 1000.f;

	return Camera::Initialize(&desc);
}

void StaticCamera::Priority_Update(Float timeDelta)
{
	Update_CameraTransform(timeDelta);
}

void StaticCamera::Update(Float timeDelta)
{
	Camera::Update(timeDelta);
}

Shared<StaticCamera> StaticCamera::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<StaticCamera>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : StaticCamera");
		return nullptr;
	}

	return prototype;
}

Shared<GameObject> StaticCamera::Clone(void* arg)
{
	auto instance = make_shared<StaticCamera>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : StaticCamera");
		return nullptr;
	}

	return instance;
}
