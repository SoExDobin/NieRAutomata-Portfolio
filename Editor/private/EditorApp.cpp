#include "pch.h"
#include "ImGuizmo.h"
#include "EditorApp.h"
#include "ClientApp.h"
#include "ClientSettingManager.h"

#include "EditorManager.h"

#include "LayerRegistry.h"
#include "MenuBar.h"
#include "PathManager.h"
#include "TagRegistry.h"
#include "EditorCamera.h"
#include "Model.h"
#include <wincodec.h>
#include <sstream>

EditorApp::EditorApp() {}

#ifdef _DEBUG
// Dev check of what the level really renders: once the level is loaded, the Scene View camera looks along
// +Z and then -Z at the object drawing NIER_SCENE_CAPTURE's model, and each view is saved as
// SceneCapture/scene-<tag>-{pz,nz}.png (Scene View = editor camera, full deferred pipeline).
// NIER_SCENE_CAPTURE_VIEWS="ex ey ez tx ty tz;..." replaces those views with explicit world-space eye/target
// pairs saved as scene-<tag>-v<N>.png (map tiles: views near the player instead of the whole tile).
// Each save also logs the D3D debug layer error count.
void EditorApp::Update_SceneCapture()
{
    wchar_t tag[64]{};
    size_t length = 0;
    if (0 != _wgetenv_s(&length, tag, L"NIER_SCENE_CAPTURE") || 0 == tag[0])
        return;
    const uint32 level = GAME_INSTANCE->Get_CurrentLevelIndex();
    if (!GAME_INSTANCE->LevelLoad_Finished() || level == ETOI(LEVEL::LOADING))
        return;
    // NIER_SCENE_CAPTURE_LEVEL=N captures level N instead of the start level (Level Select reset), and
    // NIER_SCENE_CAPTURE_SCENE=<scene .json> then loads that file over it, so trial values are captured from a
    // scratch copy without touching ProjectSetting/Scene.
    wchar_t levelText[8]{};
    if (0 == _wgetenv_s(&length, levelText, L"NIER_SCENE_CAPTURE_LEVEL") && 0 != levelText[0] &&
        static_cast<uint32>(_wtoi(levelText)) != level)
    {
        if (!EDITOR->Is_ResetRequested())
            EDITOR->Request_Reset(static_cast<uint32>(_wtoi(levelText)));
        return;
    }
    wchar_t scenePath[MAX_PATH]{};
    if (0 == m_SceneCaptureFrame && 0 == _wgetenv_s(&length, scenePath, L"NIER_SCENE_CAPTURE_SCENE") && 0 != scenePath[0])
        LOG_INFO(L"[SceneCapture] scene {} 0x{:08X}", scenePath,
            static_cast<uint32>(GAME_INSTANCE->DeSerializeLevel(scenePath)));

    const uint32 frame = ++m_SceneCaptureFrame;
    // NIER_SCENE_CAPTURE_SHADOW=0 captures with the cascaded shadows off (on/off comparison in one layout).
    wchar_t shadowText[8]{};
    if (frame == 1 && 0 == _wgetenv_s(&length, shadowText, L"NIER_SCENE_CAPTURE_SHADOW") &&
        wstring_view(shadowText) == L"0" && GAME_INSTANCE->Get_Shadow())
        GAME_INSTANCE->Toggle_Shadow();
    // NIER_SCENE_CAPTURE_OCCLUSION=0 captures with the F6 occlusion culling off (on/off comparison).
    wchar_t occlusionText[8]{};
    if (frame == 1 && 0 == _wgetenv_s(&length, occlusionText, L"NIER_SCENE_CAPTURE_OCCLUSION") &&
        wstring_view(occlusionText) == L"0" && GAME_INSTANCE->Get_Occlusion())
        GAME_INSTANCE->Toggle_Occlusion();
    const auto findTarget = [&]() -> Shared<GameObject> {
        for (const auto& [id, object] : GAME_INSTANCE->Get_GameObjects(level))
            if (const Shared<Model> model = object ? object->Get_Component<Model>() : nullptr;
                model && model->Get_ModelTag() == tag)
                return object;
        return nullptr;
    };
    // The camera is placed directly (Focus_Object eases over time). The distance fits the posed bounds;
    // the head close-up targets the top fifth of them.
    const auto aim = [&](Float yaw, Bool head) {
        const Shared<GameObject> target = findTarget();
        const Shared<EditorCamera> camera = EDITOR->Get_EditorCamera();
        const Shared<Model> model = target ? target->Get_Component<Model>() : nullptr;
        BoundingBox local{};
        if (!camera || !model || !model->Compute_PosedBounds(local)) return false;
        target->Get_Transform()->Update_WorldMatrix();
        BoundingBox world{};
        local.Transform(world, target->Get_Transform()->Get_WorldMatrix());
        Vector3 center = world.Center;
        Float halfHeight = world.Extents.y;
        if (head) { center.y += world.Extents.y * 0.8f; halfHeight = world.Extents.y * 0.2f; }
        const Float distance = halfHeight / tanf(XM_PIDIV4 * 0.5f) * 1.15f + (std::max)(world.Extents.x, world.Extents.z);
        const Vector3 look(sinf(yaw), 0.f, cosf(yaw));
        camera->Get_Transform()->Set_Position(center - look * distance);
        camera->Get_Transform()->LookAt(center);
        return true;
    };
    const auto save = [&](const wchar_t* suffix) {
        const auto view = GAME_INSTANCE->Get_OffScreenSRV(0);
        ComPtr<ID3D11Resource> resource;
        if (view) view->GetResource(resource.GetAddressOf());
        const filesystem::path directory = filesystem::absolute(L"../../../SceneCapture");
        std::error_code error;
        filesystem::create_directories(directory, error);
        const filesystem::path file = directory / (wstring(L"scene-") + tag + L"-" + suffix + L".png");
        const HRESULT saved = resource ? SaveWICTextureToFile(GAME_INSTANCE->Get_Context().Get(), resource.Get(),
            GUID_ContainerFormatPng, file.c_str()) : E_FAIL;
        LOG_INFO(L"[SceneCapture] {} 0x{:08X}", file.wstring(), static_cast<uint32>(saved));

        // D3D debug layer errors since start, so a capture run also proves the level rendered cleanly.
        uint32 d3dErrors = 0;
        string firstError;
        ComPtr<ID3D11InfoQueue> queue;
        if (SUCCEEDED(GAME_INSTANCE->Get_Device().As(&queue)))
            for (UINT64 index = 0; index < queue->GetNumStoredMessages(); ++index)
            {
                SIZE_T size = 0;
                queue->GetMessage(index, nullptr, &size);
                vector<Byte> bytes(size);
                auto* message = reinterpret_cast<D3D11_MESSAGE*>(bytes.data());
                if (FAILED(queue->GetMessage(index, message, &size)) || message->Severity > D3D11_MESSAGE_SEVERITY_ERROR)
                    continue;
                if (0 == d3dErrors++)
                    firstError.assign(message->pDescription, message->DescriptionByteLength);
            }
        // Last frame's draws; the shadow pass adds its dynamic cascades to it.
        if (d3dErrors > 0)
            LOG_WARN(L"[SceneCapture] D3D errors {} (first: {})", d3dErrors, Helper::To_wString(firstError));
        else
            LOG_INFO(L"[SceneCapture] D3D errors 0, draws {}, tris {}, frame {:.2f} ms, shadow {}, occlusion {} culled {} occluded {} | dynamic cascades draws {} {} cpu ms {:.2f} {:.2f} | static bakes {}",
                GAME_INSTANCE->Get_FrameDrawCount(), GAME_INSTANCE->Get_FrameTriangleCount(), m_SceneCaptureFrameMs,
                GAME_INSTANCE->Get_Shadow() ? L"on" : L"off",
                GAME_INSTANCE->Get_Occlusion() ? L"on" : L"off", GAME_INSTANCE->Get_FrameCulledCount(),
                GAME_INSTANCE->Get_FrameOccludedCount(),
                GAME_INSTANCE->Get_FrameCascadeDraws(0), GAME_INSTANCE->Get_FrameCascadeDraws(1),
                GAME_INSTANCE->Get_FrameCascadeMs(0), GAME_INSTANCE->Get_FrameCascadeMs(1),
                GAME_INSTANCE->Get_FrameShadowBakes());
    };

    wchar_t viewText[1024]{};
    if (0 == _wgetenv_s(&length, viewText, L"NIER_SCENE_CAPTURE_VIEWS") && 0 != viewText[0])
    {
        vector<pair<Vector3, Vector3>> views;
        std::wistringstream stream(viewText);
        wstring entry;
        while (std::getline(stream, entry, L';'))
        {
            std::wistringstream values(entry);
            Vector3 eye{}, target{};
            if (values >> eye.x >> eye.y >> eye.z >> target.x >> target.y >> target.z)
                views.emplace_back(eye, target);
        }
        const Shared<EditorCamera> camera = EDITOR->Get_EditorCamera();
        if (views.empty() || !camera) { LOG_ERROR(L"[SceneCapture] bad NIER_SCENE_CAPTURE_VIEWS"); PostQuitMessage(1); return; }
        if (frame == 60)
            if (const Shared<GameObject> target = findTarget())
            {
                // Logged so views can be matched to the source .blend (tile-local = world - position).
                BoundingBox local{};
                if (const Shared<Model> model = target->Get_Component<Model>(); model && model->Compute_LocalBounds(local))
                    LOG_INFO(L"[SceneCapture] {} local center ({} {} {}) extents ({} {} {})", tag,
                        local.Center.x, local.Center.y, local.Center.z, local.Extents.x, local.Extents.y, local.Extents.z);
            }
        // View i is placed at frame 60(i+1) and saved 60 frames later, when the next one is placed.
        if (frame % 60 == 20)
            m_SceneCaptureViewStart = std::chrono::steady_clock::now();
        if (frame % 60 != 0 || frame < 60)
            return;
        const size_t step = frame / 60 - 1;
        m_SceneCaptureFrameMs = std::chrono::duration<Float, std::milli>(std::chrono::steady_clock::now() - m_SceneCaptureViewStart).count() / 40.f;
        if (step > 0)
            save((L"v" + std::to_wstring(step - 1)).c_str());
        if (step >= views.size()) { PostQuitMessage(0); return; }
        camera->Get_Transform()->Set_Position(views[step].first);
        camera->Get_Transform()->LookAt(views[step].second);
        return;
    }

    if (frame == 60 && !aim(0.f, false)) { LOG_ERROR(L"[SceneCapture] no object draws model {}", tag); PostQuitMessage(1); }
    if (frame == 120) { save(L"pz-body"); aim(0.f, true); }
    if (frame == 180) { save(L"pz-head"); aim(XM_PI, false); }
    if (frame == 240) { save(L"nz-body"); aim(XM_PI, true); }
    if (frame == 300) { save(L"nz-head"); PostQuitMessage(0); }
}
#endif
EditorApp::~EditorApp()
{
	Destruct_IMGUI();
}

