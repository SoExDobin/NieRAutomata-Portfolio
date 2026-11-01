#include "pch.h"
#include "CameraEventSystem.h"
#include "ThirdPersonCamera.h"
#include "Pl0000Input.h"
#include <Game.h>
#include <SpdLogger.h>
#include <UIImage.h>

namespace
{
    Float CameraEventEase(Float t)
    {
        t = std::clamp(t, 0.f, 1.f);
        return t * t * (3.f - 2.f * t);
    }

    Bool CameraEventFinite(const Vector3& value)
    {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    }

    Vector3 CameraEventVector(const nlohmann::json& value)
    {
        if (!value.is_array() || value.size() != 3) throw std::runtime_error("Invalid camera vector");
        return { value.at(0).get<Float>(), value.at(1).get<Float>(), value.at(2).get<Float>() };
    }
}

CameraEventSystem::~CameraEventSystem() { Cancel(); }

Bool CameraEventSystem::Play(CAMERA_EVENT event)
{
    const auto level = static_cast<LEVEL>(GAME_INSTANCE->Get_CurrentLevelIndex());
    const wchar_t* dataPath = nullptr;
    LEVEL requiredLevel = LEVEL::LEVEL_END;
    switch (event)
    {
    case CAMERA_EVENT::CITY_ARRIVAL:
        requiredLevel = LEVEL::GAMEPLAY;
        dataPath = L"../../../Client/bin/resources/CameraEvents/CityArrival.json";
        break;
    case CAMERA_EVENT::PARK_ARRIVAL:
        requiredLevel = LEVEL::AMUSEMENT_PARK;
        dataPath = L"../../../Client/bin/resources/CameraEvents/ParkArrival.json";
        break;
    default: return false;
    }
    if (level != requiredLevel || Is_Busy() || (m_Level == level && m_Played)) return false;
    const auto camera = Cast<ThirdPersonCamera>(GAME_INSTANCE->Get_MainCamera());
    if (!camera || camera->Is_Held() || !camera->Get_Target()) return false;

    try
    {
        // City uses authored world-space shots; Park retains its original gaze-only data.
        ifstream file(dataPath);
        nlohmann::json data;
        file >> data;
        m_BlendIn = data.at("blendInSeconds").get<Float>();
        m_BlendOut = data.at("blendOutSeconds").get<Float>();
        if (!std::isfinite(m_BlendIn) || m_BlendIn < 0.f ||
            !std::isfinite(m_BlendOut) || m_BlendOut < 0.f) return false;
        vector<Shot> shots;
        Float duration = 0.f;
        if (data.contains("shots"))
        {
            const auto& entries = data.at("shots");
            if (!entries.is_array() || entries.empty()) return false;
            for (const auto& entry : entries)
            {
                Shot shot;
                shot.eye = CameraEventVector(entry.at("eye"));
                shot.target = CameraEventVector(entry.at("target"));
                shot.seconds = entry.at("seconds").get<Float>();
                shot.advance = entry.at("advance").get<Float>();
                shot.fovDegrees = entry.at("fovDegrees").get<Float>();
                const Float distance = (shot.target - shot.eye).Length();
                if (!CameraEventFinite(shot.eye) || !CameraEventFinite(shot.target) ||
                    !std::isfinite(distance) || distance <= 0.01f ||
                    !std::isfinite(shot.seconds) || shot.seconds <= 0.f ||
                    !std::isfinite(shot.advance) || shot.advance < 0.f || shot.advance >= distance - 0.01f ||
                    !std::isfinite(shot.fovDegrees) || shot.fovDegrees <= 1.f || shot.fovDegrees >= 179.f)
                    return false;
                duration += shot.seconds;
                if (!std::isfinite(duration)) return false;
                shots.push_back(shot);
            }
        }
        else
        {
            m_Target = CameraEventVector(data.at("target"));
            m_Hold = data.at("holdSeconds").get<Float>();
            m_EyeLift = data.at("eyeLift").get<Float>();
            m_EyeAdvance = data.at("eyeAdvance").get<Float>();
            // Zero/absent preserves the current gameplay FOV.
            m_EventFovDegrees = data.value("fovDegrees", 0.f);
            if (!std::isfinite(m_Target.x) || !std::isfinite(m_Target.y) || !std::isfinite(m_Target.z) ||
                !std::isfinite(m_BlendIn) || m_BlendIn <= 0.f || !std::isfinite(m_Hold) || m_Hold < 0.f ||
                !std::isfinite(m_BlendOut) || m_BlendOut < 0.f ||
                !std::isfinite(m_EyeLift) || m_EyeLift < 0.f ||
                !std::isfinite(m_EyeAdvance) || m_EyeAdvance < 0.f ||
                !std::isfinite(m_EventFovDegrees) ||
                (m_EventFovDegrees != 0.f && (m_EventFovDegrees <= 1.f || m_EventFovDegrees >= 179.f))) return false;
        }
        m_Shots = std::move(shots);
        m_ShotsDuration = duration;
    }
    catch (const std::exception&)
    {
        LOG_WARN(L"[CameraEvent] {} data unavailable or invalid", dataPath);
        return false;
    }
    m_Level = level;
    m_Event = event;
    LOG_INFO(L"[CameraEvent] {} requested", ETOI(event));
    return true;
}

