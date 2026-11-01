#include "pch.h"
#include "FreeCamera.h"
#include "Game.h"
#include <SpdLogger.h>
#include <Transform.h>
#include "ClientSettingManager.h"

FreeCamera::FreeCamera() : Camera() {}
FreeCamera::FreeCamera(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: Camera{ device, context } {}
FreeCamera::FreeCamera(const FreeCamera& rhs)
	: Camera{ rhs } {}

HRESULT FreeCamera::Initialize_Prototype()
{
	return Camera::Initialize_Prototype();
}

HRESULT FreeCamera::Initialize(void* arg)
{
	FREE_CAMERA_DESC desc{};
	desc.eye = Vector4{ 0.f, 5.f, -10.f, 1.f };
	desc.at = Vector4{ 0.f, 0.f, 0.f, 1.f };
	desc.up = Vector4{ 0.f, 1.f, 0.f, 0.f };
	desc.fovY = XMConvertToRadians(60.0f);
	desc.aspect = static_cast<Float>(ClientSettingManager::g_EngineDesc.viewportWidth / ClientSettingManager::g_EngineDesc.viewportHeight);
	desc.nearPlane = 0.1f;
	desc.farPlane = 1000.f;
	m_MouseSensitive = desc.mouseSensitive;

	return Camera::Initialize(&desc);
}

void FreeCamera::Priority_Update(Float timeDelta)
{
	Float speed = 30.f * timeDelta;
	Vector3 look = m_Transform->Get_WorldMatrix().Backward(); // (또는 Forward)
	Vector3 right = m_Transform->Get_WorldMatrix().Right();
	
	look.y = 0.f;  look.Normalize();
	right.y = 0.f; right.Normalize();
	Vector3 currentPos = m_Transform->Get_Position();
	if (GAME_INSTANCE->Get_DIKeyState(DIK_W) & 0x80) currentPos += look * speed;
	if (GAME_INSTANCE->Get_DIKeyState(DIK_S) & 0x80) currentPos -= look * speed;
	if (GAME_INSTANCE->Get_DIKeyState(DIK_A) & 0x80) currentPos -= right * speed;
	if (GAME_INSTANCE->Get_DIKeyState(DIK_D) & 0x80) currentPos += right * speed;

	m_Transform->Set_Position(currentPos);


	Long mouseWheel = GAME_INSTANCE->Get_DIMouseMove(DIMM::WHEEL);
	if (mouseWheel != 0)
	{
		// 휠은 Y축 제약 없이 부드럽게 시선 방향으로 빨려 들어가야 줌 효과가 납니다.
		Vector3 zoomDir = m_Transform->Get_WorldMatrix().Backward();
		zoomDir.Normalize();
		m_Transform->Set_Position(m_Transform->Get_Position() + zoomDir * static_cast<Float>(mouseWheel) * timeDelta * 0.5f);
	}
	// 3. 우클릭 드래그 시 회전 (Turn)
	if (GAME_INSTANCE->Get_DIMouseState(DIMB::RBUTTON) & 0x80)
	{
		Long mouseMoveX = GAME_INSTANCE->Get_DIMouseMove(DIMM::X);
		Long mouseMoveY = GAME_INSTANCE->Get_DIMouseMove(DIMM::Y);
		if (mouseMoveX != 0)
		{
			m_Transform->Turn(Vector3{
				0.f,
				static_cast<Float>(mouseMoveX),
				0.f},
				timeDelta,
				m_MouseSensitive);
		}
		if (mouseMoveY != 0)
		{
			m_Transform->Turn(Vector3{
				static_cast<Float>(mouseMoveY),
				0.f,
				0.f },
				timeDelta,
				m_MouseSensitive);
		}
	}

	Camera::Priority_Update(timeDelta);
}

void FreeCamera::Update(Float timeDelta) {}
void FreeCamera::Late_Update(Float timeDelta) {}
void FreeCamera::Fixed_Update(Float fixedDelta){}
HRESULT FreeCamera::Render() { return S_OK; }

Shared<FreeCamera> FreeCamera::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto camera = make_shared<FreeCamera>(device, context);

	if (FAILED(camera->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : FreeCamera");
		return nullptr;
	}

	return camera;
}

Shared<GameObject> FreeCamera::Clone(void* arg)
{
	auto camera = make_shared<FreeCamera>(*this);

	if (FAILED(camera->Initialize(arg)))
	{
		MSG_BOX("Failed to CreateComponent : FreeCamera");
		return nullptr;
	}

	return camera;
}