HRESULT EditorApp::Initialize() {
    // Level count and game resolution from ProjectSetting/EngineDesc.json, as the Launcher; the Editor keeps its own start level.
    ENGINE_DESC desc = {};
    ClientSettingManager::GetInstance()->Load_EngineDesc(desc);
    desc.hWnd = g_hWnd;
    desc.hInst = g_hInst;
#ifdef _DEBUG
    desc.winMode = WINMODE::WIN;
    desc.viewportWidth = 1600;
    desc.viewportHeight = 900;
#else
    desc.winMode = WINMODE::FULL;
#endif
    desc.startLevel = 3;
    desc.useOffscreenRendering = true;
    desc.renderTargetCount = 1;	// one Scene View: editing, and Play through the game camera
    desc.useLog = true;
    m_StartLevel = m_StartLevelIndex = desc.startLevel;

    EDITOR->Set_EngineDesc(desc);
    desc = EDITOR->Get_EngineDesc();

    /* 엔진 Core 설정 1순위 그래야만 하고 그래야하게 만들어야 함.*/
    if (FAILED(GAME_INSTANCE->Initialize_Engine(desc)))
      return E_FAIL;

    if (FAILED(EDITOR->Initialize()))
        return E_FAIL;

    ClientSettingManager::GetInstance()->Set_ResourcePath(
        L"../../../Client/bin/resources/");
    ClientSettingManager::GetInstance()->Set_ShaderPath(
        L"shaders/");

	if (!(m_ClientApp = ClientApp::Create(desc)))
	{
		LOG_ERROR(L"Editor startup could not initialize ClientApp");
		return E_FAIL;
	}

    if (FAILED(Initialize_IMGUI(desc)))
        return E_FAIL;
    
    GAME_INSTANCE->OnResize(desc.viewportWidth, desc.viewportHeight, 0);

	return S_OK;
}

