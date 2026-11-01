#include "pch.h"
#include "InspectorTransform.h"

#include "EditorManager.h"
#include "Game.h"
#include "Transform.h"
#include "UIObject.h"

using namespace Editor;
using namespace Engine;

namespace
{
	Bool ReadVector3(Object& object, std::string_view propertyName, Vector3& outValue)
	{
		ReflectionValue reflectedValue;
		if (FAILED(GAME_INSTANCE->Read_ReflectedProperty(object, propertyName, reflectedValue)))
			return false;
		const Vector3* value = reflectedValue.Try_Get<Vector3>();
		if (!value)
			return false;
		outValue = *value;
		return true;
	}

	void QueueVector3(const Shared<GameObject>& owner, Object& object,
		std::string_view propertyName, const Vector3& before, const Vector3& after)
	{
		ReflectionValue beforeValue;
		ReflectionValue afterValue;
		beforeValue.data = before;
		afterValue.data = after;
		EDITOR->Queue_PropertyWrite(owner, object, propertyName,
			beforeValue, afterValue,
			ImGui::IsItemActivated() || !ImGui::IsItemActive());
	}
}

HRESULT InspectorTransform::Initialize()
{
    return S_OK;
}

void InspectorTransform::RenderComponent(const Shared<Transform>& transform)
{
	if (!transform) return;

    if (ImGui::CollapsingHeader("Transform Component", ImGuiTreeNodeFlags_DefaultOpen))
    {
		const Bool isUI = std::dynamic_pointer_cast<UIObject>(transform->Get_Owner()) != nullptr;
		Vector3 pos{}, rot{}, scale{};
		if (!ReadVector3(*transform, "Position", pos) ||
			!ReadVector3(*transform, "Rotation", rot) ||
			!ReadVector3(*transform, "Scale", scale)) {
			ImGui::TextDisabled("Transform reflection properties are unavailable.");
			return;
		}

        ImGui::Text("Position");
        ImGui::PushItemWidth(-1);
		const Vector3 beforePos = pos;
        if (ImGui::DragFloat3("##Pos", reinterpret_cast<Float*>(&pos), 0.1f)) {
			QueueVector3(transform->Get_Owner(), *transform, "Position", beforePos, pos);
        }
        ImGui::PopItemWidth();

        ImGui::Text("Rotation (Degrees)");
        ImGui::PushItemWidth(-1);
		const Vector3 beforeRot = rot;
        if (ImGui::DragFloat3("##Rot", reinterpret_cast<Float*>(&rot), 1.0f, 0.0f, 360.0f)) {
			QueueVector3(transform->Get_Owner(), *transform, "Rotation", beforeRot, rot);
        }
        ImGui::PopItemWidth();

        ImGui::Text(isUI ? "Size (pixels)" : "Scale");
        ImGui::PushItemWidth(-1);
		const Vector3 beforeScale = scale;
        if (isUI ? ImGui::DragFloat2("##Size", reinterpret_cast<Float*>(&scale), 1.f, 1.f, 16384.f)
            : ImGui::DragFloat3("##Scale", reinterpret_cast<Float*>(&scale), 0.05f, 0.001f, 100.0f)) {
			QueueVector3(transform->Get_Owner(), *transform, "Scale", beforeScale, scale);
        }
        ImGui::PopItemWidth();

        ImGui::Spacing();
    }
}

Shared<InspectorTransform> InspectorTransform::Create()
{
	auto instance = make_shared<InspectorTransform>();
	if (FAILED(instance->Initialize())) return nullptr;
	return instance;
}
