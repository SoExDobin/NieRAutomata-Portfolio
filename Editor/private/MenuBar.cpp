#include "pch.h"
#include "MenuBar.h"
#include "AuthoringBackup.h"
#include "PathManager.h"
#include "EditorManager.h"
#include "ModelViewer.h"
#include "AnimationPresetEditor.h"
#include "ClientSettingManager.h"
#include "InputDevice.h"    
#include "CollisionManager.h"
#include "Navigation.h"
#include "NavHelper.h"
#include "Model.h"
#include "WorldMap.h"
#include "PointLight.h"
#include <nlohmann/json.hpp>

namespace
{
	/* Toggles in key order: F1 frame stats (FrameStatsKey, below), F2~F3 debug views (Debug builds only), F4~F7
	   optimization switches, then Ctrl+F1~F8 render target and shading switches (Ctrl+F1 Debug only). Update_HotKey,
	   the Debug menu and the (?) hotkey help all read this table, so a key is changed in one place. */
	struct DEBUG_TOGGLE
	{
		uByte key;
		Bool ctrl;
		const Char* shortcut;
		const Char* label;
		Bool (*get)();
		void (*toggle)();
	};
	const DEBUG_TOGGLE DebugToggles[] =
	{
#ifdef _DEBUG
		{ DIK_F2, false, "F2", "Collider Debug", []() -> Bool { return GAME_INSTANCE->Get_RenderDebug(); }, [] { GAME_INSTANCE->Toggle_RenderDebug(); } },
		{ DIK_F3, false, "F3", "Navigation Debug", []() -> Bool { return Navigation::Get_DebugRender(); }, [] { Navigation::Toggle_DebugRender(); } },
#endif
		{ DIK_F4, false, "F4", "Frustum Culling", []() -> Bool { return GAME_INSTANCE->Get_FrustumCulling(); }, [] { GAME_INSTANCE->Toggle_FrustumCulling(); } },
		{ DIK_F5, false, "F5", "World LOD", []() -> Bool { return GAME_INSTANCE->Get_WorldLod(); }, [] { GAME_INSTANCE->Toggle_WorldLod(); } },
		{ DIK_F6, false, "F6", "Occlusion Culling", []() -> Bool { return GAME_INSTANCE->Get_Occlusion(); }, [] { GAME_INSTANCE->Toggle_Occlusion(); } },
		{ DIK_F7, false, "F7", "Instancing", []() -> Bool { return GAME_INSTANCE->Get_Instancing(); }, [] { GAME_INSTANCE->Toggle_Instancing(); } },
#ifdef _DEBUG
		{ DIK_F1, true, "Ctrl+F1", "MRT Preview", []() -> Bool { return GAME_INSTANCE->Get_RenderTargetDebug(); }, [] { GAME_INSTANCE->Toggle_RenderTargetDebug(); } },
#endif
		{ DIK_F2, true, "Ctrl+F2", "Shadow", []() -> Bool { return GAME_INSTANCE->Get_Shadow(); }, [] { GAME_INSTANCE->Toggle_Shadow(); } },
		{ DIK_F3, true, "Ctrl+F3", "SSAO", []() -> Bool { return GAME_INSTANCE->Get_SSAO(); }, [] { GAME_INSTANCE->Toggle_SSAO(); } },
		{ DIK_F4, true, "Ctrl+F4", "Volumetric Light", []() -> Bool { return GAME_INSTANCE->Get_Volumetric(); }, [] { GAME_INSTANCE->Toggle_Volumetric(); } },
		{ DIK_F5, true, "Ctrl+F5", "Fog", []() -> Bool { return GAME_INSTANCE->Get_Fog(); }, [] { GAME_INSTANCE->Toggle_Fog(); } },
		{ DIK_F6, true, "Ctrl+F6", "Bloom", []() -> Bool { return GAME_INSTANCE->Get_Bloom(); }, [] { GAME_INSTANCE->Toggle_Bloom(); } },
		{ DIK_F7, true, "Ctrl+F7", "Color Grade", []() -> Bool { return GAME_INSTANCE->Get_ColorGrade(); }, [] { GAME_INSTANCE->Toggle_ColorGrade(); } },
		// F12는 디버거가 붙으면 Windows가 중단점 키로 가로채므로 쓰지 않는다.
		{ DIK_F8, true, "Ctrl+F8", "Cascade Colors", []() -> Bool { return GAME_INSTANCE->Get_CascadeDebug(); }, [] { GAME_INSTANCE->Toggle_CascadeDebug(); } },
	};
	// First optimization entry (F4); the Debug menu puts a separator before it when F2~F3 exist and before the first Ctrl entry.
#ifdef _DEBUG
	constexpr uint32 OptimizationToggleBegin = 2;
#else
	constexpr uint32 OptimizationToggleBegin = 0;
#endif
	constexpr uByte FrameStatsKey = DIK_F1;
	static_assert(std::size(DebugToggles) < 32, "m_PrevToggleKeys holds one bit per toggle");