void EditorApp::Update() {
    static EDITOR_STATE prevState = EDITOR_STATE::STOP;
    EDITOR_STATE curState = EDITOR->Get_State();
    
    if (curState == EDITOR_STATE::STOP && prevState != EDITOR_STATE::STOP) {
        m_IsReset = true;
    }
    if (EDITOR->Is_ResetRequested()) {
        ENGINE_DESC& desc = EDITOR->Get_EngineDesc();
        const uint32 requestedLevel = desc.startLevel;
        if (FAILED(Change_ClientLevel(requestedLevel)))
            EDITOR->Set_State(EDITOR_STATE::STOP);
        EDITOR->Clear_ResetRequest();
        m_IsReset = true;
    }
    else if (m_IsReset && FAILED(Reset_ClientApp())) {
        EDITOR->Set_State(EDITOR_STATE::STOP);
    }
    // A failed recreation stays stopped until an explicit reset succeeds.
    if (!m_ClientApp)
        EDITOR->Set_State(EDITOR_STATE::STOP);
    prevState = EDITOR->Get_State();
    
    const auto resizeRequests = EDITOR->Get_ResizeRequests();
    for (const auto& [index, info] : resizeRequests) {
        if (SUCCEEDED(GAME_INSTANCE->OnResize(info.width, info.height, index)))
            EDITOR->Clear_ResizeRequest(index);
    }
    
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();

    EDITOR->Update(m_IsReset);
}

