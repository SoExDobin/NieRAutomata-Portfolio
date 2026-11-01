#pragma once
#include "Client_Define.h"

NS_BEGIN(Engine)
class UIImage;
NS_END

NS_BEGIN(Client)

class ThirdPersonCamera;
class Pl0000Input;

// ClientApp-owned runtime camera events; scene objects only request an event ID.
class CameraEventSystem final
{
public:
    ~CameraEventSystem();
    Bool Play(CAMERA_EVENT event);
    void Update(LEVEL level, Float timeDelta, Bool entryReady);
    void Cancel();
    void End_Level(LEVEL level);
    Bool Is_Busy() const { return m_Event != CAMERA_EVENT::END; }

private:
    struct Shot
    {
        Vector3 eye{}, target{};
        Float seconds{}, advance{}, fovDegrees{};
    };
    Bool Begin();
    Bool Ready_Frame();
    void Apply_Frame(Float progress);
    void Apply(Float blend);
    void Apply_Pose(const Vector3& eye, const Quaternion& rotation, Float fov);
    void Apply_Sequence(Float elapsed);
    void Update_Sequence(Float timeDelta, Bool skip);

    CAMERA_EVENT m_Event{ CAMERA_EVENT::END };
    LEVEL m_Level{ LEVEL::LEVEL_END };
    Bool m_Played{};
    Bool m_Running{};
    Bool m_InputWasActive{};
    Bool m_EscapeWasDown{};
    Float m_Elapsed{};
    Float m_FrameElapsed{};
    Float m_BlendIn{}, m_Hold{}, m_BlendOut{};
    Float m_EyeLift{}, m_EyeAdvance{};
    Float m_FovY{};
    Float m_EventFovDegrees{};
    Vector3 m_Target{};
    Vector3 m_StartEye{}, m_StartPlayer{}, m_ShotOffset{};
    Quaternion m_StartRotation{};
    vector<Shot> m_Shots;
    Float m_ShotsDuration{};
    Bool m_Returning{};
    Float m_ReturnElapsed{}, m_ReturnFov{};
    Vector3 m_ReturnEye{};
    Quaternion m_ReturnRotation{};
    Weak<ThirdPersonCamera> m_Camera;
    Weak<GameObject> m_Player;
    Weak<Pl0000Input> m_Input;
    vector<Shared<UIImage>> m_Frame;
};

NS_END