Bool CameraEventSystem::Begin()
{
    const auto camera = Cast<ThirdPersonCamera>(GAME_INSTANCE->Get_MainCamera());
    const auto player = camera ? camera->Get_Target() : nullptr;
    const auto input = player ? player->Get_Component<Pl0000Input>() : nullptr;
    if (!camera || camera->Is_Destroy() || camera->Is_Held() || !player || player->Is_Destroy() || !input)
        return false;
    m_StartEye = camera->Get_Transform()->Get_Position();
    if (m_Shots.empty() && (m_Target - m_StartEye).LengthSquared() < 0.01f) return false;
    if (!Ready_Frame()) return false;
    m_StartRotation = camera->Get_Transform()->Get_Quaternion();
    m_StartPlayer = player->Get_Transform()->Get_Position();
    Vector3 forward = m_Target - m_StartEye;
    forward.y = 0.f;
    if (forward.LengthSquared() > 0.0001f) forward.Normalize();
    else forward = Vector3::Zero;
    m_ShotOffset = Vector3::UnitY * m_EyeLift + forward * m_EyeAdvance;
    m_FovY = camera->Get_FovY();
    m_Camera = camera;
    m_Player = player;
    m_Input = input;
    m_InputWasActive = input->Is_Active();
    input->Set_Active(false);
    camera->Hold(true);
    m_Elapsed = 0.f;
    m_FrameElapsed = 0.f;
    m_Returning = false;
    m_ReturnElapsed = 0.f;
    m_EscapeWasDown = (GAME_INSTANCE->Get_DIKeyState(static_cast<uByte>(DIKEYBOARD_ESCAPE)) & 0x80) != 0;
    m_Running = true;
    m_Played = true;
    LOG_INFO(L"[CameraEvent] {} started", ETOI(m_Event));
    return true;
}

Bool CameraEventSystem::Ready_Frame()
{
    const auto tags = GAME_INSTANCE->Get_TextureTags(ETOI(LEVEL::STATIC));
    if (std::find(tags.begin(), tags.end(), L"UI_HUD_Black") == tags.end() &&
        FAILED(GAME_INSTANCE->Load_Texture(ETOI(LEVEL::STATIC),
            L"../../../Client/bin/resources/Texture/UI/HUD/HUD_Black.png", 1, L"UI_HUD_Black"))) return false;
    // Runtime-only images owned here, never inserted into the authored scene hierarchy.
    const auto prototype = UIImage::CreatePrototype();
    if (!prototype) return false;
    for (const auto& rect : { Vector4{0.f, -585.f, 1920.f, 90.f}, Vector4{0.f, 585.f, 1920.f, 90.f},
        Vector4{-980.f, 0.f, 40.f, 1080.f}, Vector4{980.f, 0.f, 40.f, 1080.f} })
    {
        UIImage::UI_IMAGE_DESC desc;
        desc.x = rect.x; desc.y = rect.y; desc.sizeX = rect.z; desc.sizeY = rect.w;
        desc.textureTag = L"UI_HUD_Black";
        desc.color = Color{0.f, 0.f, 0.f, 1.f};
        const auto image = Cast<UIImage>(prototype->Clone(&desc));
        if (!image) return false;
        m_Frame.push_back(image);
    }
    return true;
}

void CameraEventSystem::Apply_Frame(Float progress)
{
    const Float t = std::clamp(progress, 0.f, 1.f);
    // Opaque bars slide from fully outside the canvas to their final edge positions.
    const Vector3 positions[] = {{0.f, -585.f + 90.f * t, 0.f}, {0.f, 585.f - 90.f * t, 0.f},
        {-980.f + 40.f * t, 0.f, 0.f}, {980.f - 40.f * t, 0.f, 0.f}};
    for (size_t i = 0; i < m_Frame.size(); ++i)
        m_Frame[i]->Get_Transform()->Set_LocalPosition(positions[i]);
}