	// Non-toggle shortcuts listed in the (?) hotkey help.
	const std::pair<const Char*, const Char*> EditorShortcuts[] =
	{
		{ "Ctrl+Z / Ctrl+Y", "Undo / Redo" },
		{ "Ctrl+D", "Duplicate selection" },
		{ "Delete", "Delete selection" },
		{ "F", "Focus selection (Scene View)" },
		{ "W A S D", "Move Scene camera" },
		{ "RMB drag", "Look around (Scene camera)" },
		{ "Wheel", "Zoom (Scene camera)" },
	};

}


MenuBar::MenuBar() {}

HRESULT MenuBar::Initialize() {
    m_Enable = false;
    return EditorObject::Initialize();
}
void MenuBar::Update(Bool isResize)
{
    Update_HotKey();
	// The spawn queue was flushed at the start of this frame, so last frame's layout lights exist now.
	if (m_LayoutLightsApplyPending)
	{
		m_LayoutLightsApplyPending = false;
		Spawn_LayoutLights(false);
	}
	EditorObject::Update(isResize);
}
void MenuBar::Render(Bool isResize) {
  EditorObject::Render(isResize);
	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("Edit"))
		{
			if (ImGui::MenuItem("Undo", "Ctrl+Z", false, EDITOR->Can_Undo()))
				EDITOR->Queue_Undo();
			if (ImGui::MenuItem("Redo", "Ctrl+Y", false, EDITOR->Can_Redo()))
				EDITOR->Queue_Redo();
			ImGui::EndMenu();
		}

        Render_Debug(); // Debug Rays
		Render_Prefab();

        if (ImGui::BeginMenu("WindowSetting")) {
          ImGui::Separator();

          Bool showModelViewer = EDITOR->Get_ModelViewer()->Is_Enabled();
          if (ImGui::MenuItem("Model Viewer", nullptr, showModelViewer))
              EDITOR->Get_ModelViewer()->Set_Enable(!showModelViewer);

		  Bool showAnimationPreset = EDITOR->Get_AnimationPresetEditor()->Is_Enabled();
		  if (ImGui::MenuItem("Animation Preset", nullptr, showAnimationPreset))
			  EDITOR->Get_AnimationPresetEditor()->Set_Enable(!showAnimationPreset);

          Bool showNavHelper = EDITOR->Get_NavHelper()->Is_Enabled();
          if (ImGui::MenuItem("NavMesh Builder", nullptr, showNavHelper))
              EDITOR->Get_NavHelper()->Set_Enable(!showNavHelper);

          ImGui::MenuItem("Project Settings", nullptr, &m_ShowProjectSettings);

          ImGui::EndMenu();
        }


    // ── SceneData 메뉴 ─────────────────────────────────────────────────
    if (ImGui::BeginMenu("LevelData"))
    {
		Bool isStop = (EDITOR->Get_State() == EDITOR_STATE::STOP);

        uint32 displayIndex = GAME_INSTANCE->Get_CurrentLevelIndex();
        if (displayIndex == 1) // 로딩 중이면 대상 레벨 표시
            displayIndex = EDITOR->Get_EngineDesc().startLevel;

        string saveLabel = "Save (" + Helper::To_String(std::filesystem::path(PATH.GetLevelDataPath(displayIndex)).filename().wstring()) + ")";
        if (ImGui::MenuItem(saveLabel.c_str(), nullptr, false, isStop) && EDITOR->Can_SaveLevel())
        {
          const wstring levelDataPath = PATH.GetLevelDataPath(displayIndex);
          /* Never overwrite authored scene data without a recoverable copy. */
          if (!AuthoringBackup::Capture(levelDataPath))
            LOG_ERROR(L"Level save aborted: could not back up the existing scene.");
          else if (SUCCEEDED(GAME_INSTANCE->SerializeLevel(GAME_INSTANCE->Get_CurrentLevelIndex(), levelDataPath)))
            LOG_INFO("Level saved successfully.");
          else
            LOG_ERROR(L"Level save failed.");
        }

        string loadLabel = "Load (" + Helper::To_String(std::filesystem::path(PATH.GetLevelDataPath(displayIndex)).filename().wstring()) + ")";
        if (ImGui::MenuItem(loadLabel.c_str(), nullptr, false, isStop))
        {
          if (SUCCEEDED(GAME_INSTANCE->DeSerializeLevel(PATH.GetLevelDataPath(displayIndex))))
		  {
			EDITOR->Clear_History();
			EDITOR->Clear_SelectedObject();
			LOG_INFO("Level loaded successfully.");
		  }
          else
            LOG_ERROR(L"Level load failed.");
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Spawn Layout Lights", nullptr, false, isStop))
          Spawn_LayoutLights(true);

        if (!isStop)
        {
          ImGui::Separator();
          ImGui::TextDisabled("(Stop editor to Save/Load)");
        }

        ImGui::EndMenu();
    }

    // ── Level Select 메뉴 ─────────────────────────────────────────────────
    Bool isStop = (EDITOR->Get_State() == EDITOR_STATE::STOP);
    if (ImGui::BeginMenu("Level Select", isStop))
    {
        Bool isAutoLoad = EDITOR->Is_AutoLoadEnabled();
        if (ImGui::Checkbox("Auto-Load Data", &isAutoLoad))
            EDITOR->Set_AutoLoadEnabled(isAutoLoad);
        
        ImGui::Separator();

        uint32 levelCount = ClientSettingManager::GetInstance()->Get_LevelCount();
        for (uint32 i = 1; i < levelCount; ++i)
        {
            string levelName = "Level " + std::to_string(i);
            if (i == 1) levelName += " (Loading)";
            else if (i == 2) levelName += " (Title)";
            else if (i == 3) levelName += " (Gameplay)";
            else if (i == 4) levelName += " (Amusement Park)";

            if (ImGui::MenuItem(levelName.c_str()))
            {
                EDITOR->Request_Reset(i);
            }
        }
        ImGui::EndMenu();
    }
    if (m_ShowFrameStats)
    {
        // 에디터 전체 프레임(모든 View 렌더 + ImGui) 기준. ImGui가 최근 프레임을 평균낸다.
        // draw·삼각형 수는 직전 프레임에 그린 모든 View(Game + Scene)의 합이다.
        const Float fps = ImGui::GetIO().Framerate;
        Char frameText[240]{};
#ifdef _DEBUG
        snprintf(frameText, sizeof(frameText), "%.0f FPS  %.2f ms  |  %u draws  %.2fM tris  |  cull %s %u  |  lod %s %u  |  shadow %s (all views)",
            fps, fps > 0.f ? 1000.f / fps : 0.f, GAME_INSTANCE->Get_FrameDrawCount(),
            static_cast<double>(GAME_INSTANCE->Get_FrameTriangleCount()) / 1000000.0,
            GAME_INSTANCE->Get_FrustumCulling() ? "on" : "off", GAME_INSTANCE->Get_FrameCulledCount(),
            GAME_INSTANCE->Get_WorldLod() ? "on" : "off", GAME_INSTANCE->Get_FrameLodTileCount(),
            GAME_INSTANCE->Get_Shadow() ? "on" : "off");
#else
        // Draw, triangle, cull and LOD counters are Debug-only; Release shows the frame time and switches.
        snprintf(frameText, sizeof(frameText), "%.0f FPS  %.2f ms  |  cull %s  |  lod %s  |  shadow %s",
            fps, fps > 0.f ? 1000.f / fps : 0.f, GAME_INSTANCE->Get_FrustumCulling() ? "on" : "off",
            GAME_INSTANCE->Get_WorldLod() ? "on" : "off", GAME_INSTANCE->Get_Shadow() ? "on" : "off");
#endif
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 70.0f - ImGui::CalcTextSize(frameText).x);
        ImGui::TextUnformatted(frameText);
    }

    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 60.0f);
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
        Render_HotKeyHelp();

    Float rightAlignPos = ImGui::GetWindowWidth() - 30.0f;
    ImGui::SetCursorPosX(rightAlignPos);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));

    // X 버튼 클릭 시 프로세스 종료 메시지 호출
    if (ImGui::MenuItem("X")) {
        
        if (GAME_INSTANCE && GAME_INSTANCE->Get_Device() && GAME_INSTANCE->Get_SwapChain()) {
            GAME_INSTANCE->Get_SwapChain()->SetFullscreenState(FALSE, nullptr);
        }

        PostQuitMessage(0); // Win32 응용 프로그램 안전 종료
    }

    ImGui::PopStyleColor();

    ImGui::EndMainMenuBar();
	}

    //??
    if (m_ShowProjectSettings)
    {
        if (ImGui::Begin("Project Settings", &m_ShowProjectSettings))
        {
            auto layerReg = GAME_INSTANCE->Get_LayerRegister();
            auto tagReg = GAME_INSTANCE->Get_TagRegister();
            // 1. 저장 버튼 (수정 내용을 곧바로 Json에 쓰고 터미널에 띄움)
            if (ImGui::Button("Save Configuration")) {
                layerReg->SaveToFile(PATH.GetLayerSettingsPath());
                if (tagReg) tagReg->SaveToFile(PATH.GetTagSettingsPath());

                LOG_INFO("Project Settings (Layers & Tags & Global Matrix) Saved to JSON.");
            }
            ImGui::SameLine(); ImGui::TextDisabled("<- Manually Save to disk");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            if (ImGui::CollapsingHeader("Global Collision Matrix", ImGuiTreeNodeFlags_DefaultOpen))
            {
                // 32개의 레이어를 모두 표시 (이름 지정 여부 상관없음)
                std::vector<std::pair<uint32, string>> activeLayers;
                for (int i = 0; i < 32; ++i) {
                    uint32 bit = (1 << i);
                    string name = Helper::To_String(layerReg->Get_LayerName(bit));
                    activeLayers.push_back({ bit, name });
                }
                size_t layerCount = activeLayers.size();

                // 표(Table) UI 생성
                if (layerCount > 0 && ImGui::BeginTable("CollisionMatrixTable", static_cast<int32>(layerCount + 1), ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit))
                {
                    ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextDisabled("/");
                    // 상단 가로축 헤더
                    for (size_t i = 0; i < layerCount; ++i) {
                        ImGui::TableSetColumnIndex(static_cast<int32>(i + 1));
                        ImGui::Text(activeLayers[layerCount - 1 - i].second.substr(0, 3).c_str());
                    }
                    // 행 그리기 (계단식)
                    for (size_t row = 0; row < layerCount; ++row)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text(activeLayers[row].second.c_str()); // 좌측 세로축 헤더
                        uint32 rowBit = activeLayers[row].first;
                        for (size_t col = 0; col < layerCount - row; ++col)
                        {
                            ImGui::TableSetColumnIndex(static_cast<int32>(col + 1));
                            uint32 colBit = activeLayers[layerCount - 1 - col].first;

                            bool isColliding = (layerReg->Get_GlobalMask(rowBit) & colBit) != 0;
                            ImGui::PushID(static_cast<int32>(row * 100 + col));
                            if (ImGui::Checkbox("##col_mat", &isColliding))
                            {
                                // 상호 양방향 충돌 스위치 동시 처리
                                layerReg->Toggle_GlobalMask_Symmetric(rowBit, colBit);
                            }
                            if (ImGui::IsItemHovered())
                            {
                                string rowName = activeLayers[row].second;
                                string colName = activeLayers[layerCount - 1 - col].second;
                                ImGui::SetTooltip("%s vs %s", rowName.c_str(), colName.c_str());
                            }
                            ImGui::PopID();
                        }
                    }
                    ImGui::EndTable();
                }
            }
        }
        ImGui::End();
    }
}

