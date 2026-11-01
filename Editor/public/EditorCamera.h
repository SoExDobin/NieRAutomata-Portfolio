#pragma once
#include "Camera.h"

NS_BEGIN(Editor)

class EditorCamera final : public Camera
{
public:
	typedef struct tagEditorCameraDesc final : public CAMERA_DESC
	{
		Float cameraSpeed{ 10.0f };
		Float mouseSensitive{ 0.1f };
	} EDITOR_CAMERA_DESC;

public:
	explicit EditorCamera(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	~EditorCamera() override = default;

public:
	HRESULT Initialize(void* arg) override;

	void Priority_Update(Float timeDelta) override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	void Fixed_Update(Float fixedDelta) override;
	HRESULT Render() override;

public:
	HRESULT Bind_EditorMatrix() const;
	/* Glides to frame a sphere while keeping the current view direction (Unreal-style F focus). */
	void Focus(const Vector3& center, Float radius);
	/* Play: takes the game camera's pose and lens each frame, so Pause looks around from where the game was. */
	void Follow(const Shared<Camera>& camera);
	/* Play keeps the editing pose and lens; Stop puts them back. */
	void Hold_EditPose();
	void Restore_EditPose();
	
private:
	Float m_CameraSpeed{};
	Float m_MouseSensitive{};

	Bool m_IsFocusing{ false };
	Float m_FocusElapsed{};
	Vector3 m_FocusStart{};
	Vector3 m_FocusGoal{};

	Vector3 m_EditPosition{};
	Quaternion m_EditRotation{};
	Float m_EditFovY{}, m_EditNear{}, m_EditFar{};

public:
	static Shared<EditorCamera> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context, EDITOR_CAMERA_DESC& desc);
	Shared<GameObject> Clone(void* arg) override { return nullptr; }
};

NS_END
