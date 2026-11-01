#include "pch.h"
#include "EditorView.h"
#include "ImGuizmo.h"
#include <Transform.h>
#include "UIObject.h"
#include "UIPanel.h"
#include "Game.h"
#include "EditorManager.h"
#include "PathManager.h"
#include "String_Helper.h"
#include "Engine_RTTR_Metadata.h"

namespace
{
	// Ctrl+F1 "MRT Preview" draws its tiles into the view texture in view pixels (Engine::RenderTargetDebugTiles);
	// name each tile on top of the ImGui image so it is clear which render target is which.
	void Draw_RenderTargetLabels(ID3D11ShaderResourceView* view, const ImVec2& imageMin, const ImVec2& imageSize)
	{
#ifdef _DEBUG
		if (!view || !GAME_INSTANCE->Get_RenderTargetDebug())
			return;
		ComPtr<ID3D11Resource> resource;
		ComPtr<ID3D11Texture2D> texture;
		view->GetResource(resource.GetAddressOf());
		D3D11_TEXTURE2D_DESC desc{};
		if (!resource || FAILED(resource.As(&texture)))
			return;
		texture->GetDesc(&desc);
		if (0 == desc.Width || 0 == desc.Height)
			return;
		const float scaleX = imageSize.x / static_cast<float>(desc.Width);
		const float scaleY = imageSize.y / static_cast<float>(desc.Height);
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const auto drawLabel = [&](const Engine::RenderTargetDebugTile& tile)
		{
			const ImVec2 min(imageMin.x + (tile.x - tile.size * 0.5f) * scaleX, imageMin.y + (tile.y - tile.size * 0.5f) * scaleY);
			const ImVec2 max(min.x + tile.size * scaleX, min.y + tile.size * scaleY);
			const float wrapWidth = (std::max)(max.x - min.x - 8.f, 16.f);
			// Smaller than the UI font so the label covers less of the tile.
			const float fontSize = ImGui::GetFontSize() * 0.75f;
			const ImVec2 textSize = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, wrapWidth, tile.label);
			drawList->PushClipRect(min, max, true);
			drawList->AddRect(min, max, IM_COL32(255, 255, 255, 110));
			drawList->AddRectFilled(min, ImVec2(min.x + textSize.x + 8.f, min.y + textSize.y + 4.f), IM_COL32(0, 0, 0, 180));
			drawList->AddText(nullptr, fontSize, ImVec2(min.x + 4.f, min.y + 2.f), IM_COL32(255, 255, 255, 255),
				tile.label, nullptr, wrapWidth);
			drawList->PopClipRect();
		};
		for (const Engine::RenderTargetDebugTile& tile : Engine::RenderTargetDebugTiles)
			drawLabel(tile);
		drawLabel(Engine::ShadowMapDebugTile);
#else
		(void)view; (void)imageMin; (void)imageSize;
#endif
	}
}

static ImGuizmo::OPERATION m_CurrentGizmoMode = ImGuizmo::TRANSLATE;

EditorView::EditorView() {}
EditorView::~EditorView() {}

HRESULT EditorView::Initialize()
{
	return EditorObject::Initialize();
}

void EditorView::Update(Bool isResize) {}

void EditorView::Render(Bool isResize) {
	// m_IsDirty 체크 제거하여 항상 렌더링되게 함 (기즈모/피킹 상태 반영 보장)
	EditorView::RenderView(isResize);
	Render_UIEditor();
}