void CameraEventSystem::Apply(Float blend)
{
    const auto camera = m_Camera.lock();
    const auto player = m_Player.lock();
    if (!camera || !player) return;
    const auto transform = camera->Get_Transform();
    // Authored clearance blends in/out while tracking any gravity displacement of the player.
    const Vector3 eye = m_StartEye + player->Get_Transform()->Get_Position() - m_StartPlayer + m_ShotOffset * blend;
    transform->Set_Position(eye);
    transform->LookAt(m_Target);
    transform->Update_WorldMatrix();
    const Quaternion gaze = transform->Get_Quaternion();
    transform->Set_Rotation(Quaternion::Slerp(m_StartRotation, gaze, blend));
    const Float shotFov = m_EventFovDegrees == 0.f ? m_FovY : XMConvertToRadians(m_EventFovDegrees);
    camera->Set_FovY(m_FovY + (shotFov - m_FovY) * blend);
    transform->Update_WorldMatrix();
    // Level updates run after the normal camera bind; refresh view, inverse view and frustum together.
    GAME_INSTANCE->Update_CameraPipeline();
}

void CameraEventSystem::Apply_Pose(const Vector3& eye, const Quaternion& rotation, Float fov)
{
    const auto camera = m_Camera.lock();
    if (!camera) return;
    const auto transform = camera->Get_Transform();
    transform->Set_Position(eye);
    transform->Set_Rotation(rotation);
    camera->Set_FovY(fov);
    transform->Update_WorldMatrix();
    GAME_INSTANCE->Update_CameraPipeline();
}

void CameraEventSystem::Apply_Sequence(Float elapsed)
{
    const auto camera = m_Camera.lock();
    const auto player = m_Player.lock();
    if (!camera || !player) return;
    Float local = (std::max)(0.f, elapsed - m_BlendIn);
    size_t index = 0;
    while (index + 1 < m_Shots.size() && local >= m_Shots[index].seconds)
        local -= m_Shots[index++].seconds;
    const auto& shot = m_Shots[index];
    Vector3 forward = shot.target - shot.eye;
    forward.Normalize();
    // Cut to each world position, then dolly slowly along Look.
    const Vector3 eye = shot.eye + forward * shot.advance * std::clamp(local / shot.seconds, 0.f, 1.f);
    const auto transform = camera->Get_Transform();
    transform->Set_Position(eye);
    transform->LookAt(shot.target);
    transform->Update_WorldMatrix();
    const Quaternion rotation = transform->Get_Quaternion();
    const Float blend = m_BlendIn > 0.f ? CameraEventEase(elapsed / m_BlendIn) : 1.f;
    const Vector3 home = m_StartEye + player->Get_Transform()->Get_Position() - m_StartPlayer;
    Apply_Pose(Vector3::Lerp(home, eye, blend), Quaternion::Slerp(m_StartRotation, rotation, blend),
        m_FovY + (XMConvertToRadians(shot.fovDegrees) - m_FovY) * blend);
}

void CameraEventSystem::Update_Sequence(Float timeDelta, Bool skip)
{
    const auto camera = m_Camera.lock();
    const auto player = m_Player.lock();
    if (!camera || !player) return;
    if (m_Returning) m_ReturnElapsed += timeDelta;
    else
    {
        if (!skip)
        {
            m_Elapsed += timeDelta;
            Apply_Sequence(m_Elapsed);
        }
        if (skip || m_Elapsed >= m_BlendIn + m_ShotsDuration)
        {
            if (m_BlendOut == 0.f) { Cancel(); return; }
            // Return from the displayed pose, including an Escape during entry or any cut.
            m_Returning = true;
            m_ReturnElapsed = skip ? 0.f : m_Elapsed - m_BlendIn - m_ShotsDuration;
            m_ReturnEye = camera->Get_Transform()->Get_Position();
            m_ReturnRotation = camera->Get_Transform()->Get_Quaternion();
            m_ReturnFov = camera->Get_FovY();
        }
    }
    if (!m_Returning) return;
    const Float blend = CameraEventEase(m_ReturnElapsed / m_BlendOut);
    const Vector3 home = m_StartEye + player->Get_Transform()->Get_Position() - m_StartPlayer;
    Apply_Pose(Vector3::Lerp(m_ReturnEye, home, blend), Quaternion::Slerp(m_ReturnRotation, m_StartRotation, blend),
        m_ReturnFov + (m_FovY - m_ReturnFov) * blend);
    if (m_ReturnElapsed >= m_BlendOut)
    {
        LOG_INFO(L"[CameraEvent] {} finished", ETOI(m_Event));
        Cancel();
    }
}