/* One PointLight per NieR light source on each world tile of the current level, read from
   Models/World/<Tile>/<Tile>_Lights.json (written by the offline layout-light script): street lamp
   heads from the tile's Layout.lay and the area's GAD room lights, with their color and range. Each light goes
   through the Editor spawn queue as a child of its tile at the tile-local position, so it follows the tile, undoes
   like any spawn and saves with the scene. A light that already has a PointLight child at its spot (or straight above
   or below it) is not spawned again; that child gets the listed Position/Color/Range/Glow through the Inspector's
   write path instead (the next frame for lights spawned now, m_LayoutLightsApplyPending), so running it again after
   regenerating the list moves and retunes them, and extra copies at the same spot are destroyed.
   Glow (the bulb sprite) is listed only for lamps whose head mesh is in the tile; a light without it gets 0. */
void MenuBar::Spawn_LayoutLights(Bool spawnMissing)
{
	const uint32 levelIndex = GAME_INSTANCE->Get_CurrentLevelIndex();
	uint32 queued = 0;
	uint32 written = 0;
	uint32 removed = 0;
	for (const auto& [id, object] : GAME_INSTANCE->Get_GameObjects(levelIndex))
	{
		if (!object || object->Has_Parent() || !dynamic_pointer_cast<Client::WorldMap>(object))
			continue;
		const Shared<Model> model = object->Get_Component<Model>();
		if (!model)
			continue;

		const wstring& tag = model->Get_ModelTag();
		const filesystem::path path = filesystem::path(PATH.GetResourceDir()) / L"Models" / L"World" / tag / (tag + L"_Lights.json");
		ifstream file(path);
		if (!file.is_open())
			continue;
		const nlohmann::json document = nlohmann::json::parse(file, nullptr, false);
		if (document.is_discarded() || !document.contains("lights") || !document["lights"].is_array())
		{
			LOG_WARN(L"[LayoutLights] Unreadable light list: {}", path.wstring());
			continue;
		}
		// A listed light claims the nearest PointLight child in its column (same spot seen from above, height within
		// LayoutLightReach), so a list that only moved a light up or down moves that child instead of adding one.
		// Other children in a claimed column are copies left by an older run and are destroyed.
		constexpr Float LayoutLightColumn = 0.3f;
		constexpr Float LayoutLightReach = 1.f;
		vector<Shared<Client::PointLight>> claimed;
		vector<Shared<Client::PointLight>> crowded;
		for (const auto& light : document["lights"])
		{
			const auto& position = light["position"];
			const Vector3 local{ position[0].get<Float>(), position[1].get<Float>(), position[2].get<Float>() };
			Shared<Client::PointLight> existing{ nullptr };
			Float nearest = numeric_limits<Float>::max();
			for (const Shared<GameObject>& child : object->Get_Children())
			{
				const auto pointLight = dynamic_pointer_cast<Client::PointLight>(child);
				if (!pointLight)
					continue;
				const Vector3 offset = pointLight->Get_Transform()->Get_LocalPosition() - local;
				if (offset.x * offset.x + offset.z * offset.z > LayoutLightColumn * LayoutLightColumn || std::abs(offset.y) > LayoutLightReach)
					continue;
				crowded.push_back(pointLight);
				if (std::find(claimed.begin(), claimed.end(), pointLight) == claimed.end() && offset.LengthSquared() < nearest)
				{
					nearest = offset.LengthSquared();
					existing = pointLight;
				}
			}
			if (existing)
				claimed.push_back(existing);
			if (!existing)
			{
				if (spawnMissing)
				{
					EDITOR->Queue_Spawn(L"PointLight", levelIndex, object->Get_ObjectGuid(), numeric_limits<size_t>::max(), local);
					m_LayoutLightsApplyPending = true;
					++queued;
				}
				continue;
			}

			ReflectionValue before;
			ReflectionValue after;
			if (existing->Get_Transform()->Get_LocalPosition() != local)
			{
				before.data = existing->Get_Transform()->Get_LocalPosition();
				after.data = local;
				EDITOR->Queue_PropertyWrite(existing, *existing->Get_Transform(), "Position", before, after, true);
				++written;
			}
			if (light.contains("color"))
			{
				const auto& color = light["color"];
				const Color listed{ color[0].get<Float>(), color[1].get<Float>(), color[2].get<Float>(), 1.f };
				if (listed != existing->Get_Color())
				{
					before.data = existing->Get_Color();
					after.data = listed;
					EDITOR->Queue_PropertyWrite(existing, *existing, "Color", before, after, true);
					++written;
				}
			}
			if (light.contains("range") && light["range"].get<Float>() != existing->Get_Range())
			{
				before.data = existing->Get_Range();
				after.data = light["range"].get<Float>();
				EDITOR->Queue_PropertyWrite(existing, *existing, "Range", before, after, true);
				++written;
			}
			const Float glow = light.value("glow", 0.f);
			if (glow != existing->Get_Glow())
			{
				before.data = existing->Get_Glow();
				after.data = glow;
				EDITOR->Queue_PropertyWrite(existing, *existing, "Glow", before, after, true);
				++written;
			}
		}
		for (const Shared<Client::PointLight>& copy : crowded)
		{
			if (std::find(claimed.begin(), claimed.end(), copy) != claimed.end())
				continue;
			claimed.push_back(copy);
			EDITOR->Queue_Destroy(copy->Get_ObjectGuid(), levelIndex);
			++removed;
		}
	}
	LOG_INFO(L"[LayoutLights] Queued {} PointLights, {} Position/Color/Range/Glow writes and {} duplicate removals on level {}",
		queued, written, removed, levelIndex);
}