void EditorView::RenderView(Bool isResize) {
	// One view: the editor camera while editing or paused (picking, gizmo, drops), the game camera while playing
	// (EditorManager::Render).
	auto srvView = GAME_INSTANCE->Get_OffScreenSRV(0);
	const Bool playing = EDITOR->Get_State() == EDITOR_STATE::PLAY;

	Bool isViewHovered = false;

	if (ImGui::Begin("Scene View"))
	{
		Bool loadFinished = GAME_INSTANCE->LevelLoad_Finished();
		Bool canPlay = (EDITOR->Get_State() == EDITOR_STATE::STOP);
		Bool isPlayDisabled = canPlay && loadFinished;
#ifdef _DEBUG
		// A scene capture run (EditorApp::Update_SceneCapture) may hold a scratch scene; Play saves the level file first.
		wchar_t captureTag[64]{};
		size_t captureLength = 0;
		if (0 == _wgetenv_s(&captureLength, captureTag, L"NIER_SCENE_CAPTURE") && 0 != captureTag[0])
			isPlayDisabled = false;
#endif
		if (!isPlayDisabled) ImGui::BeginDisabled();
		if (ImGui::Button("Play") && EDITOR->Can_SaveLevel()) {
			if (SUCCEEDED(GAME_INSTANCE->SerializeLevel(GAME_INSTANCE->Get_CurrentLevelIndex(), PATH.GetLevelDataPath(GAME_INSTANCE->Get_CurrentLevelIndex()))))
				EDITOR->Set_State(EDITOR_STATE::PLAY);
			else
				LOG_CRITICAL("Failed to Save Level Data");
		}
		if (!isPlayDisabled) ImGui::EndDisabled();
		ImGui::SameLine();
		Bool canPause = (EDITOR->Get_State() == EDITOR_STATE::PLAY ||
			EDITOR->Get_State() == EDITOR_STATE::PAUSE);
		if (!canPause) ImGui::BeginDisabled();
		if (ImGui::Button(EDITOR->Get_State() == EDITOR_STATE::PAUSE ? "Resume" : "Pause")) {
			if (EDITOR->Get_State() == EDITOR_STATE::PAUSE)
				EDITOR->Set_State(EDITOR_STATE::PLAY);
			else
				EDITOR->Set_State(EDITOR_STATE::PAUSE);
		}
		if (!canPause) ImGui::EndDisabled();
		ImGui::SameLine();
		Bool canStop = (EDITOR->Get_State() != EDITOR_STATE::STOP);
		if (!canStop) ImGui::BeginDisabled();
		if (ImGui::Button("Stop")) {
			EDITOR->Set_State(EDITOR_STATE::STOP);
		}
		if (!canStop) ImGui::EndDisabled();

		ImGui::SameLine();
		const Bool canStep = EDITOR->Get_State() == EDITOR_STATE::PAUSE;
		if (!canStep) ImGui::BeginDisabled();
		if (ImGui::Button("Step"))
			EDITOR->Request_SingleStep();
		if (!canStep) ImGui::EndDisabled();

		ImGui::SameLine();
		ImGui::Text(" | Gizmo: "); // 구분선
		ImGui::SameLine();
		// Translate 버튼
		{
			Bool isSelected = (m_CurrentGizmoMode == ImGuizmo::TRANSLATE);
			if (isSelected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 1.0f));
			if (ImGui::Button("T")) { m_CurrentGizmoMode = ImGuizmo::TRANSLATE; }
			if (isSelected) ImGui::PopStyleColor(); // 버튼 클릭 여부와 상관없이 Push했으면 무조건 Pop
			ImGui::SameLine();
		}
		// Rotate 버튼
		{
			Bool isSelected = (m_CurrentGizmoMode == ImGuizmo::ROTATE);
			if (isSelected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 1.0f));
			if (ImGui::Button("R")) { m_CurrentGizmoMode = ImGuizmo::ROTATE; }
			if (isSelected) ImGui::PopStyleColor();
			ImGui::SameLine();
		}
		// Scale 버튼
		{
			Bool isSelected = (m_CurrentGizmoMode == ImGuizmo::SCALE);
			if (isSelected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 1.0f));
			if (ImGui::Button("S")) { m_CurrentGizmoMode = ImGuizmo::SCALE; }
			if (isSelected) ImGui::PopStyleColor();
		}

		ImGui::Separator();

		auto selected = EDITOR->Get_SelectedObject();
		if (selected)
			ImGui::TextColored(ImVec4(1, 1, 0, 1), "Selected: %S", selected->Get_Name().c_str());
		else
			ImGui::Text("Selected: None");

		if (srvView && !isResize)
		{
			ImVec2 currentSize = ImGui::GetContentRegionAvail();
			EDITOR->RequestResize(currentSize.x, currentSize.y, 0);

			currentSize.x = std::max(1.f, floorf(currentSize.x));
			currentSize.y = std::max(1.f, floorf(currentSize.y));
			ImGui::Image(reinterpret_cast<ImTextureID>(srvView.Get()), currentSize);
			isViewHovered = ImGui::IsItemHovered();
			Draw_RenderTargetLabels(srvView.Get(), ImGui::GetItemRectMin(), ImGui::GetItemRectSize());

			// While playing, clicks and keys in the view are the game's.
			if (!playing)
			{
				// ── Scene View 드래그 앤 드롭 수신 ──────────────────────────────────
				if (ImGui::BeginDragDropTarget())
				{
					wstring prototypeTag, prefabName;
					if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(Drag_PayLoadKey.c_str()))
						prototypeTag = static_cast<const wchar_t *>(payload->Data);
					// Asset Browser items are "Type|Tag"; a Prefab one places that document.
					if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ASSET_BROWSER_ITEM"))
					{
						const string item = static_cast<const Char *>(payload->Data);
						const string prefix = string(Asset_Type_Key::Prefab) + "|";
						if (item.starts_with(prefix))
							prefabName = Helper::To_wString(item.substr(prefix.size()));
					}
					if (!prototypeTag.empty() || !prefabName.empty())
					{
						// 드롭된 마우스 위치 계산 (Viewport NDC 변환)
						ImVec2 mousePos = ImGui::GetMousePos();
						ImVec2 imageRectMin = ImGui::GetItemRectMin();
						ImVec2 imageRectSize = ImGui::GetItemRectSize();

						Float localX = mousePos.x - imageRectMin.x;
						Float localY = mousePos.y - imageRectMin.y;
						Float ndcX = (localX / imageRectSize.x) * 2.f - 1.f;
						Float ndcY = 1.f - (localY / imageRectSize.y) * 2.f;

						// 레이 생성
						Matrix invView = EDITOR->Get_SceneViewMatrix().Invert();
						Matrix invProj = EDITOR->Get_SceneProjMatrix().Invert();

						Vector4 vNear = Vector4(ndcX, ndcY, 0.f, 1.f);
						Vector4 vFar = Vector4(ndcX, ndcY, 1.f, 1.f);
						vNear = Vector4::Transform(vNear, invProj); 
						vNear /= vNear.w; 
						vNear = Vector4::Transform(vNear, invView);

						vFar = Vector4::Transform(vFar, invProj);   
						vFar /= vFar.w;   
						vFar = Vector4::Transform(vFar, invView);

						Vector3 rayOrigin = Vector3(vNear.x, vNear.y, vNear.z);
						Vector3 rayTarget = Vector3(vFar.x, vFar.y, vFar.z);
						Vector3 rayDir = rayTarget - rayOrigin;
						rayDir.Normalize();

						// 스폰 위치 결정: 지면(Y=0)과의 교점 또는 특정 거리 앞
						Vector3 spawnPos{};
						if (abs(rayDir.y) > 0.0001f)
						{
							Float t = -rayOrigin.y / rayDir.y;
							if (t > 0)
							{
								spawnPos = rayOrigin + rayDir * t;
							}
							else
							{
								spawnPos = rayOrigin + rayDir * 5.f;
							}
						}
						else
						{
							spawnPos = rayOrigin + rayDir * 5.f;
						}

						if (!prefabName.empty())
							EDITOR->Queue_SpawnPrefab(prefabName, GAME_INSTANCE->Get_CurrentLevelIndex(), spawnPos);
						else
							EDITOR->Queue_Spawn(prototypeTag,
								GAME_INSTANCE->Get_CurrentLevelIndex(), {},
								numeric_limits<size_t>::max(), spawnPos);
					}
					ImGui::EndDragDropTarget();
				}

				ImVec2 imageRectMin = ImGui::GetItemRectMin();
				ImVec2 imageRectSize = ImGui::GetItemRectSize();

				EditorView::MousePicking(imageRectSize, imageRectMin);
				Update_ImGuizmo(imageRectSize, imageRectMin);

				// F frames the selection, matching the Hierarchy double-click.
				if (isViewHovered && ImGui::IsKeyPressed(ImGuiKey_F, false) && selected)
					EDITOR->Focus_Object(selected);
			}
		}
	}
	ImGui::End();
	GAME_INSTANCE->Set_InputEnabled(playing
		? (isViewHovered || GAME_INSTANCE->Get_MouseLock())
		: isViewHovered);
}

