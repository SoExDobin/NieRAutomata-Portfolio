#include "Camera.h"
#include "Game.h"
#include "Transform.h"
#include "SpdLogger.h"

Camera::Camera() : GameObject{} {}
Camera::Camera(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: GameObject{ device, context } {}
Camera::Camera(const Camera& rhs)
	: GameObject{ rhs }, m_FovY{rhs.m_FovY}, m_Aspect{rhs.m_Aspect}, m_Near{rhs.m_Near}, m_Far{rhs.m_Far} {}

HRESULT Camera::Initialize_Prototype()
{
	return GameObject::Initialize_Prototype();
}

HRESULT Camera::Initialize(void* arg)
{
	CAMERA_DESC localDesc{};
	if (nullptr == arg)
	{
		localDesc.eye = Vector4{ 0.f, 5.f, -10.f, 1.f };
		localDesc.at = Vector4{ 0.f, 0.f, 0.f, 1.f };
		localDesc.up = Vector4{ 0.f, 1.f, 0.f, 0.f };
		localDesc.fovY = XMConvertToRadians(60.0f);
		localDesc.aspect = 1.6f;
		localDesc.nearPlane = 0.1f;
		localDesc.farPlane = 1000.f;
		arg = &localDesc;
	}

	if (FAILED(GameObject::Initialize(arg)))
		return E_FAIL;

	m_ObjectDesc = static_pointer_cast<OBJECT_DESC>(make_shared<CAMERA_DESC>(*static_cast<CAMERA_DESC*>(arg))).get();
	CAMERA_DESC& desc = *static_cast<CAMERA_DESC*>(arg);
	m_Transform->Set_Position(Vector3{ desc.eye });
	m_Transform->LookAt(Vector3{ desc.at });

	m_FovY = desc.fovY;
	m_Aspect = desc.aspect;
	m_Near = desc.nearPlane;
	m_Far = desc.farPlane;

	Update_CameraTransform(0.f);

	return S_OK;
}

void Camera::Set_Aspect(Float aspect)
{
	m_Aspect = aspect;
}

void Camera::Bind_Aspect(Float aspect)
{
	m_Aspect = aspect;
	Bind_CameraTransform();
}

void Camera::Set_Target(const Shared<GameObject>& target)
{
	m_Target = target;
	m_TargetGuid = target ? target->Get_ObjectGuid() : ObjectGuid{};
	Update_CameraTransform(0.f);
}

Shared<GameObject> Camera::Get_Target() const
{
	if (const Shared<GameObject> target = m_Target.lock(); target && !target->Is_Destroy())
		return target;
	m_Target.reset();
	if (!m_TargetGuid.Is_Valid())
		return nullptr;

	const Shared<GameObject> target = GAME_INSTANCE->Find(m_TargetGuid);
	if (!target || target->Is_Destroy())
		return nullptr;
	m_Target = target;
	return target;
}

HRESULT Camera::Set_TargetObjectGuid(ObjectGuid targetGuid)
{
	m_TargetGuid = targetGuid;
	m_Target.reset();
	Update_CameraTransform(0.f);
	return S_OK;
}

void Camera::Priority_Update(Float timeDelta)
{
	Update_CameraTransform(timeDelta);
}

void Camera::Update_CameraTransform(Float timeDelta)
{
	m_Transform->Update_WorldMatrix();
}

void Camera::Bind_CameraTransform() const
{
	m_Transform->Update_WorldMatrix();
	const auto& worldMatrix = m_Transform->Get_WorldMatrix();

	Matrix viewMat = worldMatrix.Invert();

	Float fAspect = m_Aspect;
	if (XMScalarNearEqual(fAspect, 0.0f, 0.00001f))
	{
		fAspect = 0.001f;
	}

	Matrix projMat = XMMatrixPerspectiveFovLH(
		m_FovY,
		fAspect,
		m_Near,
		m_Far
	);

	GAME_INSTANCE->Set_Transform(D3DTS::VIEW, viewMat);
	GAME_INSTANCE->Set_Transform(D3DTS::PROJ, projMat);
}