void MenuBar::Render_Prefab()
{
	const Bool isStop = EDITOR->Get_State() == EDITOR_STATE::STOP;
	if (!ImGui::BeginMenu("Prefab", isStop))
		return;

	ImGui::SetNextItemWidth(180.f);
	ImGui::InputTextWithHint("##PrefabName", "Prefab name", m_PrefabName,
		static_cast<size_t>(MAX_PATH));
	const Shared<GameObject> selectedRoot = EDITOR->Get_SelectedObject();
	const Bool canSave = selectedRoot && selectedRoot->Get_StableChildKey().empty() &&
		m_PrefabName[0] != '\0';
	if (ImGui::MenuItem("Save Selected", nullptr, false, canSave))
	{
		const filesystem::path inputPath(Helper::To_wString(m_PrefabName));
		filesystem::path fileName = inputPath.filename();
		if (fileName.empty() || fileName != inputPath)
		{
			LOG_ERROR("Prefab name must not contain a directory path.");
		}
		else
		{
			fileName.replace_extension(L".json");
			const filesystem::path filePath =
				filesystem::path(PATH.GetPrefabSettingsDir()) / fileName;
			PrefabGuid prefabGuid{};
			Bool registeredNewGuid = false;
			if (filesystem::exists(filePath))
			{
				if (FAILED(GAME_INSTANCE->Register_PrefabDocument(filePath.wstring(), prefabGuid)))
					LOG_ERROR(L"Existing Prefab document is invalid: {}", filePath.wstring());
			}
			else
			{
				prefabGuid = Create_PrefabGuid();
				registeredNewGuid = prefabGuid.Is_Valid() &&
					SUCCEEDED(GAME_INSTANCE->Register_Prefab(prefabGuid, filePath.wstring()));
			}

			if (!AuthoringBackup::Capture(filePath))
			{
				if (registeredNewGuid)
					GAME_INSTANCE->Unregister_Prefab(prefabGuid);
				LOG_ERROR(L"Prefab save aborted: could not back up {}", filePath.wstring());
			}
			else if (prefabGuid.Is_Valid() &&
				SUCCEEDED(GAME_INSTANCE->SerializePrefabDocument(prefabGuid, selectedRoot)))
				LOG_INFO(L"Prefab saved: {}", filePath.wstring());
			else
			{
				if (registeredNewGuid)
					GAME_INSTANCE->Unregister_Prefab(prefabGuid);
				LOG_ERROR(L"Prefab save failed: {}", filePath.wstring());
			}
		}
	}
	if (!selectedRoot)
		ImGui::TextDisabled("Select a hierarchy root to save.");
	else if (!selectedRoot->Get_StableChildKey().empty())
		ImGui::TextDisabled("Code-defined children cannot be Prefab roots.");

	ImGui::Separator();
	if (ImGui::BeginMenu("Instantiate"))
	{
		const vector<pair<PrefabGuid, wstring>> prefabDocuments =
			GAME_INSTANCE->Get_PrefabDocuments();
		if (prefabDocuments.empty())
			ImGui::TextDisabled("No Prefab documents.");
		for (const auto& [prefabGuid, prefabPathString] : prefabDocuments)
		{
			const filesystem::path prefabPath(prefabPathString);
			const string label = Helper::To_String(prefabPath.stem().wstring());
			if (!ImGui::MenuItem(label.c_str()))
				continue;

			Shared<GameObject> instantiatedRoot;
			if (SUCCEEDED(GAME_INSTANCE->DeSerializePrefabDocument(prefabGuid, instantiatedRoot)))
			{
				EDITOR->Set_SelectedObject(instantiatedRoot);
				LOG_INFO(L"Prefab instantiated: {}", prefabPath.wstring());
			}
			else
				LOG_ERROR(L"Prefab instantiate failed: {}", prefabPath.wstring());
		}
		ImGui::EndMenu();
	}

	ImGui::EndMenu();
}