void EditorView::MousePicking(ImVec2 viewport, ImVec2 imageStartPos)
{
	ImVec2 mousePos = ImGui::GetMousePos();
	
	// 마우스가 이미지 영역 안에 있는지 수동 체크
	Bool isHovered = (mousePos.x >= imageStartPos.x && mousePos.x <= imageStartPos.x + viewport.x &&
					  mousePos.y >= imageStartPos.y && mousePos.y <= imageStartPos.y + viewport.y);

	if (!isHovered) return;

	// 좌클릭 시에만 연산 수행
	if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		if (ImGuizmo::IsOver()) return;

		Float localX = mousePos.x - imageStartPos.x;
		Float localY = mousePos.y - imageStartPos.y;
		
		Float ndcX = (localX / viewport.x) * 2.f - 1.f;
		Float ndcY = 1.f - (localY / viewport.y) * 2.f;

		Matrix invView = EDITOR->Get_SceneViewMatrix().Invert();
		Matrix invProj = EDITOR->Get_SceneProjMatrix().Invert();

		Vector4 vNear = Vector4(ndcX, ndcY, 0.f, 1.f);
		Vector4 vFar = Vector4(ndcX, ndcY, 1.f, 1.f);
		
		vNear = Vector4::Transform(vNear, invProj);
		vNear /= vNear.w;
		vNear = Vector4::Transform(vNear, invView);
		vFar = Vector4::Transform(vFar, invProj);
		vFar /= vFar.w;   
		vFar = Vector4::Transform(vFar, invView);
		
		Vector3 rayOrigin = Vector3(vNear.x, vNear.y, vNear.z);
		Vector3 rayTarget = Vector3(vFar.x, vFar.y, vFar.z);
		Vector3 rayDir = rayTarget - rayOrigin;
		rayDir.Normalize();
		
		Shared<GameObject> pickedObject = nullptr;
		Float minDistance = FLT_MAX;
		
		// 모든 객체를 순회하며 피킹 검사
		for (auto& pair : GAME_INSTANCE->Get_GameObjects(GAME_INSTANCE->Get_CurrentLevelIndex())) {
			auto obj = pair.second;

			if (obj == EDITOR->Get_EditorCamera()) continue;

			auto uiObj = std::dynamic_pointer_cast<UIObject>(obj);
			if (uiObj)
			{
				uiObj->Update_UITransform(viewport.x, viewport.y);

				Matrix uiWorld = uiObj->Get_Transform()->Get_WorldMatrix();
				Matrix invUIWorld = uiWorld.Invert();

				Vector3 mousePos_Center;
				mousePos_Center.x = localX - viewport.x * 0.5f;
				mousePos_Center.y = -localY + viewport.y * 0.5f;
				mousePos_Center.z = 0.f;

				Vector3 localMouse = Vector3::Transform(mousePos_Center, invUIWorld);

				if (localMouse.x >= -0.5f && localMouse.x <= 0.5f &&
					localMouse.y >= -0.5f && localMouse.y <= 0.5f)
				{
					pickedObject = obj;
					minDistance = -1.f; 
					break;
				}
				continue;
			}

			// 3D 객체 피킹 (충돌체 없으면 간단한 Sphere 체크)
			Vector3 pos = obj->Get_Transform()->Get_Position();
			BoundingSphere sphere(pos, 1.0f); // 범위를 약간 넓힘 (0.5f -> 1.0f)
			float dist = 0.f;

			if (sphere.Intersects(rayOrigin, rayDir, dist)) {
				if (dist < minDistance) {
					minDistance = dist;
					pickedObject = obj;
				}
			}
		}
		
		if (pickedObject) {
			EDITOR->Set_SelectedObject(pickedObject);
		}
		else {
			EDITOR->Clear_SelectedObject();
		}
	}
}