void CameraEventSystem::Update(LEVEL level, Float timeDelta, Bool entryReady)
{
    if (m_Level != level) { End_Level(m_Level); m_Level = level; }
    if (!Is_Busy() || !entryReady) return;
    if (!m_Running && !Begin()) { Cancel(); return; }
    const auto camera = m_Camera.lock();
    const auto player = m_Player.lock();
    if (!camera || camera->Is_Destroy() || !player || player->Is_Destroy() ||
        camera != GAME_INSTANCE->Get_MainCamera() || camera->Get_Target() != player)
    { Cancel(); return; }

    Float delta = std::isfinite(timeDelta) ? (std::max)(timeDelta, 0.f) : 0.f;
    for (const auto& image : m_Frame)
        GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::UI_OVERLAY, image);
    const Bool escape = (GAME_INSTANCE->Get_DIKeyState(static_cast<uByte>(DIKEYBOARD_ESCAPE)) & 0x80) != 0;
    // Keep the gameplay pose for a three-second border Lerp before advancing any shot clock.
    constexpr Float frameSeconds = 3.f;
    if (m_FrameElapsed < frameSeconds)
    {
        if (escape && !m_EscapeWasDown) { Cancel(); return; }
        m_EscapeWasDown = escape;
        const Float consumed = (std::min)(delta, frameSeconds - m_FrameElapsed);
        m_FrameElapsed += consumed;
        delta -= consumed;
        Apply_Frame(m_FrameElapsed / frameSeconds);
        Apply_Pose(m_StartEye + player->Get_Transform()->Get_Position() - m_StartPlayer, m_StartRotation, m_FovY);
        if (m_FrameElapsed < frameSeconds) return;
    }
    if (!m_Shots.empty())
    {
        const Bool skip = escape && !m_EscapeWasDown;
        m_EscapeWasDown = escape;
        Update_Sequence(delta, skip);
        return;
    }
    m_Elapsed += delta;
    const Float returnStart = m_BlendIn + m_Hold;
    // Skip keeps the same smooth return instead of cutting back or consuming a held entry key.
    if (escape && !m_EscapeWasDown && m_Elapsed < returnStart)
    {
        if (m_BlendOut == 0.f) { Cancel(); return; }
        // Finish entering before returning, so the skip's first frame cannot jump to the full gaze.
        m_Hold = 0.f;
        m_Elapsed = (std::min)(m_Elapsed, m_BlendIn);
    }
    m_EscapeWasDown = escape;
    const Float outStart = m_BlendIn + m_Hold;
    const Float total = outStart + m_BlendOut;
    if (m_BlendOut == 0.f && m_Elapsed >= outStart) { Cancel(); return; }
    const Float blend = m_Elapsed < m_BlendIn ? CameraEventEase(m_Elapsed / m_BlendIn) :
        (m_Elapsed < outStart ? 1.f : 1.f - CameraEventEase((m_Elapsed - outStart) / m_BlendOut));
    Apply(blend);
    if (m_Elapsed >= total)
    {
        LOG_INFO(L"[CameraEvent] {} finished", ETOI(m_Event));
        Cancel();
    }
}

void CameraEventSystem::Cancel()
{
    // The renderer may still own a submitted reference in this frame.
    for (const auto& image : m_Frame) image->Set_Active(false);
    m_Frame.clear();
    if (m_Running)
    {
        // Only write the pose if this is still the camera/target we acquired.
        const auto camera = m_Camera.lock();
        const auto player = m_Player.lock();
        if (camera && !camera->Is_Destroy())
        {
            if (player && !player->Is_Destroy() && camera == GAME_INSTANCE->Get_MainCamera() && camera->Get_Target() == player)
                Apply_Pose(m_StartEye + player->Get_Transform()->Get_Position() - m_StartPlayer, m_StartRotation, m_FovY);
            camera->Set_FovY(m_FovY);
            camera->Hold(false);
        }
        if (const auto input = m_Input.lock(); input && !input->Is_Destroy())
            input->Set_Active(m_InputWasActive);
    }
    m_Running = false;
    m_Event = CAMERA_EVENT::END;
    m_Camera.reset();
    m_Player.reset();
    m_Input.reset();
}

void CameraEventSystem::End_Level(LEVEL level)
{
    if (m_Level != level) return;
    Cancel();
    m_Played = false;
    m_Level = LEVEL::LEVEL_END;
}
