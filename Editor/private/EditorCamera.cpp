#include "pch.h"
#include "EditorCamera.h"

#include <Transform.h>

EditorCamera::EditorCamera(const ComPtr<ID3D11Device> &device,
                           const ComPtr<ID3D11DeviceContext> &context)
    : Camera{device, context} {}

HRESULT EditorCamera::Initialize(void *arg) {
	if (nullptr == arg) {
		LOG_ERROR(L"Need EDITOR_CAMERA_DESC to Create Camera");
		return E_FAIL;
	}
	if (FAILED(Camera::Initialize_Prototype())) {
		LOG_ERROR(L"Failed to Initialize Prototype");
		return E_FAIL;
	}

	const EDITOR_CAMERA_DESC &desc = *static_cast<EDITOR_CAMERA_DESC *>(arg);
	m_CameraSpeed = desc.cameraSpeed;
	m_MouseSensitive = desc.mouseSensitive;

	if (FAILED(GameObject::Initialize(arg)))
		return E_FAIL;

	m_ObjectDesc = static_pointer_cast<OBJECT_DESC>(make_shared<CAMERA_DESC>(*static_cast<CAMERA_DESC*>(arg))).get();
	m_Transform->Set_Position(Vector3{ desc.eye });
	m_Transform->LookAt(Vector3{ desc.at });
	m_FovY = desc.fovY;
	m_Aspect = desc.aspect;
	m_Near = desc.nearPlane;
	m_Far = desc.farPlane;

	Update_CameraTransform(0.f);

	return S_OK;
}

void EditorCamera::Priority_Update(Float timeDelta) {
  Camera::Priority_Update(timeDelta);
}

void EditorCamera::Update(Float timeDelta) {

    if (timeDelta <= 0)
        timeDelta = 0.016667f;
    // The steps below read the position back from the world matrix: bring it up to a pose set since the last frame
    // (Follow, Restore_EditPose), or that pose is written back over.
    m_Transform->Update_WorldMatrix();

    if (m_IsFocusing)
    {
        constexpr Float focusDuration = 0.25f;
        m_FocusElapsed += timeDelta;
        Float t = std::min(1.f, m_FocusElapsed / focusDuration);
        t = t * t * (3.f - 2.f * t);
        m_Transform->Set_Position(Vector3::Lerp(m_FocusStart, m_FocusGoal, t));
        // Get_Position reads the world matrix, which the WASD step below starts from.
        m_Transform->Update_WorldMatrix();
        if (m_FocusElapsed >= focusDuration)
            m_IsFocusing = false;
    }

    Float speed = m_CameraSpeed * timeDelta;
    // 1. WASD (XZ 평면 이동)
    Vector3 look = m_Transform->Get_WorldMatrix().Backward();
    Vector3 right = m_Transform->Get_WorldMatrix().Right();

    // Y축 파괴 및 정규화
    look.y = 0.f;  look.Normalize();
    right.y = 0.f; right.Normalize();
    Vector3 currentPos = m_Transform->Get_Position();
    if (GAME_INSTANCE->Get_DIKeyState(DIK_W) & 0x80) currentPos += look * speed;
    if (GAME_INSTANCE->Get_DIKeyState(DIK_S) & 0x80) currentPos -= look * speed;
    if (GAME_INSTANCE->Get_DIKeyState(DIK_A) & 0x80) currentPos -= right * speed;
    if (GAME_INSTANCE->Get_DIKeyState(DIK_D) & 0x80) currentPos += right * speed;
    m_Transform->Set_Position(currentPos);
    // 2. 마우스 휠 (줌 인/아웃)
	Long mouseWheel = GAME_INSTANCE->Get_DIMouseMove(DIMM::WHEEL);
	if (mouseWheel != 0)
	{
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
				0.f },
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
}

void EditorCamera::Late_Update(Float timeDelta) {}
void EditorCamera::Fixed_Update(Float fixedDelta) {}
HRESULT EditorCamera::Render() { return S_OK; }

void EditorCamera::Focus(const Vector3& center, Float radius) {
  radius = std::max(radius, 0.5f);
  m_Transform->Update_WorldMatrix();
  Vector3 look = m_Transform->Get_WorldMatrix().Backward();
  look.Normalize();

  // Distance at which the bounding sphere fits the narrower field of view, with a margin.
  const Float halfFovY = m_FovY * 0.5f;
  const Float halfFovX = atanf(tanf(halfFovY) * std::max(m_Aspect, 0.001f));
  const Float distance = radius / sinf(std::min(halfFovX, halfFovY)) * 1.1f;

  if (m_Far < distance + radius * 2.f)
    m_Far = distance + radius * 2.f;

  m_FocusStart = m_Transform->Get_Position();
  m_FocusGoal = center - look * distance;
  m_FocusElapsed = 0.f;
  m_IsFocusing = true;
}

void EditorCamera::Follow(const Shared<Camera>& camera) {
  Vector3 scale{}, position{};
  Quaternion rotation{};
  Matrix world = camera->Get_Transform()->Get_WorldMatrix();
  world.Decompose(scale, rotation, position);
  m_Transform->Set_Position(position);
  m_Transform->Set_Rotation(rotation);
  m_FovY = camera->Get_FovY();
  m_Near = camera->Get_NearPlane();
  m_Far = camera->Get_FarPlane();
  m_IsFocusing = false;
}

void EditorCamera::Hold_EditPose() {
  m_Transform->Update_WorldMatrix();
  m_EditPosition = m_Transform->Get_Position();
  m_EditRotation = m_Transform->Get_Quaternion();
  m_EditFovY = m_FovY;
  m_EditNear = m_Near;
  m_EditFar = m_Far;
}

void EditorCamera::Restore_EditPose() {
  m_Transform->Set_Position(m_EditPosition);
  m_Transform->Set_Rotation(m_EditRotation);
  m_FovY = m_EditFovY;
  m_Near = m_EditNear;
  m_Far = m_EditFar;
  m_IsFocusing = false;
}

HRESULT EditorCamera::Bind_EditorMatrix() const {
  m_Transform->Update_WorldMatrix();
  const Matrix &viewMatrix = m_Transform->Get_WorldMatrix().Invert();
  Matrix projMatrix = XMMatrixPerspectiveFovLH(m_FovY, m_Aspect, m_Near, m_Far);

  GAME_INSTANCE->Set_Transform(D3DTS::VIEW, viewMatrix);
  GAME_INSTANCE->Set_Transform(D3DTS::PROJ, projMatrix);

  return S_OK;
}

Shared<EditorCamera>
EditorCamera::Create(const ComPtr<ID3D11Device> &device,
                     const ComPtr<ID3D11DeviceContext> &context,
                     EDITOR_CAMERA_DESC &desc) {
  auto editorCamera = make_shared<EditorCamera>(device, context);

  if (FAILED(editorCamera->Initialize(&desc))) {
    LOG_ERROR(L"Failed to Create EditorCamera");
    return nullptr;
  }

  return editorCamera;
}
