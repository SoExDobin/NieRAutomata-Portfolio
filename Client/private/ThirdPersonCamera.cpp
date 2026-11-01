#include "pch.h"
#include "ThirdPersonCamera.h"
#include <SpdLogger.h>

#include "ClientSettingManager.h"
#include "Game.h"
#include "Pl0000.h"

namespace
{
	// 카메라가 벽·바닥(정적 COL)과 두는 거리. 근평면 귀퉁이(약 0.15m)가 벽을 넘지 않을 만큼 잡는다.
	constexpr Float CollisionMargin = 0.3f;
}

ThirdPersonCamera::ThirdPersonCamera() : Camera{} {}
ThirdPersonCamera::ThirdPersonCamera(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: Camera{device, context} {}
ThirdPersonCamera::ThirdPersonCamera(const ThirdPersonCamera& rhs)
	: Camera{rhs} {}

HRESULT ThirdPersonCamera::Initialize_Prototype()
{
	return Camera::Initialize_Prototype();
}

HRESULT ThirdPersonCamera::Initialize(void* arg)
{
	THIRD_PERSON_CAMERA_DESC localDesc{};
	localDesc.eye = Vector4{ 0.f, 10.f, -10.f, 1.f };
	localDesc.at = Vector4{ 0.f, 0.f, 0.f, 1.f };
	localDesc.up = Vector4{ 0.f, 1.f, 0.f, 1.f };
	localDesc.fovY = XMConvertToRadians(60.f);
	localDesc.aspect = static_cast<Float>(ClientSettingManager::g_EngineDesc.viewportWidth) / static_cast<Float>(ClientSettingManager::g_EngineDesc.viewportHeight);
	localDesc.nearPlane = 0.1f;
	localDesc.farPlane = 1000.f;
	
	localDesc.distance = 7.f;
	localDesc.minDistance = 3.f;
	localDesc.maxDistance = 30.f;
	localDesc.offset = Vector3{ 0.f, 1.f, 0.f };
	localDesc.mouseSensitive = 0.1f;
	localDesc.wheelSensitive = 0.01f;

	if (FAILED(Camera::Initialize(&localDesc)))
		return E_FAIL;

	m_ObjectDesc = static_cast<OBJECT_DESC*>(arg);

	m_Distance = localDesc.distance;
	m_TargetDistance = localDesc.distance;

	if (localDesc.minDistance == 0.f)
		m_MinDistance = m_TargetDistance;
	else
		m_MinDistance = localDesc.minDistance;
	if (localDesc.maxDistance == 0.f)
		m_MaxDistance = m_TargetDistance;
	else
		m_MaxDistance = localDesc.maxDistance;
	m_Offset = localDesc.offset;

	Vector3 angles = m_Transform->Get_Rotation();
	m_OrbitX = XMConvertToDegrees(angles.y);
	m_OrbitY = XMConvertToDegrees(angles.x);

	m_MouseSensitive = localDesc.mouseSensitive;
	m_WheelSensitive = localDesc.wheelSensitive;

	m_Friction = 0.8f;
	m_OrbitVelocityX = 0.f;
	m_OrbitVelocityY = 0.f;

	if (FAILED(GAME_INSTANCE->Set_MainCamera(static_pointer_cast<Camera>(shared_from_this()))))
		return E_FAIL;

	return S_OK;
}

void ThirdPersonCamera::On_Destroy()
{
	Camera::On_Destroy();
}

void ThirdPersonCamera::Priority_Update(Float timeDelta)
{
	const Shared<GameObject> target = Get_Target();
	if (!m_IsHeld && target)
	{
		if (!GAME_INSTANCE->Get_InputEnabled())
		{
			m_OrbitVelocityX = 0.f;
			m_OrbitVelocityY = 0.f;
		}
		auto targetTransform = target->Get_Transform();

		if (GAME_INSTANCE->Get_DIKeyState(static_cast<uByte>(DIKEYBOARD_ESCAPE)) & 0x80)
			GAME_INSTANCE->Set_MouseLock(false);

		if (GAME_INSTANCE->Get_DIMouseState(DIMB::LBUTTON) & 0x80)
			GAME_INSTANCE->Set_MouseLock(true);

		m_OrbitVelocityX += m_MouseSensitive * GAME_INSTANCE->Get_DIMouseMove(DIMM::X);
		m_OrbitVelocityY += m_MouseSensitive * GAME_INSTANCE->Get_DIMouseMove(DIMM::Y);
		
		Float frictionFactor = pow(m_Friction, timeDelta * 60.f);
		m_OrbitVelocityX *= frictionFactor;
		m_OrbitVelocityY *= frictionFactor;

		m_OrbitX += m_OrbitVelocityX * timeDelta * 60.f;
		m_OrbitY += m_OrbitVelocityY * timeDelta * 60.f;
		m_OrbitY = clamp(m_OrbitY, -30.f, 60.f);

		// While the player holds a lock-on of their own, the camera swings round behind them to keep the target ahead;
		// the mouse still turns it.
		if (const auto player = Cast<Pl0000>(target); player && player->Is_ManualLockOn())
		{
			const Vector3 toLockOn = player->Get_LockOnTarget().lock()->Get_Transform()->Get_Position() - targetTransform->Get_Position();
			if (toLockOn.x * toLockOn.x + toLockOn.z * toLockOn.z > 0.01f)
			{
				const Float yaw = XMConvertToDegrees(atan2f(toLockOn.x, toLockOn.z));
				m_OrbitX += std::remainder(yaw - m_OrbitX, 360.f) * (1.f - pow(0.9f, timeDelta * 60.f));
			}
		}

		Long mouseWheel = GAME_INSTANCE->Get_DIMouseMove(DIMM::WHEEL);
		if (mouseWheel != 0) {
			m_Distance -= m_WheelSensitive * static_cast<Float>(mouseWheel); // 0.01f는 줌 속도 조절용
			m_Distance = clamp(m_Distance, m_MinDistance, m_MaxDistance);
		}

		Quaternion mouseRot = Quaternion::CreateFromYawPitchRoll(
			XMConvertToRadians(m_OrbitX),
			XMConvertToRadians(m_OrbitY),
			0.f
		);
		Quaternion targetRot = mouseRot;
		
		Vector3 pivotPos = targetTransform->Get_Position() + Vector3::Transform(m_Offset, targetTransform->Get_Quaternion());
		Vector3 lookDir = Vector3::TransformNormal(Vector3(0, 0, -1), Matrix::CreateFromQuaternion(targetRot));
		Vector3 targetPos = pivotPos + lookDir * m_Distance;

		// 60 FPS에서 프레임당 0.3과 같고, 프레임 시간이 흔들려도 따라가는 거리가 일정하다.
		Float followFactor = 1.f - pow(0.7f, timeDelta * 60.f);
		Vector3 cameraPos = Vector3::Lerp(m_Transform->Get_Position(), targetPos, followFactor);

		// pivot과 카메라 사이에 벽·바닥이 있으면 그 앞까지 바로 당긴다. Lerp로 당기면 벽 속이 한 프레임 보인다.
		// 풀리면 위 Lerp가 원래 거리로 천천히 되돌린다. 가운데 레이 하나로는 벽을 스칠 때 근평면이 새므로
		// 카메라 좌우·상하로 여유만큼 벌린 레이를 더 쏴서 가장 가까운 것을 쓴다.
		const Vector3 toCamera = cameraPos - pivotPos;
		const Float cameraDistance = toCamera.Length();
		if (cameraDistance > 0.0001f)
		{
			const Vector3 cameraDir = toCamera / cameraDistance;
			const Vector3 right = Vector3::Transform(Vector3::UnitX, targetRot);
			const Vector3 up = Vector3::Transform(Vector3::UnitY, targetRot);
			const Vector3 rayEnd = pivotPos + cameraDir * (cameraDistance + CollisionMargin);
			const Vector3 spreads[]{ Vector3::Zero, right, -right, up, -up };

			Float allowedDistance = cameraDistance;
			for (const Vector3& spread : spreads)
			{
				const Vector3 ray = rayEnd + spread * CollisionMargin - pivotPos;
				const Float rayLength = ray.Length();
				Vector3 hit{};
				if (GAME_INSTANCE->Raycast_Physics(pivotPos, ray / rayLength, rayLength, hit))
					allowedDistance = min(allowedDistance, (hit - pivotPos).Dot(cameraDir) - CollisionMargin);
			}
			if (allowedDistance < cameraDistance)
				cameraPos = pivotPos + cameraDir * max(allowedDistance, 0.f);
		}
		m_Transform->Set_Position(cameraPos);
		m_Transform->Set_Rotation(targetRot);
	}

	Camera::Priority_Update(timeDelta);
}

void ThirdPersonCamera::Update(Float timeDelta)
{

}

void ThirdPersonCamera::Hold(Bool hold)
{
	if (m_IsHeld && !hold)
	{
		// The cutscene's last pose becomes the orbit: Priority_Update then only eases the position to the orbit distance.
		m_Transform->Update_WorldMatrix();
		const Vector3 angles = m_Transform->Get_Rotation();
		m_OrbitX = XMConvertToDegrees(angles.y);
		m_OrbitY = clamp(XMConvertToDegrees(angles.x), -30.f, 60.f);
		m_OrbitVelocityX = 0.f;
		m_OrbitVelocityY = 0.f;
	}
	m_IsHeld = hold;
}

void ThirdPersonCamera::Late_Update(Float timeDelta)
{
	
}

void ThirdPersonCamera::Fixed_Update(Float fixedDelta)
{
	
}

Shared<ThirdPersonCamera> ThirdPersonCamera::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto thirdPersonCamera = make_shared<ThirdPersonCamera>(device, context);

	if (FAILED(thirdPersonCamera->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : ThirdPersonCamera");
		return nullptr;
	}

	return thirdPersonCamera;
}

Shared<GameObject> ThirdPersonCamera::Clone(void* arg)
{
	auto thirdPersonCamera = make_shared<ThirdPersonCamera>(*this);

	if (FAILED(thirdPersonCamera->Initialize(arg)))
	{
		MSG_BOX("Failed to CreateComponent : ThirdPersonCamera");
		return nullptr;
	}

	return thirdPersonCamera;
}