void EditorView::Update_ImGuizmo(ImVec2 viewport, ImVec2 imageStartPos)
{
	if (EDITOR->Get_State() == EDITOR_STATE::PLAY) {
		m_WasUsingGizmo = false;
		return;
	}

	auto selectedObj = EDITOR->Get_SelectedObject();
	// Authored UI uses the reference-resolution canvas, not the scene camera gizmo.
	if (std::dynamic_pointer_cast<UIPanel>(selectedObj)) {
		m_WasUsingGizmo = false;
		return;
	}
	if (!selectedObj) {
		m_WasUsingGizmo = false;
		return;
	}
	auto transform = selectedObj->Get_Transform();
	const Bool beginGesture = !m_WasUsingGizmo;
	const auto QueueTransformValue = [&](std::string_view propertyName,
		const Vector3& before, const Vector3& after) {
		ReflectionValue beforeValue;
		ReflectionValue afterValue;
		beforeValue.data = before;
		afterValue.data = after;
		EDITOR->Queue_PropertyWrite(selectedObj, *transform, propertyName,
			beforeValue, afterValue, beginGesture);
	};

	// 1. 기즈모 드로잉 설정
	ImGuizmo::SetDrawlist(); // 기본값 사용 (현재 윈도우)
	ImGuizmo::SetRect(imageStartPos.x, imageStartPos.y, viewport.x, viewport.y);
	ImGuizmo::Enable(true);

	// 2. 행렬 준비 (SimpleMath::Matrix 사용)
	Matrix view = EDITOR->Get_SceneViewMatrix();
	Matrix proj = EDITOR->Get_SceneProjMatrix();
	
	// 3. 조작 처리
	auto uiObj = std::dynamic_pointer_cast<UIObject>(selectedObj);
	if (uiObj)
	{
		ImGuizmo::SetOrthographic(true);
		Matrix uiView = Matrix::Identity;
		Matrix uiProj = XMMatrixOrthographicOffCenterLH(0, viewport.x, viewport.y, 0, 0.f, 1.f);
		
		Vector2 anchorPos = uiObj->Get_AnchorPos(viewport.x, viewport.y);
		uiObj->Update_UITransform(viewport.x, viewport.y);

		Matrix uiScreenWorld = transform->Get_LocalMatrix() * Matrix::CreateTranslation(anchorPos.x, anchorPos.y, 0);

		if (ImGuizmo::Manipulate(
			reinterpret_cast<Float*>(&uiView),
			reinterpret_cast<Float*>(&uiProj),
			m_CurrentGizmoMode,
			ImGuizmo::LOCAL,
			reinterpret_cast<Float*>(&uiScreenWorld)))
		{
			Vector3 scale, pos;
			Quaternion rot;
			if (uiScreenWorld.Decompose(scale, rot, pos)) {
				if (m_CurrentGizmoMode == ImGuizmo::TRANSLATE)
					QueueTransformValue("Position", transform->Get_LocalPosition(),
						Vector3(pos.x - anchorPos.x, pos.y - anchorPos.y, 0.f));
				else if (m_CurrentGizmoMode == ImGuizmo::ROTATE) {
					const Vector3 radians = rot.ToEuler();
					QueueTransformValue("Rotation", transform->Get_LocalEulerAngles(),
						Vector3{ XMConvertToDegrees(radians.x), XMConvertToDegrees(radians.y),
							XMConvertToDegrees(radians.z) });
				}
				else if (m_CurrentGizmoMode == ImGuizmo::SCALE)
					QueueTransformValue("Scale", transform->Get_LocalScale(), scale);
			}
		}
	}
	else
	{
		ImGuizmo::SetOrthographic(false);
		transform->Update_WorldMatrix();
		Matrix world = transform->Get_WorldMatrix();
		Draw_ForwardAxis(world, view, proj, viewport, imageStartPos);

		if (ImGuizmo::Manipulate(
			reinterpret_cast<Float*>(&view),
			reinterpret_cast<Float*>(&proj),
			m_CurrentGizmoMode,
			ImGuizmo::WORLD,
			reinterpret_cast<Float*>(&world)))
		{
			// The gizmo edits the world matrix, but Position/Rotation/Scale are local values.
			Matrix local = world;
			if (const Shared<GameObject> parent = selectedObj->Get_Parent())
				local = world * parent->Get_Transform()->Get_WorldMatrix().Invert();

			Vector3 scale, pos;
			Quaternion rot;
			if (local.Decompose(scale, rot, pos)) {
				if (m_CurrentGizmoMode == ImGuizmo::TRANSLATE)
					QueueTransformValue("Position", transform->Get_LocalPosition(), pos);
				else if (m_CurrentGizmoMode == ImGuizmo::ROTATE) {
					const Vector3 radians = rot.ToEuler();
					QueueTransformValue("Rotation", transform->Get_LocalEulerAngles(),
						Vector3{ XMConvertToDegrees(radians.x), XMConvertToDegrees(radians.y),
							XMConvertToDegrees(radians.z) });
				}
				else if (m_CurrentGizmoMode == ImGuizmo::SCALE)
					QueueTransformValue("Scale", transform->Get_LocalScale(), scale);
			}
		}
	}

	m_WasUsingGizmo = ImGuizmo::IsUsing();
}