void MenuBar::Update_HotKey()
{
	const ImGuiIO& io = ImGui::GetIO();
	if (!io.WantTextInput && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false))
		EDITOR->Queue_Undo();
	if (!io.WantTextInput && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false))
		EDITOR->Queue_Redo();
	if (!io.WantTextInput && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false))
	{
		const Shared<GameObject> selected = EDITOR->Get_SelectedObject();
		if (selected && EDITOR->Can_EditHierarchy(selected))
		{
			uint32 levelIndex = GAME_INSTANCE->Get_CurrentLevelIndex();
			if (!GAME_INSTANCE->Contains(levelIndex, selected->Get_ObjectGuid()))
				levelIndex = 0;
			EDITOR->Queue_Duplicate(selected->Get_ObjectGuid(), levelIndex);
		}
	}

    // Toggle on the press edge only, so holding a key does not flip it every frame. A key counts only with Ctrl
    // matching its entry, so Ctrl+F4 does not also flip F4.
    const Bool ctrlDown = ((GAME_INSTANCE->Get_DIKeyState(DIK_LCONTROL) | GAME_INSTANCE->Get_DIKeyState(DIK_RCONTROL)) & 0x80) != 0;
    for (uint32 i = 0; i < std::size(DebugToggles); ++i)
    {
        const Bool down = DebugToggles[i].ctrl == ctrlDown && (GAME_INSTANCE->Get_DIKeyState(DebugToggles[i].key) & 0x80) != 0;
        const uint32 bit = 1u << i;
        if (down && !(m_PrevToggleKeys & bit))
            DebugToggles[i].toggle();
        m_PrevToggleKeys = down ? (m_PrevToggleKeys | bit) : (m_PrevToggleKeys & ~bit);
    }

    const Bool frameStatsDown = !ctrlDown && (GAME_INSTANCE->Get_DIKeyState(FrameStatsKey) & 0x80) != 0;
    if (frameStatsDown && !m_PrevFrameStatsKey)
        m_ShowFrameStats = !m_ShowFrameStats;
    m_PrevFrameStatsKey = frameStatsDown;

}

