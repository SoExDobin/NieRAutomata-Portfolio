#include "pch.h"
#include "EditorView.h"
#include "EditorManager.h"
#include "AuthoringBackup.h"
#include "PathManager.h"
#include "Game.h"
#include "UIObject.h"
#include "Texture.h"
#include "String_Helper.h"

void EditorView::Render_UIEditor()
{
    if (!ImGui::Begin("UI Editor")) { ImGui::End(); return; }
    const auto level = GAME_INSTANCE->Get_CurrentLevelIndex();
    const Bool editable = EDITOR->Get_State() == EDITOR_STATE::STOP && GAME_INSTANCE->LevelLoad_Finished();
    auto selected = std::dynamic_pointer_cast<UIObject>(EDITOR->Get_SelectedObject());
    ImGui::BeginDisabled(!editable);
    if (ImGui::Button("New Panel")) EDITOR->Queue_Spawn(L"UIPanel", level);
    ImGui::SameLine();
    ImGui::BeginDisabled(!selected || !EDITOR->Can_EditChildren(selected));
    if (ImGui::Button("Add Panel")) EDITOR->Queue_Spawn(L"UIPanel", level, selected->Get_ObjectGuid());
    ImGui::SameLine();
    if (ImGui::Button("Add Image")) EDITOR->Queue_Spawn(L"UIImage", level, selected->Get_ObjectGuid());
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Save Scene") && EDITOR->Can_SaveLevel()) {
        const auto path = PATH.GetLevelDataPath(level);
        if (AuthoringBackup::Capture(path) && SUCCEEDED(GAME_INSTANCE->SerializeLevel(level, path)))
            LOG_INFO("UI scene saved.");
        else LOG_ERROR("UI scene save failed.");
    }
    ImGui::EndDisabled();
    ImGui::TextDisabled("1920 x 1080 | Drag: move | Corner: size | Inspector: Anchor / Texture");
    ImGui::TextDisabled("Hierarchy: nesting / order | LevelData / Prefab menu: save and load");

    auto root = selected;
    while (root) {
        auto parent = std::dynamic_pointer_cast<UIObject>(root->Get_Parent());
        if (!parent) break;
        root = parent;
    }
    vector<Shared<UIObject>> roots;
    for (const auto& [id, object] : GAME_INSTANCE->Get_GameObjects(level)) {
        auto ui = std::dynamic_pointer_cast<UIObject>(object);
        if (ui && !ui->Has_Parent() && !ui->Is_Destroy()) roots.push_back(ui);
    }
    if (!root && !roots.empty()) root = roots.front();
    const string rootName = root ? Helper::To_String(root->Get_Name()) : "(Create a panel)";
    if (ImGui::BeginCombo("Panel", rootName.c_str())) {
        for (const auto& candidate : roots) {
            ImGui::PushID(candidate.get());
            if (ImGui::Selectable(Helper::To_String(candidate->Get_Name()).c_str(), candidate == root)) {
                EDITOR->Set_SelectedObject(candidate);
                selected = root = candidate;
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }

    const ImVec2 available = ImGui::GetContentRegionAvail();
    const Float zoom = std::min(available.x / 1920.f, available.y / 1080.f);
    if (zoom <= 0.f) { ImGui::End(); return; }
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 extent(1920.f * zoom, 1080.f * zoom);
    ImGui::InvisibleButton("##UICanvas", extent);
    const Bool hovered = ImGui::IsItemHovered();
    auto* draw = ImGui::GetWindowDrawList();
    const ImVec2 end(origin.x + extent.x, origin.y + extent.y);
    draw->AddRectFilled(origin, end, IM_COL32(30, 32, 36, 255));
    draw->PushClipRect(origin, end, true);
    for (int x = 0; x <= 1920; x += 120)
        draw->AddLine(ImVec2(origin.x + x * zoom, origin.y), ImVec2(origin.x + x * zoom, end.y), IM_COL32(48, 50, 54, 255));
    for (int y = 0; y <= 1080; y += 120)
        draw->AddLine(ImVec2(origin.x, origin.y + y * zoom), ImVec2(end.x, origin.y + y * zoom), IM_COL32(48, 50, 54, 255));

    vector<Shared<UIObject>> objects;
    const auto collect = [&](const auto& self, const Shared<GameObject>& object) -> void {
        if (!object || !object->Is_Active() || object->Is_Destroy()) return;
        if (auto ui = std::dynamic_pointer_cast<UIObject>(object)) objects.push_back(ui);
        for (const auto& child : object->Get_Children()) self(self, child);
    };
    collect(collect, root);
    const auto toScreen = [&](const Vector3& point) {
        return ImVec2(origin.x + (point.x + 960.f) * zoom, origin.y + (540.f - point.y) * zoom);
    };
    ImVec2 resizeHandle{};
    for (const auto& object : objects) {
        const Matrix world = object->Get_UIWorldMatrix(1920.f, 1080.f);
        const ImVec2 a = toScreen(Vector3::Transform(Vector3(-.5f, .5f, 0), world));
        const ImVec2 b = toScreen(Vector3::Transform(Vector3(.5f, .5f, 0), world));
        const ImVec2 c = toScreen(Vector3::Transform(Vector3(.5f, -.5f, 0), world));
        const ImVec2 d = toScreen(Vector3::Transform(Vector3(-.5f, -.5f, 0), world));
        const auto texture = object->Get_Component<Texture>();
        if (texture && !texture->Get_Textures().empty()) {
            const auto color = texture->Get_RGBAByValue();
            draw->AddImageQuad(reinterpret_cast<ImTextureID>(texture->Get_Textures().front().Get()),
                a, b, c, d, ImVec2(0, 0), ImVec2(1, 0), ImVec2(1, 1), ImVec2(0, 1),
                ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, color.w)));
        }
        const ImU32 border = object == selected ? IM_COL32(255, 190, 65, 255) : IM_COL32(100, 120, 135, 140);
        draw->AddQuad(a, b, c, d, border, object == selected ? 2.f : 1.f);
        if (object == selected) resizeHandle = c;
    }
    if (selected && editable) draw->AddRectFilled(ImVec2(resizeHandle.x - 5, resizeHandle.y - 5),
        ImVec2(resizeHandle.x + 5, resizeHandle.y + 5), IM_COL32(255, 190, 65, 255));
    draw->PopClipRect();

    const ImVec2 mouse = ImGui::GetMousePos();
    const Bool clicked = hovered && editable && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    if (clicked) {
        m_UIBeginGesture = true;
        m_UIResizing = selected && fabsf(mouse.x - resizeHandle.x) <= 7.f && fabsf(mouse.y - resizeHandle.y) <= 7.f;
        if (!m_UIResizing) {
            const Vector3 cursor((mouse.x - origin.x) / zoom - 960.f, 540.f - (mouse.y - origin.y) / zoom, 0);
            for (auto it = objects.rbegin(); it != objects.rend(); ++it) {
                if (fabsf((*it)->Get_SizeX()) < .001f || fabsf((*it)->Get_SizeY()) < .001f) continue;
                const auto local = Vector3::Transform(cursor, (*it)->Get_UIWorldMatrix(1920.f, 1080.f).Invert());
                if (fabsf(local.x) <= .5f && fabsf(local.y) <= .5f) {
                    selected = *it;
                    EDITOR->Set_SelectedObject(selected);
                    break;
                }
            }
        }
        m_UIDragging = selected && EDITOR->Can_EditHierarchy(selected);
    }
    if (!editable || !ImGui::IsMouseDown(ImGuiMouseButton_Left)) m_UIDragging = false;
    if (m_UIDragging && selected) {
        const auto delta = ImGui::GetIO().MouseDelta;
        // Click establishes selection; moving starts a fresh undo gesture on that frame.
        const auto transform = selected->Get_Transform();
        Vector3 motion(delta.x / zoom, -delta.y / zoom, 0);
        Vector3 before = m_UIResizing ? transform->Get_LocalScale() : transform->Get_LocalPosition();
        Vector3 after = before;
        if (m_UIResizing) {
            motion = Vector3::TransformNormal(motion, selected->Get_UIWorldMatrix(1920.f, 1080.f).Invert());
            after.x = std::max(1.f, before.x + 2.f * motion.x * before.x);
            after.y = std::max(1.f, before.y - 2.f * motion.y * before.y);
        } else {
            if (auto parent = std::dynamic_pointer_cast<UIObject>(selected->Get_Parent())) {
                const Matrix frame = Matrix::CreateScale(1.f / std::max(.001f, parent->Get_SizeX()),
                    1.f / std::max(.001f, parent->Get_SizeY()), 1.f) * parent->Get_UIWorldMatrix(1920.f, 1080.f);
                motion = Vector3::TransformNormal(motion, frame.Invert());
            }
            after.x += motion.x;
            after.y -= motion.y;
        }
        ReflectionValue from, to;
        from.data = before;
        to.data = clicked ? before : after;
        if (!clicked && (delta.x != 0.f || delta.y != 0.f)) {
            EDITOR->Queue_PropertyWrite(selected, *transform, m_UIResizing ? "Scale" : "Position", from, to, m_UIBeginGesture);
            m_UIBeginGesture = false;
        }
    }
    ImGui::End();
}