HRESULT EditorApp::Render() {
    const HRESULT renderResult = EDITOR->Render(m_IsReset);
#ifdef _DEBUG
    Update_SceneCapture();
#endif
    
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
    
    if (m_IsReset) {
        m_IsReset = false;
    }

    const HRESULT presentResult = GAME_INSTANCE->Present();
    return FAILED(renderResult) ? renderResult : presentResult;
}

Unique<EditorApp> EditorApp::Create() {
    Unique<EditorApp> editorApp = make_unique<EditorApp>();

    if (FAILED(editorApp->Initialize())) {
      MSG_BOX("Editor could not start.\nCheck Editor/bin/logs/engine.log for the failing resource or initialization step.");
      return nullptr;
    }

    return editorApp;
}

HRESULT EditorApp::Initialize_IMGUI(const ENGINE_DESC &desc) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    (void)io;

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;           // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport /
                                                        // Platform Windows

    ImGui::StyleColorsDark();
    ImGuiStyle &style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
      style.WindowRounding = 0.f;
      style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    if (desc.hWnd == nullptr) {
        MSG_BOX("ERROR: HWND NULL! Failed ton Initialize ImGui");
        return E_FAIL;
    }

    ImGui_ImplWin32_Init(desc.hWnd);
    if (GAME_INSTANCE->Get_Device().Get() == nullptr ||
        GAME_INSTANCE->Get_Context().Get() == nullptr) {
      MSG_BOX("Failed ImGui_ImplDX11_Init");
      return E_FAIL;
    }
    if (!ImGui_ImplDX11_Init(GAME_INSTANCE->Get_Device().Get(), GAME_INSTANCE->Get_Context().Get())) {
      MSG_BOX("Failed ImGui_ImplDX11_Init");
      return E_FAIL;
    }

    return S_OK;
}

HRESULT EditorApp::Destruct_IMGUI() {
	if (ImGui::GetCurrentContext())
	{
		GAME_INSTANCE->Get_LayerRegister()->SaveToFile(PATH.GetLayerSettingsPath());
		GAME_INSTANCE->Get_TagRegister()->SaveToFile(PATH.GetTagSettingsPath());
	}

    // Client/resource startup can fail before the ImGui context/backends exist.
    if (ImGui::GetCurrentContext()) {
        if (ImGui::GetIO().BackendRendererUserData)
            ImGui_ImplDX11_Shutdown();
        if (ImGui::GetIO().BackendPlatformUserData)
            ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
    
    m_ClientApp.reset();
    GAME_INSTANCE->DestroyInstance();

	return S_OK;
}

HRESULT EditorApp::Reset_ClientApp() {
  m_ClientApp.reset();
  GAME_INSTANCE->Clear_AllResource();
  m_ClientApp = ClientApp::Create(EDITOR->Get_EngineDesc());
  if (!m_ClientApp) {
    GAME_INSTANCE->Clear_AllResource();
    EDITOR->Get_EngineDesc().startLevel = m_StartLevelIndex;
    LOG_ERROR(L"Client reset failed; editor remains stopped. Request a level reset to retry.");
    return E_FAIL;
  }
  m_StartLevelIndex = m_StartLevel;
  return S_OK;
}

HRESULT EditorApp::Change_ClientLevel(uint32 levIndex) {
    m_ClientApp.reset();
    GAME_INSTANCE->Clear_Resource(levIndex);
    EDITOR->Get_EngineDesc().startLevel = levIndex;
    m_ClientApp = ClientApp::Create(EDITOR->Get_EngineDesc());
    if (!m_ClientApp) {
        GAME_INSTANCE->Clear_AllResource();
        EDITOR->Get_EngineDesc().startLevel = m_StartLevelIndex;
        LOG_ERROR(L"Client level {} initialization failed; editor remains stopped. Request a level reset to retry.", levIndex);
        return E_FAIL;
    }
    m_StartLevelIndex = m_StartLevel = levIndex;
    return S_OK;
}