/* Draws the selected object's +Z (look) axis, the direction a DirectionalLight shines along,
   so rotations can be checked against the scene. Length stays constant on screen. */
void EditorView::Draw_ForwardAxis(const Matrix& world, const Matrix& view, const Matrix& proj,
	ImVec2 viewport, ImVec2 imageStartPos)
{
	const Vector3 origin = world.Translation();
	Vector3 look = world.Backward();
	if (look.LengthSquared() < 1e-8f)
		return;
	look.Normalize();

	const Vector3 cameraPosition = view.Invert().Translation();
	const Float length = std::max(0.01f, Vector3::Distance(cameraPosition, origin) * 0.2f);
	const Matrix viewProj = view * proj;

	const auto ToScreen = [&](const Vector3& point, ImVec2& outScreen) -> Bool {
		const Vector4 clip = Vector4::Transform(Vector4(point.x, point.y, point.z, 1.f), viewProj);
		if (clip.w <= 1e-4f)
			return false;
		outScreen.x = imageStartPos.x + (clip.x / clip.w * 0.5f + 0.5f) * viewport.x;
		outScreen.y = imageStartPos.y + (0.5f - clip.y / clip.w * 0.5f) * viewport.y;
		return true;
	};

	ImVec2 start, end;
	if (!ToScreen(origin, start) || !ToScreen(origin + look * length, end))
		return;

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ImU32 color = IM_COL32(255, 220, 0, 255);
	drawList->AddLine(start, end, color, 3.f);
	drawList->AddCircleFilled(end, 5.f, color);
}

Shared<EditorView> EditorView::Create()
{
	auto editorView = make_shared<EditorView>();

	if (FAILED(editorView->Initialize()))
	{
		MSG_BOX("Failed to Create EditorView");
		return nullptr;
	}

	return editorView;
}

