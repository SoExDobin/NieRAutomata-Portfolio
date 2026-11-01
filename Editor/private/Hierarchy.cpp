#include "pch.h"
#include "Hierarchy.h"
#include "EditorManager.h"
#include "Game.h"
#include "GameObject.h"
#include "String_Helper.h"
#include "Engine_RTTR_Metadata.h"



using namespace Engine;

Hierarchy::Hierarchy() : EditorObject{} {}
Hierarchy::~Hierarchy() {}

HRESULT Hierarchy::Initialize() { return EditorObject::Initialize(); }

void Hierarchy::Update(Bool isResize) {
  if (!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
    Delete_Selected();
  }

  EditorObject::Update(isResize);
}

void Hierarchy::Render(Bool isResize) {
    ImGui::Begin("Hierarchy");

    // 1. Render Static Level (0)
    auto& staticObjects = GAME_INSTANCE->Get_GameObjects(0);
    for (auto &[id, pObj] : staticObjects) {
        if (!pObj->Has_Parent()) {
            Render_Node(pObj, 0);
        }
    }

    // 2. Render Current Level
    uint32 currentLevel = GAME_INSTANCE->Get_CurrentLevelIndex();
    if (currentLevel != 0) {
        auto& currentLevelObjects = GAME_INSTANCE->Get_GameObjects(currentLevel);
        for (auto &[id, pObj] : currentLevelObjects) {
            if (!pObj->Has_Parent()) {
                Render_Node(pObj, currentLevel);
            }
        }
    }

    // ── 드래그-드롭 수신 영역 ────────────────────────────────────────────
    // Hierarchy 창 전체 영역에 대해 드롭 수신 시도
    ImGui::Dummy(ImGui::GetContentRegionAvail());
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(Drag_PayLoadKey.c_str()))
        {
            const wchar_t *rawTag = static_cast<const wchar_t *>(payload->Data);
            wstring prototypeTag(rawTag);

            EDITOR->Queue_Spawn(prototypeTag, GAME_INSTANCE->Get_CurrentLevelIndex());
        }

        // Asset Browser items are "Type|Tag"; a Prefab one becomes a new root.
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ASSET_BROWSER_ITEM"))
        {
            const string item = static_cast<const Char *>(payload->Data);
            const string prefix = string(Asset_Type_Key::Prefab) + "|";
            if (item.starts_with(prefix))
                EDITOR->Queue_SpawnPrefab(Helper::To_wString(item.substr(prefix.size())),
                    GAME_INSTANCE->Get_CurrentLevelIndex());
        }
        
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(ObjectMove_PayLoadKey.c_str()))
        {
            const OBJECT_MOVE_PAYLOAD move = *static_cast<const OBJECT_MOVE_PAYLOAD*>(payload->Data);
            Shared<GameObject> draggedObj = GAME_INSTANCE->Find(move.objectGuid);
            if (draggedObj)
            {
                if (!EDITOR->Can_EditHierarchy(draggedObj))
                {
                    LOG_WARN(L"[Hierarchy] Code-defined child cannot be moved to Root: {}", draggedObj->Get_Name());
                }
                else
                {
                    EDITOR->Queue_MoveToRoot(move.objectGuid, move.levelIndex);
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::End();
}

void Hierarchy::Render_Node(const Shared<GameObject> &pObj, uint32 levelIndex) {
    const auto& children = pObj->Get_Children();
	const Bool canEditNode = EDITOR->Can_EditHierarchy(pObj);
	const Bool canEditChildren = EDITOR->Can_EditChildren(pObj);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

    if (children.empty())
      flags |= ImGuiTreeNodeFlags_Leaf;

    // 현재 선택된 오브젝트면 하이라이트
    Shared<GameObject> selected = EDITOR->Get_SelectedObject();
    if (selected && selected->Get_ObjectGuid() == pObj->Get_ObjectGuid())
      flags |= ImGuiTreeNodeFlags_Selected;

    // wstring → UTF-8 string 변환 (ImGui는 UTF-8 기준)
    const wstring &wName = pObj->Get_Name();
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wName.c_str(), -1, nullptr, 0,
                                      nullptr, nullptr);
    string name(utf8Len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wName.c_str(), -1, name.data(), utf8Len,
                        nullptr, nullptr);

    const string nodeId = To_String(pObj->Get_ObjectGuid());
    const Bool isRenaming = m_RenamingGuid.Is_Valid() && m_RenamingGuid == pObj->Get_ObjectGuid();
    bool bOpened = ImGui::TreeNodeEx(nodeId.c_str(), flags, "%s", isRenaming ? "" : name.c_str());

    if (isRenaming)
    {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        const Bool focusRequested = m_RenameFocusPending;
        if (focusRequested)
        {
            ImGui::SetKeyboardFocusHere();
            m_RenameFocusPending = false;
        }
        if (ImGui::InputText(("##Rename" + nodeId).c_str(), m_RenameBuffer, sizeof(m_RenameBuffer),
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
        {
            const int wideLen = MultiByteToWideChar(CP_UTF8, 0, m_RenameBuffer, -1, nullptr, 0);
            wstring newName(wideLen > 1 ? wideLen - 1 : 0, wchar_t{});
            if (wideLen > 1)
                MultiByteToWideChar(CP_UTF8, 0, m_RenameBuffer, -1, newName.data(), wideLen);
            EDITOR->Queue_Rename(pObj->Get_ObjectGuid(), levelIndex, newName);
            m_RenamingGuid = {};
        }
        else if (!focusRequested && (ImGui::IsKeyPressed(ImGuiKey_Escape) || !ImGui::IsItemActive()))
        {
            m_RenamingGuid = {};
        }
    }

	if (ImGui::BeginPopupContextItem())
	{
		EDITOR->Set_SelectedObject(pObj);
		if (ImGui::MenuItem("Rename", nullptr, false, canEditNode))
		{
			m_RenamingGuid = pObj->Get_ObjectGuid();
			strncpy_s(m_RenameBuffer, name.c_str(), _TRUNCATE);
			m_RenameFocusPending = true;
		}
		if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, canEditNode))
			EDITOR->Queue_Duplicate(pObj->Get_ObjectGuid(), levelIndex);

		const Shared<GameObject> parent = pObj->Get_Parent();
		size_t childIndex = numeric_limits<size_t>::max();
		if (parent)
			parent->Get_ChildIndex(pObj->Get_ObjectGuid(), childIndex);
		const Bool canReorder = parent && canEditNode && EDITOR->Can_EditChildren(parent) &&
			childIndex != numeric_limits<size_t>::max();
		if (ImGui::MenuItem("Move Up", nullptr, false, canReorder && childIndex > 0))
			EDITOR->Queue_Reorder(pObj->Get_ObjectGuid(), childIndex - 1, levelIndex);
		if (ImGui::MenuItem("Move Down", nullptr, false,
			canReorder && childIndex + 1 < parent->Get_Children().size()))
			EDITOR->Queue_Reorder(pObj->Get_ObjectGuid(), childIndex + 1, levelIndex);

		ImGui::Separator();
		if (ImGui::MenuItem("Delete", "Delete", false, canEditNode))
			EDITOR->Queue_Destroy(pObj->Get_ObjectGuid(), levelIndex);
		ImGui::EndPopup();
	}

    // ── 노드 단위 드롭 수신 (객체 이동) ───────────────────────────
    if (canEditNode && ImGui::BeginDragDropSource())
    {
        const OBJECT_MOVE_PAYLOAD payload{ pObj->Get_ObjectGuid(), levelIndex };
        ImGui::SetDragDropPayload(ObjectMove_PayLoadKey.c_str(), &payload, sizeof(payload));
        ImGui::Text("Moving: %s", name.c_str());
        ImGui::EndDragDropSource();
    }

    // ── 노드 단위 드롭 수신 (자식으로 생성) ───────────────────────────
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("PREFAB_DRAG"))
        {
            const wchar_t *rawTag = static_cast<const wchar_t *>(payload->Data);
            wstring prototypeTag(rawTag);

            if (!canEditChildren)
            {
                LOG_WARN(L"[Hierarchy] Cannot add Editor child to code-defined hierarchy: {}", pObj->Get_Name());
            }
            else
            {
                EDITOR->Queue_Spawn(prototypeTag, levelIndex, pObj->Get_ObjectGuid());
            }
        }

        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(ObjectMove_PayLoadKey.c_str()))
        {
            const OBJECT_MOVE_PAYLOAD move = *static_cast<const OBJECT_MOVE_PAYLOAD*>(payload->Data);
            Shared<GameObject> draggedObj = GAME_INSTANCE->Find(move.objectGuid);

            // 순환 참조 방지 로직 (자신이 부모가 되거나, 조상을 자신의 자식으로 넣는 경우 방지)
            if (draggedObj && draggedObj != pObj && move.levelIndex == levelIndex)
            {
                if (!EDITOR->Can_EditHierarchy(draggedObj))
                {
                    LOG_WARN(L"[Hierarchy] Code-defined child cannot be moved: {}", draggedObj->Get_Name());
                }
                else if (!canEditChildren)
                {
                    LOG_WARN(L"[Hierarchy] Cannot modify code-defined hierarchy: {}", pObj->Get_Name());
                }
                else
                {
                    EDITOR->Queue_Reparent(move.objectGuid, pObj->Get_ObjectGuid(), levelIndex);
                }
            }
        }

        ImGui::EndDragDropTarget();
    }

    // 클릭 감지 → Inspector 연동 (드래그 중에는 선택 변경 방지)
    if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
      EDITOR->Set_SelectedObject(pObj);
    }
    // Double-click frames the object in the Scene View, like Unreal's outliner.
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
      EDITOR->Set_SelectedObject(pObj);
      EDITOR->Focus_Object(pObj);
    }

    if (bOpened) {
      for (auto &child : children) {
        Render_Node(child, levelIndex);
      }
      ImGui::TreePop();
    }
}

void Hierarchy::Delete_Selected() {
    Shared<GameObject> selected = EDITOR->Get_SelectedObject();
    if (!selected)
      return;

    if (!EDITOR->Can_EditHierarchy(selected))
    {
      LOG_WARN(L"[Hierarchy] Code-defined child cannot be deleted: {}", selected->Get_Name());
      return;
    }

    uint32 levelIndex = GAME_INSTANCE->Get_CurrentLevelIndex();
    if (!GAME_INSTANCE->Contains(levelIndex, selected->Get_ObjectGuid()))
        levelIndex = 0;

    EDITOR->Queue_Destroy(selected->Get_ObjectGuid(), levelIndex);
}

Shared<Hierarchy> Hierarchy::Create() {
  auto hierarchy = make_shared<Hierarchy>();

  if (FAILED(hierarchy->Initialize())) {
    MSG_BOX("Failed to Create Hierarchy");
    return nullptr;
  }

  return hierarchy;
}
