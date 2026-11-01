#include "EditorWindows.h"
#include "Game.h"
#include "LayerRegistry.h"
#include "TagRegistry.h"
#include "pch.h"

void EditorWindows::RenderInspector() {
	
}

void EditorWindows::RenderLayerTagSettings() {
	ImGui::Begin("Layer & Tag Settings");
	{
		auto layerRegistry = GAME->Get_LayerRegister();
		auto tagRegistry = GAME->Get_TagRegister();

		if (ImGui::CollapsingHeader("Layers", ImGuiTreeNodeFlags_DefaultOpen)) {
			const auto &layers = layerRegistry->Get_AllLayers();
			for (const auto &[layer, name] : layers) {
				char buffer[256];
				wcstombs_s(nullptr, buffer, name.c_str(), sizeof(buffer));

				if (ImGui::InputText(std::to_string(ETOI(layer)).c_str(), buffer, sizeof(buffer))) {
					wchar_t wbuffer[256];
					mbstowcs_s(nullptr, wbuffer, buffer, sizeof(wbuffer));
					layerRegistry->Set_LayerName(layer, wbuffer);
				}
			}
		}

		if (ImGui::CollapsingHeader("Tags", ImGuiTreeNodeFlags_DefaultOpen)) {
			const auto &tags = tagRegistry->Get_AllTags();
			for (const auto &[tag, name] : tags) {
				char buffer[256];
				wcstombs_s(nullptr, buffer, name.c_str(), sizeof(buffer));

				if (ImGui::InputText(std::to_string(ETOI(tag)).c_str(), buffer,sizeof(buffer))) {
					wchar_t wbuffer[256];
					mbstowcs_s(nullptr, wbuffer, buffer, sizeof(wbuffer));
					tagRegistry->Set_TagName(tag, wbuffer);
				}
			}
		}
	}
	ImGui::End();
}