void MenuBar::Render_HotKeyHelp() const
{
    ImGui::BeginTooltip();
    ImGui::TextUnformatted("Hotkeys");
    ImGui::Separator();
    if (ImGui::BeginTable("HotKeyToggles", 3, ImGuiTableFlags_SizingFixedFit))
    {
        const auto row = [](const Char* key, const Char* label, Bool on)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(key);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(label);
            ImGui::TableNextColumn();
            ImGui::TextColored(on ? ImVec4(0.4f, 1.f, 0.4f, 1.f) : ImVec4(0.6f, 0.6f, 0.6f, 1.f), on ? "on" : "off");
        };
        row("F1", "Frame Stats", m_ShowFrameStats);
        for (const DEBUG_TOGGLE& toggle : DebugToggles)
            row(toggle.shortcut, toggle.label, toggle.get());
        ImGui::EndTable();
    }
    ImGui::Separator();
    if (ImGui::BeginTable("HotKeyShortcuts", 2, ImGuiTableFlags_SizingFixedFit))
    {
        for (const auto& [key, action] : EditorShortcuts)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(key);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(action);
        }
        ImGui::EndTable();
    }
    ImGui::EndTooltip();
}

void MenuBar::Render_Debug()
{
    if (ImGui::BeginMenu("Debug"))
    {
        ImGui::MenuItem("Frame Stats", "F1", &m_ShowFrameStats);
        ImGui::Separator();

        // 메뉴를 여는 것만으로 상태가 바뀌지 않도록 읽기와 토글을 나눈다.
        for (uint32 i = 0; i < std::size(DebugToggles); ++i)
        {
            if (i > 0 && (i == OptimizationToggleBegin || DebugToggles[i].ctrl != DebugToggles[i - 1].ctrl))
                ImGui::Separator();
            const DEBUG_TOGGLE& toggle = DebugToggles[i];
            if (ImGui::MenuItem(toggle.label, toggle.shortcut, toggle.get()))
                toggle.toggle();
        }

        // 실행 중에만 바뀐다. 마음에 드는 값은 Engine_Rendering_Const.h의 SSAO_* 상수로 옮긴다.
        ImGui::Separator();
        if (ImGui::BeginMenu("SSAO Tuning"))
        {
            Vector3 params = GAME_INSTANCE->Get_SSAOParams();
            Bool changed = ImGui::SliderFloat("Radius (m)", &params.x, 0.1f, 5.f, "%.2f");
            changed |= ImGui::SliderFloat("Intensity", &params.y, 0.f, 4.f, "%.2f");
            changed |= ImGui::SliderFloat("Bias (m)", &params.z, 0.f, 0.2f, "%.3f");
            if (ImGui::Button("Reset"))
            {
                params = Vector3{ SSAO_RADIUS, SSAO_INTENSITY, SSAO_BIAS };
                changed = true;
            }
            if (changed)
                GAME_INSTANCE->Set_SSAOParams(params);
            ImGui::EndMenu();
        }

        // 실행 중에만 바뀐다. 마음에 드는 값은 Engine_Rendering_Const.h의 VOLUMETRIC_* 상수로 옮긴다.
        if (ImGui::BeginMenu("Volumetric Tuning"))
        {
            Vector3 params = GAME_INSTANCE->Get_VolumetricParams();
            Bool changed = ImGui::SliderFloat("Intensity", &params.x, 0.f, 2.f, "%.2f");
            changed |= ImGui::SliderFloat("Density (1/m)", &params.y, 0.f, 0.05f, "%.4f");
            changed |= ImGui::SliderFloat("Distance (m)", &params.z, 10.f, 300.f, "%.0f");
            if (ImGui::Button("Reset"))
            {
                params = Vector3{ VOLUMETRIC_INTENSITY, VOLUMETRIC_DENSITY, VOLUMETRIC_DISTANCE };
                changed = true;
            }
            if (changed)
                GAME_INSTANCE->Set_VolumetricParams(params);
            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }
}

Shared<MenuBar> MenuBar::Create() {
  auto menuBar = make_shared<MenuBar>();

  if (FAILED(menuBar->Initialize())) {
    MSG_BOX("Failed To Create MenuBar");
    return nullptr;
  }

  return menuBar;
}
