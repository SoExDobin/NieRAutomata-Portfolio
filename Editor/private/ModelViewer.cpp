#include "pch.h"
#include "ModelViewer.h"
#include "AnimationPresetEditor.h"
#include "Game.h"
#include "ResourceManager.h"
#include "Model.h"
#include "Shader.h"
#include "PathManager.h"

#include <imgui_stdlib.h>
#include <nlohmann/json.hpp>

#pragma comment(lib, "d3dcompiler.lib")

NS_BEGIN(Editor)

namespace
{
	string To_Utf8Path(const filesystem::path& path)
	{
		const std::u8string value = path.generic_u8string();
		return string{ value.begin(), value.end() };
	}

	const Char* TextureSlotForAssimpType(uint32 typeIndex)
	{
		switch (typeIndex)
		{
		case 1:
		case 12: return "g_AlbedoMap";
		case 4:
		case 14: return "g_EmissiveMap";
		case 6:
		case 13: return "g_NormalMap";
		case 8: return "g_OpacityMap";
		case 15: return "g_MaskMap"; // ModelConverter stores the NieR packed mask in METALNESS
		default: return nullptr;
		}
	}

	// Only values that differ from MaterialParameters defaults, so documents authored before the
	// parameters existed round-trip byte for byte and a preset shows just what it changes.
	nlohmann::json To_ParametersJson(const Engine::MaterialParameters& value)
	{
		const Engine::MaterialParameters defaults{};
		nlohmann::json out = nlohmann::json::object();
		// 6 decimals: 0.2f is written as 0.2 instead of 0.20000000298..., and reads back as the same float.
		auto readable = [](Float v) { return std::round(static_cast<double>(v) * 1e6) / 1e6; };
		auto vector4 = [&](const char* key, const Engine::Vector4& current, const Engine::Vector4& base) {
			if (current != base) out[key] = { readable(current.x), readable(current.y), readable(current.z), readable(current.w) };
		};
		auto number = [&](const char* key, Float current, Float base) {
			if (current != base) out[key] = readable(current);
		};
		auto flag = [&](const char* key, Bool current, Bool base) {
			if (current != base) out[key] = current;
		};
		vector4("baseColorTint", value.baseColorTint, defaults.baseColorTint);
		vector4("emissiveColor", value.emissiveColor, defaults.emissiveColor);
		number("emissiveIntensity", value.emissiveIntensity, defaults.emissiveIntensity);
		number("normalStrength", value.normalStrength, defaults.normalStrength);
		number("opacityMaskClipValue", value.opacityMaskClipValue, defaults.opacityMaskClipValue);
		number("aoScale", value.aoScale, defaults.aoScale);
		number("roughnessScale", value.roughnessScale, defaults.roughnessScale);
		number("metalnessScale", value.metalnessScale, defaults.metalnessScale);
		flag("metallicOverride", value.metallicOverride, defaults.metallicOverride);
		number("metallic", value.metallic, defaults.metallic);
		flag("roughnessOverride", value.roughnessOverride, defaults.roughnessOverride);
		number("roughness", value.roughness, defaults.roughness);
		flag("maskSrgb", value.maskSrgb, defaults.maskSrgb);
		if (value.detailNormalTileX != defaults.detailNormalTileX || value.detailNormalTileY != defaults.detailNormalTileY)
			out["detailNormalTile"] = { readable(value.detailNormalTileX), readable(value.detailNormalTileY) };
		number("detailNormalWeight", value.detailNormalWeight, defaults.detailNormalWeight);
		number("intensity", value.intensity, defaults.intensity);
		if (value.uvTileX != defaults.uvTileX || value.uvTileY != defaults.uvTileY)
			out["uvTile"] = { readable(value.uvTileX), readable(value.uvTileY) };
		if (value.uvTile2X != defaults.uvTile2X || value.uvTile2Y != defaults.uvTile2Y)
			out["uvTile2"] = { readable(value.uvTile2X), readable(value.uvTile2Y) };
		flag("useUV2", value.useUV2, defaults.useUV2);
		flag("unpremultiplyAlpha", value.unpremultiplyAlpha, defaults.unpremultiplyAlpha);
		return out;
	}

	Bool JsonStringArrayContains(const nlohmann::json& values, const string& target)
	{
		if (!values.is_array()) return false;
		return std::ranges::any_of(values, [&](const nlohmann::json& value) {
			return value.is_string() && value.get<string>() == target;
		});
	}

	// Loads once per resource path so the slot list can show the picture it points at.
	ComPtr<ID3D11ShaderResourceView> Find_TexturePreview(
		map<string, ComPtr<ID3D11ShaderResourceView>>& cache, const string& resourcePath)
	{
		if (resourcePath.empty() || resourcePath.find("..") != string::npos)
			return nullptr;
		if (const auto found = cache.find(resourcePath); found != cache.end())
			return found->second;

		ComPtr<ID3D11ShaderResourceView> srv;
		const filesystem::path path = filesystem::path(PATH.GetResourceDir()) /
			filesystem::path(std::u8string(resourcePath.begin(), resourcePath.end()));
		std::error_code error;
		if (filesystem::is_regular_file(path, error))
		{
			const ComPtr<ID3D11Device> device = GAME_INSTANCE->Get_Device();
			const HRESULT hr = _wcsicmp(path.extension().c_str(), L".dds") == 0
				? CreateDDSTextureFromFile(device.Get(), path.c_str(), nullptr, srv.GetAddressOf())
				: CreateWICTextureFromFile(device.Get(), path.c_str(), nullptr, srv.GetAddressOf());
			if (FAILED(hr)) srv.Reset();
		}
		cache.emplace(resourcePath, srv);
		return srv;
	}

	const wstring& DefaultShaderTag(const Shared<Engine::Model>& model)
	{
		return model->Is_Skeletal() ? Engine::VTXANIMMESH::Tag : Engine::VTXMESH::Tag;
	}

	// The shader whose passes and slots the materials use: the preset "shaderTag" (vertex tag + file variant,
	// e.g. VTX_Animation_Mesh_Weapon) or the model's default vertex shader.
	Shared<Engine::Shader> MaterialShader(const Shared<Engine::Model>& model, const string& shaderTag = {})
	{
		if (!model) return nullptr;
		return GAME_INSTANCE->Get_Shader(0,
			shaderTag.empty() ? DefaultShaderTag(model).c_str() : Helper::To_wString(shaderTag).c_str());
	}

	// "shaderTag" of a settings file before it is loaded, so the load validates against the right shader.
	string Read_ShaderTag(const filesystem::path& settingsPath)
	{
		try
		{
			ifstream in(settingsPath, ios::binary);
			nlohmann::json document;
			in >> document;
			return document.is_object() ? document.value("shaderTag", string{}) : string{};
		}
		catch (const std::exception&)
		{
			return {};
		}
	}

	// Typed or pasted paths: drop surrounding spaces/quotes and use '/' (the engine rejects '\').
	void Normalize_TexturePath(string& value)
	{
		std::ranges::replace(value, '\\', '/');
		const auto first = value.find_first_not_of(" \t\"'");
		const auto last = value.find_last_not_of(" \t\"'");
		value = first == string::npos ? string{} : value.substr(first, last - first + 1);
	}

	// Empty when the engine will accept the path; otherwise the reason shown under the slot.
	string Check_TexturePath(const string& resourcePath)
	{
		if (resourcePath.empty())
			return "Pick a texture or enter a path under resources (Models/<tag>/Textures/XXXX.dds).";
		if (resourcePath.find("..") != string::npos || resourcePath.front() == '/' || resourcePath.find(':') != string::npos)
			return "The path must be relative to resources, e.g. Models/pl0000/Textures/1DC3217F.dds.";
		std::error_code error;
		if (!filesystem::is_regular_file(filesystem::path(PATH.GetResourceDir()) /
			filesystem::path(std::u8string(resourcePath.begin(), resourcePath.end())), error))
			return "File not found under resources: " + resourcePath;
		return {};
	}

	// First slot that would make Prepare fail, as "material / slot: reason"; empty when all are usable.
	string Find_DraftTextureProblem(const Engine::MODEL_MATERIAL_SETTINGS& draft)
	{
		for (const Engine::MODEL_MATERIAL_SETTING& setting : draft.materials)
			for (const Engine::MATERIAL_TEXTURE_SETTING& texture : setting.textures)
				if (string reason = Check_TexturePath(texture.resourcePath); !reason.empty())
					return std::to_string(setting.materialIndex) + " " + setting.sourceMaterialName + " / " +
						texture.slotName + ": " + reason;
		return {};
	}

	// The settings document exactly as Save writes it (the preview path uses the same bytes).
	HRESULT Build_SettingsDocument(const Engine::MODEL_MATERIAL_SETTINGS& draft, nlohmann::json& document, string& status)
	{
		document = {
			{ "schemaVersion", 1 },
			{ "modelTag", draft.modelTag },
			{ "modelPath", draft.modelPath },
			{ "materials", nlohmann::json::array() }
		};
		if (!draft.shaderTag.empty())
			document["shaderTag"] = draft.shaderTag;
		for (const Engine::MODEL_MATERIAL_SETTING& setting : draft.materials)
		{
			nlohmann::json textures = nlohmann::json::object();
			for (const Engine::MATERIAL_TEXTURE_SETTING& texture : setting.textures)
			{
				if (texture.slotName.empty() || textures.contains(texture.slotName))
				{
					status = "Texture slot names must be non-empty and unique per material.";
					return E_INVALIDARG;
				}
				textures[texture.slotName] = texture.resourcePath;
			}
			document["materials"].push_back({
				{ "materialIndex", setting.materialIndex },
				{ "sourceMaterialName", setting.sourceMaterialName },
				{ "sourceShaderName", setting.sourceShaderName },
				{ "sourceTechniqueName", setting.sourceTechniqueName },
				{ "passName", setting.passName },
				{ "renderGroup", setting.renderGroup == RENDERGROUP::DECAL ? "DECAL" :
					setting.renderGroup == RENDERGROUP::BLEND ? "BLEND" : "NONBLEND" },
				{ "textures", std::move(textures) }
			});
			if (nlohmann::json parameters = To_ParametersJson(setting.parameters); !parameters.empty())
				document["materials"].back()["parameters"] = std::move(parameters);
		}
		return S_OK;
	}

	// ImGui DX11 backend vertex output; the pixel shader replaces ImGui's to show one channel opaque.
	constexpr char ChannelShaderSource[] = R"(
struct PS_INPUT { float4 pos : SV_POSITION; float4 col : COLOR0; float2 uv : TEXCOORD0; };
sampler sampler0;
Texture2D texture0;
float4 main(PS_INPUT input) : SV_Target
{
    const float4 color = texture0.Sample(sampler0, input.uv);
    return float4(color.CHANNEL, 1.f);
})";

	// Preview "Lit": the deferred lighting nodes (Node_SpecularGGX, Node_EnvironmentSpecular,
	// Node_CombineLighting) with the DirectionalLight defaults (color 1, ambient 0.2) and a camera headlight,
	// so the preview shows what DeferredShader will. Normal alpha 0 marks background.
	constexpr char LitShaderSource[] = R"(
#include "EngineShaderMaterial.hlsli"
Texture2D g_PreviewDiffuse : register(t0);
Texture2D g_PreviewNormal : register(t1);
Texture2D g_PreviewMaterial : register(t2);
cbuffer PreviewLight : register(b0)
{
    float4 g_PreviewToLight;
    float4 g_PreviewToEye;
};
struct VS_OUT { float4 position : SV_POSITION; };
VS_OUT VS_Main(uint id : SV_VertexID)
{
    VS_OUT output;
    const float2 uv = float2((id << 1) & 2, id & 2);
    output.position = float4(uv * float2(2.f, -2.f) + float2(-1.f, 1.f), 0.f, 1.f);
    return output;
}
float4 PS_Main(VS_OUT input) : SV_Target
{
    const int3 pixel = int3(input.position.xy, 0);
    const float4 packedNormal = g_PreviewNormal.Load(pixel);
    const float3 albedo = g_PreviewDiffuse.Load(pixel).rgb;
    if (packedNormal.a <= 0.f)
        return float4(albedo, 1.f);
    const float3 normal = normalize(packedNormal.xyz * 2.f - 1.f);
    const float4 material = g_PreviewMaterial.Load(pixel);
    const float3 toLight = normalize(g_PreviewToLight.xyz), toEye = normalize(g_PreviewToEye.xyz);
    const float3 ambient = float3(0.2f, 0.2f, 0.2f), lightColor = float3(1.f, 1.f, 1.f);
    const float3 shade = ambient + lightColor * saturate(dot(normal, toLight));
    const float3 specular = Node_SpecularGGX(normal, toLight, toEye, albedo, material.r, material.g) * lightColor +
        Node_EnvironmentSpecular(normal, toEye, albedo, material.r, material.g, ambient, lightColor);
    return float4(saturate(Node_CombineLighting(albedo, material.r, material.b, shade, specular)), 1.f);
})";

	// ImDrawList callback: UserCallbackData is the ID3D11PixelShader to use until ResetRenderState.
	void Set_ImGuiPixelShader(const ImDrawList*, const ImDrawCmd* command)
	{
		GAME_INSTANCE->Get_Context()->PSSetShader(static_cast<ID3D11PixelShader*>(command->UserCallbackData), nullptr, 0);
	}
}

ModelViewer::ModelViewer() : EditorObject{} 
{
}

HRESULT ModelViewer::Initialize() 
{
	m_Enable = false; // Initially closed
	return EditorObject::Initialize();
}

void ModelViewer::Update(Bool isResize) 
{
	if (!m_Enable) return;

	Handle_KeyInput();
	if (m_IsPlaying && m_pSelectedModel && m_pSelectedModel->Is_Skeletal())
		m_pSelectedModel->Update_ModelAnimation(ImGui::GetIO().DeltaTime * m_PlaySpeed);
	EditorObject::Update(isResize);
}

void ModelViewer::Render(Bool isResize) 
{
	if (!m_Enable) return;

	ImGui::SetNextWindowSize(ImVec2(1280.f, 860.f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Model Viewer", &m_Enable))
	{
		ImGui::Columns(2, "ModelViewerColumns");
		// The model list only needs names; give the material editor and preview the width.
		if (!m_ColumnsInitialized)
		{
			ImGui::SetColumnWidth(0, 220.f);
			m_ColumnsInitialized = true;
		}

		// Left: Model List
		View_ModelList();

		ImGui::NextColumn();
		
		// Right: the preview stays above the tabs so clips and materials are checked against the same view.
		View_PreviewPanel();
		if (ImGui::BeginTabBar("ModelViewerTabs"))
		{
			if (ImGui::BeginTabItem("Animations"))
			{
				View_AnimationList();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Materials"))
			{
				View_MaterialSettings();
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}

		ImGui::Columns(1);
	}
	ImGui::End();

	if (m_PreviewPoppedOut && m_pSelectedModel)
	{
		ImGui::SetNextWindowSize(ImVec2(760.f, 820.f), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Model Preview", &m_PreviewPoppedOut))
		{
			View_PreviewToolbar();
			const ImVec2 available = ImGui::GetContentRegionAvail();
			View_Preview(available.x, available.y);
		}
		ImGui::End();
	}
	View_TextureViewer();
}

void ModelViewer::View_ModelList() 
{
	ImGui::Text("Models");
	ImGui::Separator();

	if (ImGui::BeginChild("ModelListChild", ImVec2(0, 0), true))
	{
		uint32 currentLevel = GAME_INSTANCE->Get_CurrentLevelIndex();
		auto models = GAME_INSTANCE->Get_Models(currentLevel);

		for (auto &pModel : models)
		{
			if (!pModel)
				continue;

			const wstring &wName = pModel->Get_ModelTag();
			string utf8Name = Helper::To_String(wName);
			string displayTag = utf8Name.empty() ? "(unnamed)" : utf8Name;

			bool bSelected = (m_SelectedModelTag == wName);
			if (ImGui::Selectable(displayTag.c_str(), bSelected))
			{
				m_SelectedModelTag = wName;
				m_pSelectedModel = pModel;
				m_SelectedAnimIndex = 0;
				m_PreviewMeshIndex = -1;
				m_TexturePreviews.clear();
				m_ShowTextureViewer = false;
				Reset_PreviewView();
				Refresh_ModelTextureFiles();
				Load_MaterialDraft();
				Select_AnimationPreset();
			}
		}
	}
	ImGui::EndChild();
}

filesystem::path ModelViewer::Get_MaterialSettingsPath() const
{
	if (!m_pSelectedModel || m_pSelectedModel->Get_ModelTag().empty())
		return {};
	return filesystem::path(PATH.GetMaterialSettingsDir()) /
		Engine::Model::Make_MaterialSettingsFileName(m_pSelectedModel->Get_ModelTag());
}

void ModelViewer::Load_MaterialDraft()
{
	m_HasMaterialDraft = false;
	m_MaterialDraft = {};
	if (!m_pSelectedModel)
	{
		m_MaterialStatus = "Select a model first.";
		return;
	}

	const filesystem::path settingsPath = Get_MaterialSettingsPath();
	if (filesystem::is_regular_file(settingsPath))
	{
		if (FAILED(m_pSelectedModel->Load_MaterialSettings(
			settingsPath, m_pSelectedModel->Get_ModelTag(), MaterialShader(m_pSelectedModel, Read_ShaderTag(settingsPath)))))
		{
			m_MaterialStatus = "Material settings are invalid; the previous in-memory snapshot was preserved.";
			return;
		}
	}

	const Shared<const Engine::MODEL_MATERIAL_SETTINGS> settings =
		m_pSelectedModel->Get_MaterialSettings();
	if (!settings)
	{
		m_MaterialStatus = "No authored settings. Generate a draft from the Converter inspection JSON.";
		return;
	}

	m_MaterialDraft = *settings;
	m_HasMaterialDraft = true;
	m_MaterialStatus = "Loaded ModelTag material settings.";
	// The model already shows these settings; live preview starts from here.
	nlohmann::json document;
	string ignored;
	m_AppliedDraftDump = SUCCEEDED(Build_SettingsDocument(m_MaterialDraft, document, ignored)) ? document.dump(4) : string{};
	m_DraftPreviewStatus.clear();
}

void ModelViewer::Generate_MaterialDraft()
{
	m_HasMaterialDraft = false;
	m_MaterialDraft = {};
	if (!m_pSelectedModel)
	{
		m_MaterialStatus = "Select a model first.";
		return;
	}
	if (filesystem::is_regular_file(Get_MaterialSettingsPath()))
	{
		m_MaterialStatus = "An authored settings file already exists. Reload it instead of overwriting it from conversion data.";
		return;
	}

	filesystem::path converterPath = m_pSelectedModel->Get_ModelFilePath();
	converterPath.replace_extension(L".json");
	try
	{
		ifstream in(converterPath, ios::binary);
		if (!in.is_open())
		{
			m_MaterialStatus = "Converter inspection JSON was not found next to the model.";
			return;
		}
		nlohmann::json converter;
		in >> converter;
		if (!converter.contains("materials") || !converter["materials"].is_array())
		{
			m_MaterialStatus = "Converter inspection JSON has no material array.";
			return;
		}

		const vector<string> modelMaterialNames = m_pSelectedModel->Get_MaterialNames();
		if (converter["materials"].size() != modelMaterialNames.size())
		{
			m_MaterialStatus = "Converter material count does not match the loaded model.";
			return;
		}

		// Only slots the model shader declares; the Converter also reports OPACITY etc. that no pass samples.
		const auto shader = MaterialShader(m_pSelectedModel);
		Engine::MODEL_MATERIAL_SETTINGS draft;
		draft.modelTag = Helper::To_String(m_pSelectedModel->Get_ModelTag());
		draft.modelPath = m_pSelectedModel->Get_ModelResourcePath();
		draft.materials.reserve(modelMaterialNames.size());

		const nlohmann::json emptyArray = nlohmann::json::array();
		const nlohmann::json inspection = converter.value("inspection", nlohmann::json::object());
		const nlohmann::json reference = inspection.value("referenceMaterials", nlohmann::json::object());
		const nlohmann::json exactMatches = reference.value("exactMatches", emptyArray);
		const nlohmann::json duplicateConverter = inspection.value("duplicateConverterMaterialNames", emptyArray);
		const nlohmann::json duplicateSource = reference.value("duplicateSourceMaterialNames", emptyArray);
		const nlohmann::json converterOnly = reference.value("converterOnlyMaterials", emptyArray);

		for (size_t arrayIndex = 0; arrayIndex < converter["materials"].size(); ++arrayIndex)
		{
			const nlohmann::json& source = converter["materials"][arrayIndex];
			Engine::MODEL_MATERIAL_SETTING setting;
			setting.materialIndex = source.value("materialIndex", static_cast<uint32>(arrayIndex));
			setting.sourceMaterialName = source.value("name", string{});
			if (setting.materialIndex >= modelMaterialNames.size() ||
				setting.sourceMaterialName != modelMaterialNames[setting.materialIndex])
			{
				m_MaterialStatus = "Converter material identity does not match the loaded model.";
				return;
			}

			for (const nlohmann::json& match : exactMatches)
			{
				if (match.value("materialIndex", UINT32_MAX) != setting.materialIndex ||
					match.value("materialName", string{}) != setting.sourceMaterialName)
					continue;
				setting.sourceShaderName = match.value("sourceShaderName", string{});
				setting.sourceTechniqueName = match.value("sourceTechniqueName", string{});
				break;
			}

			const Bool unresolved = JsonStringArrayContains(duplicateConverter, setting.sourceMaterialName) ||
				JsonStringArrayContains(duplicateSource, setting.sourceMaterialName) ||
				JsonStringArrayContains(converterOnly, setting.sourceMaterialName);
			if (unresolved)
				setting.passName = "Unresolved";
			else
				// Source shader names are provenance, not implemented engine pass names.
				setting.passName = !shader || shader->Has_Pass("Default_Pass") || shader->Get_PassNames().empty()
					? "Default_Pass" : shader->Get_PassNames().front();

			if (source.contains("textures") && source["textures"].is_array())
			{
				for (const nlohmann::json& texture : source["textures"])
				{
					const Char* slotName = TextureSlotForAssimpType(texture.value("type", UINT32_MAX));
					const string importedPath = texture.value("path", string{});
					if (!slotName || importedPath.empty() || importedPath.find("..") != string::npos ||
						!shader || !shader->Has_SRV(slotName))
						continue;
					if (std::ranges::any_of(setting.textures, [&](const Engine::MATERIAL_TEXTURE_SETTING& value) {
						return value.slotName == slotName;
					}))
						continue;

					const filesystem::path relativeModelDirectory =
						filesystem::path(Helper::To_wString(draft.modelPath)).parent_path();
					filesystem::path relativeTexturePath =
						(relativeModelDirectory / filesystem::path(Helper::To_wString(importedPath))).lexically_normal();
					const filesystem::path fallbackPath = relativeModelDirectory / L"Textures" /
						filesystem::path(Helper::To_wString(importedPath)).filename();
					if (filesystem::is_regular_file(filesystem::path(PATH.GetResourceDir()) / fallbackPath))
						relativeTexturePath = fallbackPath;
					setting.textures.push_back({ slotName, To_Utf8Path(relativeTexturePath) });
				}
			}
			draft.materials.push_back(std::move(setting));
		}

		std::ranges::sort(draft.materials, {}, &Engine::MODEL_MATERIAL_SETTING::materialIndex);
		m_MaterialDraft = std::move(draft);
		m_HasMaterialDraft = true;
		m_MaterialStatus = "Generated an unsaved draft from Converter inspection data.";
	}
	catch (const std::exception& exception)
	{
		m_MaterialStatus = string("Failed to generate material draft: ") + exception.what();
	}
}

void ModelViewer::Save_MaterialDraft()
{
	if (!m_pSelectedModel || !m_HasMaterialDraft)
	{
		m_MaterialStatus = "There is no material draft to save.";
		return;
	}
	if (const string problem = Find_DraftTextureProblem(m_MaterialDraft); !problem.empty())
	{
		m_MaterialStatus = "Not saved; the existing settings file was preserved. " + problem;
		return;
	}
	if (SUCCEEDED(Save_MaterialSettings(m_pSelectedModel, m_MaterialDraft, Get_MaterialSettingsPath(), m_MaterialStatus)))
	{
		nlohmann::json document;
		string ignored;
		if (SUCCEEDED(Build_SettingsDocument(m_MaterialDraft, document, ignored)))
			m_AppliedDraftDump = document.dump(4);
		m_DraftPreviewStatus.clear();
	}
}

void ModelViewer::Refresh_ModelTextureFiles()
{
	m_ModelTextureFiles.clear();
	if (!m_pSelectedModel) return;
	const filesystem::path relativeDirectory =
		filesystem::path(Helper::To_wString(m_pSelectedModel->Get_ModelResourcePath())).parent_path() / L"Textures";
	std::error_code error;
	for (filesystem::directory_iterator it(filesystem::path(PATH.GetResourceDir()) / relativeDirectory, error), end;
		!error && it != end; it.increment(error))
	{
		const filesystem::path extension = it->path().extension();
		if (!it->is_regular_file(error) || (_wcsicmp(extension.c_str(), L".dds") != 0 &&
			_wcsicmp(extension.c_str(), L".png") != 0 && _wcsicmp(extension.c_str(), L".tga") != 0))
			continue;
		m_ModelTextureFiles.push_back(To_Utf8Path(relativeDirectory / it->path().filename()));
	}
	std::ranges::sort(m_ModelTextureFiles);
}

void ModelViewer::Apply_DraftPreview(Bool force)
{
	if (!m_pSelectedModel || !m_HasMaterialDraft)
		return;
	nlohmann::json document;
	string status;
	if (FAILED(Build_SettingsDocument(m_MaterialDraft, document, status)))
	{
		m_DraftPreviewStatus = "Preview not updated: " + status;
		return;
	}
	string dump = document.dump(4);
	if (!force && dump == m_AppliedDraftDump)
		return;
	m_AppliedDraftDump = dump;
	// A slot just added or a half-typed path would only make Prepare log errors; wait for a usable path.
	if (const string problem = Find_DraftTextureProblem(m_MaterialDraft); !problem.empty())
	{
		m_DraftPreviewStatus = "Preview waits for a valid texture path. " + problem;
		return;
	}

	// Prepare validates from a file, so the unsaved draft goes through a temp file outside ProjectSetting.
	std::error_code errorCode;
	const filesystem::path temporary = filesystem::temp_directory_path(errorCode) /
		(L"NieR_ModelViewer_" + m_pSelectedModel->Get_ModelTag() + L"_preview.json");
	{
		ofstream out(temporary, ios::binary | ios::trunc);
		out << dump;
		if (!out.good())
		{
			m_DraftPreviewStatus = "Preview not updated: the temp file could not be written.";
			return;
		}
	}
	const auto shader = MaterialShader(m_pSelectedModel, m_MaterialDraft.shaderTag);
	Shared<const Engine::MODEL_MATERIAL_SETTINGS> prepared;
	const Bool applied = shader &&
		m_pSelectedModel->Prepare_MaterialSettings(temporary, m_pSelectedModel->Get_ModelTag(), prepared, shader) == S_OK &&
		SUCCEEDED(m_pSelectedModel->Apply_MaterialSettings(prepared));
	filesystem::remove(temporary, errorCode);
	m_DraftPreviewStatus = applied
		? "Preview shows the unsaved draft. Save Atomically writes it to ProjectSetting."
		: "Preview not updated: the draft has an invalid pass, slot or texture path (red text below).";
}

HRESULT ModelViewer::Save_MaterialSettings(const Shared<Engine::Model>& model,
	const Engine::MODEL_MATERIAL_SETTINGS& draft, const filesystem::path& target, string& status)
{
	if (!model || target.empty())
	{
		status = "A model and target path are required.";
		return E_INVALIDARG;
	}

	nlohmann::json document;
	if (const HRESULT built = Build_SettingsDocument(draft, document, status); FAILED(built))
		return built;

	const filesystem::path temporary = target.wstring() + L".tmp";
	std::error_code errorCode;
	filesystem::create_directories(target.parent_path(), errorCode);
	if (errorCode)
	{
		status = "Failed to create the material settings directory.";
		return E_FAIL;
	}
	{
		ofstream out(temporary, ios::binary | ios::trunc);
		if (!out.is_open())
		{
			status = "Failed to open the temporary material settings file.";
			return E_FAIL;
		}
		out << document.dump(4);
		out.flush();
		if (!out.good())
		{
			status = "Failed while writing temporary material settings.";
			return E_FAIL;
		}
	}

	const auto shader = MaterialShader(model, draft.shaderTag);
	Shared<const Engine::MODEL_MATERIAL_SETTINGS> prepared;
	if (!shader || model->Prepare_MaterialSettings(
		temporary, model->Get_ModelTag(), prepared, shader) != S_OK)
	{
		filesystem::remove(temporary, errorCode);
		status = "Draft validation failed; the existing settings file was preserved.";
		return E_FAIL;
	}
	if (!MoveFileExW(temporary.c_str(), target.c_str(),
		MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
	{
		filesystem::remove(temporary, errorCode);
		status = "Failed to atomically replace the material settings file.";
		return E_FAIL;
	}
	if (FAILED(model->Apply_MaterialSettings(prepared)))
	{
		status = "Saved settings could not be applied to the preview model.";
		return E_FAIL;
	}
	status = "Material settings saved atomically and applied to the preview model.";
	return S_OK;
}

void ModelViewer::View_PreviewPanel()
{
	if (!m_pSelectedModel || !ImGui::CollapsingHeader("Preview", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	if (ImGui::Checkbox("Pop Out", &m_PreviewPoppedOut) && m_PreviewPoppedOut)
		ImGui::SetWindowFocus("Model Preview");
	if (m_PreviewPoppedOut)
	{
		ImGui::TextDisabled("Preview is shown in the \"Model Preview\" window.");
		return;
	}
	View_PreviewToolbar();
	View_Preview(ImGui::GetContentRegionAvail().x, m_PreviewHeightInline);
	// Drag the bar under the preview to resize it.
	ImGui::InvisibleButton("##PreviewSplitter", ImVec2(ImGui::GetContentRegionAvail().x, 8.f));
	if (ImGui::IsItemActive())
		m_PreviewHeightInline = std::clamp(m_PreviewHeightInline + ImGui::GetIO().MouseDelta.y, 160.f, 1600.f);
	if (ImGui::IsItemHovered() || ImGui::IsItemActive())
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
	const ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
	ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(min.x, min.y + 3.f), ImVec2(max.x, max.y - 3.f),
		ImGui::GetColorU32(ImGui::IsItemHovered() ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator));
}

void ModelViewer::View_MaterialSettings()
{
	ImGui::Text("ModelTag Material Settings");
	ImGui::Separator();
	if (!m_pSelectedModel)
	{
		ImGui::Text("No model selected.");
		return;
	}
	// Edits are applied once the widget is released, so a drag does not reload textures every frame.
	if (m_LiveDraftPreview && !ImGui::IsAnyItemActive())
		Apply_DraftPreview(false);

	ImGui::Checkbox("Live Draft", &m_LiveDraftPreview);
	ImGui::SetItemTooltip("Apply unsaved edits to the preview when a field is released.");
	ImGui::SameLine();
	if (ImGui::Button("Apply Draft Now")) Apply_DraftPreview(true);
	if (!m_DraftPreviewStatus.empty()) ImGui::TextWrapped("%s", m_DraftPreviewStatus.c_str());

	ImGui::TextWrapped("ModelTag: %s", Helper::To_String(m_pSelectedModel->Get_ModelTag()).c_str());
	ImGui::TextWrapped("Model: %s", m_pSelectedModel->Get_ModelResourcePath().c_str());
	if (m_HasMaterialDraft)
	{
		// Shaders of the model's vertex layout: the default file and its variants (Shader_VtxAnimMesh_Weapon.hlsl).
		const wstring& vertexTag = DefaultShaderTag(m_pSelectedModel);
		const string current = m_MaterialDraft.shaderTag.empty()
			? Helper::To_String(vertexTag) + " (default)" : m_MaterialDraft.shaderTag;
		if (ImGui::BeginCombo("Shader", current.c_str()))
		{
			for (const wstring& tag : GAME_INSTANCE->Get_ShaderTags(0))
			{
				if (tag.rfind(vertexTag, 0) != 0)
					continue;
				const string name = Helper::To_String(tag);
				const string value = tag == vertexTag ? string{} : name;
				if (ImGui::Selectable(name.c_str(), value == m_MaterialDraft.shaderTag))
					m_MaterialDraft.shaderTag = value;
			}
			ImGui::EndCombo();
		}
		ImGui::SetItemTooltip("Saved as \"shaderTag\". It must be the shader the game object creates (weapons: VTX_Animation_Mesh_Weapon).");
	}
	if (ImGui::Button("Preview All Meshes")) m_PreviewMeshIndex = -1;
	ImGui::SameLine();
	if (m_PreviewMeshIndex < 0) ImGui::TextUnformatted("All meshes");
	else ImGui::Text("Mesh %d only", m_PreviewMeshIndex);
	if (ImGui::CollapsingHeader("Mesh / Material Mapping", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const auto names = m_pSelectedModel->Get_MaterialNames();
		if (ImGui::BeginChild("MeshMaterialMapping", ImVec2(0, 160.f), true))
		{
			for (uint32 meshIndex = 0; meshIndex < m_pSelectedModel->Get_NumMeshes(); ++meshIndex)
			{
				string meshName;
				uint32 materialIndex{};
				if (FAILED(m_pSelectedModel->Get_MeshMaterialInfo(meshIndex, meshName, materialIndex))) continue;
				const string label = std::to_string(meshIndex) + ": " + meshName + " -> " +
					std::to_string(materialIndex) + " / " +
					(materialIndex < names.size() ? names[materialIndex] : "INVALID") + "##mesh" + std::to_string(meshIndex);
				if (ImGui::Selectable(label.c_str(), m_PreviewMeshIndex == static_cast<int32>(meshIndex)))
					m_PreviewMeshIndex = static_cast<int32>(meshIndex);
			}
		}
		ImGui::EndChild();
	}
	ImGui::TextWrapped("Mesh mapping is imported from the model. Save applies the draft to the preview; existing instances keep their snapshot until reloaded.");
	if (ImGui::Button("Reload Authored Settings")) Load_MaterialDraft();
	ImGui::SameLine();
	if (ImGui::Button("Generate Default Draft")) Generate_MaterialDraft();
	ImGui::SameLine();
	if (ImGui::Button("Save Atomically")) Save_MaterialDraft();
	if (!m_MaterialStatus.empty()) ImGui::TextWrapped("%s", m_MaterialStatus.c_str());
	if (!m_HasMaterialDraft) return;

	// Keep the editor usable under a tall preview; the Model Viewer window scrolls instead.
	if (ImGui::BeginChild("MaterialSettingsEditor", ImVec2(0, (std::max)(ImGui::GetContentRegionAvail().y, 520.f)), true))
	{
		ImGui::SetNextItemWidth(200.f);
		ImGui::SliderFloat("Thumbnail Size", &m_ThumbnailSize, 48.f, 320.f, "%.0f px");
		ImGui::SameLine();
		ImGui::TextDisabled("Click a thumbnail to open it in the Texture Viewer.");
		for (Engine::MODEL_MATERIAL_SETTING& setting : m_MaterialDraft.materials)
		{
			ImGui::PushID(static_cast<int>(setting.materialIndex));
			const string heading = std::to_string(setting.materialIndex) + " - " + setting.sourceMaterialName;
			if (ImGui::CollapsingHeader(heading.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::TextWrapped("Source: %s / %s", setting.sourceShaderName.c_str(), setting.sourceTechniqueName.c_str());
				ImGui::TextWrapped("Source shader metadata does not reproduce its shading. Concrete passes are authored separately.");
				const auto shader = MaterialShader(m_pSelectedModel, m_MaterialDraft.shaderTag);
				if (ImGui::BeginCombo("Pass", setting.passName.c_str()))
				{
					if (shader)
						for (const string& passName : shader->Get_PassNames())
							if (ImGui::Selectable(passName.c_str(), passName == setting.passName))
								setting.passName = passName;
					ImGui::EndCombo();
				}
				if (!shader || !shader->Has_Pass(setting.passName))
					ImGui::TextColored(ImVec4(1.f, 0.4f, 0.3f, 1.f), "Pass is not available in this model shader.");
				int renderGroup = setting.renderGroup == RENDERGROUP::DECAL ? 1 : setting.renderGroup == RENDERGROUP::BLEND ? 2 : 0;
				if (ImGui::Combo("Render Group", &renderGroup, "NONBLEND\0DECAL\0BLEND\0"))
					setting.renderGroup = renderGroup == 1 ? RENDERGROUP::DECAL : renderGroup == 2 ? RENDERGROUP::BLEND : RENDERGROUP::NONBLEND;
				ImGui::TextWrapped("DECAL meshes blend into the G-buffer after the opaque meshes (world tiles). "
					"BLEND meshes draw forward-lit after lighting (water); the preview skips them.");

				if (ImGui::TreeNodeEx("Parameters", ImGuiTreeNodeFlags_DefaultOpen))
				{
					Engine::MaterialParameters& parameters = setting.parameters;
					ImGui::ColorEdit4("Base Color Tint", &parameters.baseColorTint.x);
					ImGui::ColorEdit4("Emissive Color", &parameters.emissiveColor.x);
					ImGui::DragFloat("Emissive Intensity", &parameters.emissiveIntensity, 0.01f, 0.f, 100.f);
					ImGui::DragFloat("Opacity Clip", &parameters.opacityMaskClipValue, 0.01f, 0.f, 1.f);
					ImGui::TextDisabled("0 keeps every pixel, 0.5 = NieR g_1BitMask");
					ImGui::Checkbox("Inverse Alpha", &parameters.unpremultiplyAlpha);
					ImGui::SameLine();
					ImGui::TextDisabled("color / alpha: DDS cutouts whose empty texels are black (not for PBS10 height alpha)");
					ImGui::Checkbox("Override Metallic", &parameters.metallicOverride);
					ImGui::SameLine();
					ImGui::DragFloat("Metallic", &parameters.metallic, 0.01f, 0.f, 1.f);
					ImGui::Checkbox("Override Roughness", &parameters.roughnessOverride);
					ImGui::SameLine();
					ImGui::DragFloat("Roughness", &parameters.roughness, 0.01f, 0.f, 1.f);
					ImGui::TextDisabled("Roughness = 1 - NieR g_Glossiness");
					ImGui::Checkbox("Mask sRGB", &parameters.maskSrgb);
					ImGui::SameLine();
					ImGui::TextDisabled("g_MaskMap color space is sRGB in the .blend (W_PBS)");
					ImGui::DragFloat("Metalness Scale", &parameters.metalnessScale, 0.01f, 0.f, 4.f);
					ImGui::DragFloat("Roughness Scale", &parameters.roughnessScale, 0.01f, 0.f, 4.f);
					ImGui::DragFloat("AO Scale", &parameters.aoScale, 0.01f, 0.f, 1.f);
					ImGui::DragFloat("Normal Strength", &parameters.normalStrength, 0.01f, 0.f, 4.f);
					ImGui::DragFloat2("Detail Normal Tile", &parameters.detailNormalTileX, 0.1f, 0.f, 512.f);
					ImGui::DragFloat("Detail Normal Weight", &parameters.detailNormalWeight, 0.01f, 0.f, 1.f);
					ImGui::DragFloat("Intensity", &parameters.intensity, 0.01f, 0.f, 4.f);
					ImGui::DragFloat2("UV Tile", &parameters.uvTileX, 0.1f, 0.f, 512.f);
					ImGui::TextDisabled("Texcoord multiplier (NieR g_Tile / g_MaterialTile1), read by VTX_Mesh_Map");
					ImGui::DragFloat2("UV Tile 2", &parameters.uvTile2X, 0.1f, 0.f, 512.f);
					ImGui::TextDisabled("Second-UV multiplier (NieR g_MaterialTile2, PBS10 layers 2-3)");
					ImGui::Checkbox("Use UV2", &parameters.useUV2);
					ImGui::SameLine();
					ImGui::TextDisabled("NieR g_UV2Use: PBS00 samples the second UV set");
					if (ImGui::Button("Reset Parameters")) parameters = {};
					ImGui::TreePop();
				}

				for (size_t textureIndex = 0; textureIndex < setting.textures.size();)
				{
					ImGui::PushID(static_cast<int>(textureIndex));
					const ComPtr<ID3D11ShaderResourceView> thumbnail =
						Find_TexturePreview(m_TexturePreviews, setting.textures[textureIndex].resourcePath);
					const Float thumbnailSize = m_ThumbnailSize;
					if (thumbnail)
					{
						// RGB ignores alpha: NieR albedo/mask alpha would otherwise make the picture look faded or empty.
						if (ImGui::InvisibleButton("##thumbnail", ImVec2(thumbnailSize, thumbnailSize)))
						{
							m_InspectTexturePath = setting.textures[textureIndex].resourcePath;
							m_InspectTextureSlot = setting.sourceMaterialName + " / " + setting.textures[textureIndex].slotName;
							m_ShowTextureViewer = true;
							ImGui::SetWindowFocus("Texture Viewer");
						}
						const ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
						Draw_Image(ImGui::GetWindowDrawList(), thumbnail.Get(), min.x, min.y, max.x, max.y, ImageChannel::RGB);
						if (ImGui::IsItemHovered())
						{
							ImGui::GetWindowDrawList()->AddRect(min, max, ImGui::GetColorU32(ImGuiCol_ButtonHovered), 0.f, 0, 2.f);
							if (ImGui::BeginTooltip())
							{
								ComPtr<ID3D11Resource> resource;
								ComPtr<ID3D11Texture2D> texture;
								thumbnail->GetResource(resource.GetAddressOf());
								D3D11_TEXTURE2D_DESC desc{};
								if (resource && SUCCEEDED(resource.As(&texture)))
									texture->GetDesc(&desc);
								ImGui::Text("%u x %u  (RGB | Alpha)", desc.Width, desc.Height);
								constexpr Float tooltipSize = 320.f;
								ImGui::Dummy(ImVec2(tooltipSize * 2.f + 8.f, tooltipSize));
								const ImVec2 at = ImGui::GetItemRectMin();
								ImDrawList* tooltipList = ImGui::GetWindowDrawList();
								Draw_Image(tooltipList, thumbnail.Get(), at.x, at.y, at.x + tooltipSize, at.y + tooltipSize, ImageChannel::RGB);
								Draw_Image(tooltipList, thumbnail.Get(), at.x + tooltipSize + 8.f, at.y,
									at.x + tooltipSize * 2.f + 8.f, at.y + tooltipSize, ImageChannel::A);
								ImGui::EndTooltip();
							}
						}
					}
					else
					{
						ImGui::Dummy(ImVec2(thumbnailSize, thumbnailSize));
						const ImVec2 min = ImGui::GetItemRectMin();
						const ImVec2 max = ImGui::GetItemRectMax();
						ImGui::GetWindowDrawList()->AddRect(min, max, IM_COL32(200, 80, 60, 255));
						ImGui::GetWindowDrawList()->AddText(ImVec2(min.x + 6.f, min.y + 6.f),
							IM_COL32(200, 80, 60, 255), "No image");
					}
					ImGui::SameLine();
					ImGui::BeginGroup();
					// Inputs share the row with the thumbnail; leave room for the "Resource Path" label.
					ImGui::PushItemWidth((std::max)(ImGui::GetContentRegionAvail().x - 110.f, 120.f));
					ImGui::InputText("Slot", &setting.textures[textureIndex].slotName);
					if (!shader || !shader->Has_SRV(setting.textures[textureIndex].slotName))
						ImGui::TextColored(ImVec4(1.f, 0.4f, 0.3f, 1.f), "Texture slot is not available in this model shader.");
					string& resourcePath = setting.textures[textureIndex].resourcePath;
					if (ImGui::InputText("Resource Path", &resourcePath))
						Normalize_TexturePath(resourcePath);
					ImGui::PopItemWidth();
					// Files of the model's Textures folder, so paths do not have to be typed.
					ImGui::SetNextItemWidth((std::max)(ImGui::GetContentRegionAvail().x - 110.f, 120.f));
					if (ImGui::BeginCombo("Pick Texture", resourcePath.empty() ? "Select from the model Textures folder" : resourcePath.c_str(),
						ImGuiComboFlags_HeightLarge))
					{
						for (const string& file : m_ModelTextureFiles)
						{
							if (ImGui::Selectable(file.c_str(), file == resourcePath))
								resourcePath = file;
							if (ImGui::IsItemHovered())
								if (const auto hovered = Find_TexturePreview(m_TexturePreviews, file); hovered && ImGui::BeginTooltip())
								{
									ImGui::Dummy(ImVec2(256.f, 256.f));
									const ImVec2 at = ImGui::GetItemRectMin();
									Draw_Image(ImGui::GetWindowDrawList(), hovered.Get(), at.x, at.y, at.x + 256.f, at.y + 256.f, ImageChannel::RGB);
									ImGui::EndTooltip();
								}
						}
						if (m_ModelTextureFiles.empty())
							ImGui::TextDisabled("No files in the model Textures folder.");
						ImGui::EndCombo();
					}
					if (const string reason = Check_TexturePath(resourcePath); !reason.empty())
						ImGui::TextColored(ImVec4(1.f, 0.4f, 0.3f, 1.f), "%s", reason.c_str());
					const Bool removeSlot = ImGui::Button("Remove Slot");
					ImGui::EndGroup();
					if (removeSlot)
					{
						setting.textures.erase(setting.textures.begin() + textureIndex);
						ImGui::PopID();
						continue;
					}
					ImGui::PopID();
					++textureIndex;
				}

				// Offer exactly the texture slots the model shader declares and the material does not use yet.
				if (ImGui::BeginCombo("Add Slot", "Select a shader texture slot"))
				{
					if (shader)
						for (const string& slotName : shader->Get_SRVNames())
						{
							const Bool used = std::ranges::any_of(setting.textures,
								[&](const Engine::MATERIAL_TEXTURE_SETTING& texture) { return texture.slotName == slotName; });
							if (!used && ImGui::Selectable(slotName.c_str()))
								setting.textures.push_back({ slotName, {} });
						}
					ImGui::EndCombo();
				}
			}
			ImGui::PopID();
		}
	}
	ImGui::EndChild();
}

void ModelViewer::Reset_PreviewView()
{
	m_PreviewYaw = 0.f;
	m_PreviewPitch = 0.1f;
	m_PreviewZoom = 1.f;
	m_PreviewPan = Engine::Vector2::Zero;
}

HRESULT ModelViewer::Create_PreviewTargets(uint32 width, uint32 height)
{
	if (m_PreviewDepth && width == m_PreviewWidth && height == m_PreviewHeight)
		return S_OK;
	for (auto& target : m_PreviewTargets) target.Reset();
	for (auto& image : m_PreviewImages) image.Reset();
	m_PreviewDepth.Reset();
	m_PreviewWidth = m_PreviewHeight = 0;

	auto device = GAME_INSTANCE->Get_Device();
	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = width; desc.Height = height; desc.MipLevels = 1; desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = 1;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	for (uint32 i = 0; i < ETOI(PreviewBuffer::END); ++i)
	{
		ComPtr<ID3D11Texture2D> texture;
		if (FAILED(device->CreateTexture2D(&desc, nullptr, texture.GetAddressOf())) ||
			FAILED(device->CreateRenderTargetView(texture.Get(), nullptr, m_PreviewTargets[i].GetAddressOf())) ||
			FAILED(device->CreateShaderResourceView(texture.Get(), nullptr, m_PreviewImages[i].GetAddressOf())))
			return E_FAIL;
	}
	desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	ComPtr<ID3D11Texture2D> depth;
	if (FAILED(device->CreateTexture2D(&desc, nullptr, depth.GetAddressOf())) ||
		FAILED(device->CreateDepthStencilView(depth.Get(), nullptr, m_PreviewDepth.GetAddressOf())))
	{
		m_PreviewDepth.Reset();
		return E_FAIL;
	}
	m_PreviewWidth = width;
	m_PreviewHeight = height;
	return S_OK;
}

// Distance that fits the bounds as seen from the current orbit angle (box projected on the view axes).
Float ModelViewer::Compute_PreviewDistance(const BoundingBox& bounds, Float aspect) const
{
	const Float yawSin = std::abs(std::sin(m_PreviewYaw)), yawCos = std::abs(std::cos(m_PreviewYaw));
	const Float pitchSin = std::abs(std::sin(m_PreviewPitch)), pitchCos = std::abs(std::cos(m_PreviewPitch));
	const Float horizontal = bounds.Extents.x * yawCos + bounds.Extents.z * yawSin;
	const Float depthFlat = bounds.Extents.x * yawSin + bounds.Extents.z * yawCos;
	const Float vertical = bounds.Extents.y * pitchCos + depthFlat * pitchSin;
	const Float depth = depthFlat * pitchCos + bounds.Extents.y * pitchSin;
	const Float halfFov = std::tan(XM_PIDIV4 * 0.5f);
	const Float fit = (std::max)(vertical / halfFov, horizontal / (halfFov * aspect)) * 1.08f + depth;
	return (std::max)(fit, 0.001f) / m_PreviewZoom;
}

HRESULT ModelViewer::Shade_Preview(const Engine::Vector3& toLight, const Engine::Vector3& toEye)
{
	auto device = GAME_INSTANCE->Get_Device();
	auto context = GAME_INSTANCE->Get_Context();
	if (!m_LitShaderTried)
	{
		m_LitShaderTried = true;
		ComPtr<ID3DBlob> vsCode, psCode, errors;
		D3D11_BUFFER_DESC desc{};
		desc.ByteWidth = sizeof(Engine::Vector4) * 2;
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		// The source name puts the includes next to the engine hlsli copies in the working directory's shaders/.
		if (FAILED(D3DCompile(LitShaderSource, sizeof(LitShaderSource) - 1, "shaders/ModelViewerLit.hlsl", nullptr,
				D3D_COMPILE_STANDARD_FILE_INCLUDE, "VS_Main", "vs_5_0", 0, 0, vsCode.GetAddressOf(), errors.ReleaseAndGetAddressOf())) ||
			FAILED(D3DCompile(LitShaderSource, sizeof(LitShaderSource) - 1, "shaders/ModelViewerLit.hlsl", nullptr,
				D3D_COMPILE_STANDARD_FILE_INCLUDE, "PS_Main", "ps_5_0", 0, 0, psCode.GetAddressOf(), errors.ReleaseAndGetAddressOf())) ||
			FAILED(device->CreateVertexShader(vsCode->GetBufferPointer(), vsCode->GetBufferSize(), nullptr, m_LitVertexShader.GetAddressOf())) ||
			FAILED(device->CreatePixelShader(psCode->GetBufferPointer(), psCode->GetBufferSize(), nullptr, m_LitPixelShader.GetAddressOf())) ||
			FAILED(device->CreateBuffer(&desc, nullptr, m_LitConstants.GetAddressOf())))
		{
			const string message = errors ? string(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize()) : string{};
			LOG_ERROR(L"[ModelViewer] Lit preview shader could not be created: {}", Helper::To_wString(message));
			m_LitVertexShader.Reset(); m_LitPixelShader.Reset(); m_LitConstants.Reset();
		}
	}
	if (!m_LitPixelShader) return E_FAIL;

	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (FAILED(context->Map(m_LitConstants.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return E_FAIL;
	Engine::Vector4* constants = static_cast<Engine::Vector4*>(mapped.pData);
	constants[0] = Engine::Vector4(toLight.x, toLight.y, toLight.z, 0.f);
	constants[1] = Engine::Vector4(toEye.x, toEye.y, toEye.z, 0.f);
	context->Unmap(m_LitConstants.Get(), 0);

	ID3D11RenderTargetView* litTarget = m_PreviewTargets[ETOI(PreviewBuffer::Lit)].Get();
	context->OMSetRenderTargets(1, &litTarget, nullptr);
	ID3D11ShaderResourceView* gbuffer[PreviewGBufferCount]{};
	for (uint32 i = 0; i < PreviewGBufferCount; ++i) gbuffer[i] = m_PreviewImages[i].Get();
	context->IASetInputLayout(nullptr);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->VSSetShader(m_LitVertexShader.Get(), nullptr, 0);
	context->GSSetShader(nullptr, nullptr, 0);
	context->PSSetShader(m_LitPixelShader.Get(), nullptr, 0);
	context->PSSetShaderResources(0, PreviewGBufferCount, gbuffer);
	context->PSSetConstantBuffers(0, 1, m_LitConstants.GetAddressOf());
	context->RSSetState(nullptr);
	context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
	context->OMSetDepthStencilState(nullptr, 0);
	context->Draw(3, 0);
	// Unbind so the G-buffer can be a render target again next frame.
	ID3D11ShaderResourceView* nullViews[PreviewGBufferCount]{};
	context->PSSetShaderResources(0, PreviewGBufferCount, nullViews);
	return S_OK;
}

HRESULT ModelViewer::Preview_Model(const Shared<Engine::Model>& model,
	ComPtr<ID3D11ShaderResourceView>& image, int32 meshIndex, uint32 width, uint32 height)
{
	using namespace Engine;
	image.Reset();
	if (!model || meshIndex < -1 || (meshIndex >= 0 && static_cast<uint32>(meshIndex) >= model->Get_NumMeshes()))
		return E_INVALIDARG;
	const Shared<const MODEL_MATERIAL_SETTINGS> settings = model->Get_MaterialSettings();
	auto sourceShader = MaterialShader(model, settings ? settings->shaderTag : string{});
	if (!sourceShader || FAILED(model->Validate_MaterialBindings(sourceShader))) return E_FAIL;
	width = std::clamp(width, 64u, 4096u);
	height = std::clamp(height, 64u, 4096u);
	if (FAILED(Create_PreviewTargets(width, height))) return E_FAIL;
	auto context = GAME_INSTANCE->Get_Context();
	if (m_PreviewModel.lock() != model || m_PreviewSourceShader != sourceShader)
	{
		m_PreviewShader = make_shared<Shader>(*sourceShader);
		m_PreviewSourceShader = sourceShader;
		m_PreviewModel = model;
	}

	// Frame what the draw actually produces. Skinned vertices stay in bind space and get the ModelSettings
	// pre-transform (pl0000: scale 0.01, yaw 180) and the current animation pose through the bones, so
	// framing the raw vertex positions put the camera 100x too far away or aimed at empty space.
	BoundingBox bounds;
	if (!model->Compute_PosedBounds(bounds, meshIndex)) return E_FAIL;
	const Vector3 center = bounds.Center;
	const Float radius = (std::max)(Vector3(bounds.Extents).Length(), 0.001f);
	const Float aspect = static_cast<Float>(width) / static_cast<Float>(height);
	const Float distance = Compute_PreviewDistance(bounds, aspect);

	ID3D11RenderTargetView* previousTargets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};
	ComPtr<ID3D11DepthStencilView> previousDepth;
	context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, previousTargets, previousDepth.GetAddressOf());
	D3D11_VIEWPORT previousViewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]{};
	UINT viewportCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
	context->RSGetViewports(&viewportCount, previousViewports);
	ID3D11RenderTargetView* targets[PreviewGBufferCount]{};
	for (uint32 i = 0; i < PreviewGBufferCount; ++i) targets[i] = m_PreviewTargets[i].Get();
	context->OMSetRenderTargets(PreviewGBufferCount, targets, m_PreviewDepth.Get());
	const Float background[]{ 0.08f, 0.09f, 0.11f, 1.f };
	const Float clearNormal[]{ 0.f, 0.f, 0.f, 0.f };
	const Float black[]{ 0.f, 0.f, 0.f, 1.f };
	for (uint32 i = 0; i < PreviewGBufferCount; ++i)
		context->ClearRenderTargetView(m_PreviewTargets[i].Get(), i == ETOI(PreviewBuffer::Diffuse) ? background :
			i == ETOI(PreviewBuffer::Normal) ? clearNormal : black);
	context->ClearDepthStencilView(m_PreviewDepth.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.f, 0);
	const D3D11_VIEWPORT viewport{ 0.f, 0.f, static_cast<Float>(width), static_cast<Float>(height), 0.f, 1.f };
	context->RSSetViewports(1, &viewport);

	// Orbit camera around the bounds; yaw 0 sits on the -Z side looking along +Z like the old fixed camera.
	const Vector3 direction(std::sin(m_PreviewYaw) * std::cos(m_PreviewPitch), std::sin(m_PreviewPitch),
		-std::cos(m_PreviewYaw) * std::cos(m_PreviewPitch));
	Vector3 rightAxis(-direction.z, 0.f, direction.x);
	rightAxis.Normalize();
	Vector3 upAxis = rightAxis.Cross(direction);
	upAxis.Normalize();
	const Vector3 target = center + rightAxis * m_PreviewPan.x + upAxis * m_PreviewPan.y;
	const Vector3 eye = target + direction * distance;
	Matrix world = Matrix::Identity;
	Matrix view = XMMatrixLookAtLH(eye, target, Vector3::Up);
	Matrix projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, aspect,
		(std::max)(distance - radius * 2.f, distance * 0.01f), distance + radius * 2.f);
	HRESULT result = S_OK;
	if (m_PreviewShader->Supports_CBuffer(ConstantBuffer::Camera))
	{
		CameraCB camera{}; camera.viewMatrix = view; camera.projMatrix = projection;
		camera.viewInverseMatrix = view.Invert(); camera.projInverseMatrix = projection.Invert();
		camera.cameraPosition = Vector4(eye.x, eye.y, eye.z, 1.f);
		result = m_PreviewShader->Bind_CBufferData(camera);
	}
	else if (FAILED(m_PreviewShader->Bind_Matrix(ViewMatrix, &view)) ||
		FAILED(m_PreviewShader->Bind_Matrix(ProjMatrix, &projection))) result = E_FAIL;
	if (FAILED(m_PreviewShader->Bind_Matrix(WorldMatrix, &world))) result = E_FAIL;
	/* Decal meshes blend over the opaque ones, so they draw last as in the Renderer's DECAL group. BLEND meshes
	   (water) light themselves from the scene's light, shadow and depth, which the preview does not have. */
	for (const Bool decals : { false, true })
		for (uint32 i = 0; SUCCEEDED(result) && i < model->Get_NumMeshes(); ++i)
		{
			if (meshIndex >= 0 && i != static_cast<uint32>(meshIndex)) continue;
			const RENDERGROUP group = model->Get_MeshRenderGroup(i);
			if (RENDERGROUP::BLEND == group || (RENDERGROUP::DECAL == group) != decals) continue;
			if ((model->Is_Skeletal() && FAILED(model->Bind_BoneMatrices(m_PreviewShader, BoneMatrices, i))) ||
				FAILED(model->BindAndBeginMaterial(m_PreviewShader, i)) || FAILED(model->Render(i))) result = E_FAIL;
		}
	// Headlight from above-left of the camera.
	if (SUCCEEDED(result) && m_PreviewBuffer == PreviewBuffer::Lit)
		result = Shade_Preview(direction + upAxis * 0.7f - rightAxis * 0.5f, direction);
	context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, previousTargets, previousDepth.Get());
	for (auto* previous : previousTargets) if (previous) previous->Release();
	context->RSSetViewports(viewportCount, previousViewports);
	if (SUCCEEDED(result)) image = m_PreviewImages[ETOI(m_PreviewBuffer)];
	return result;
}

void ModelViewer::View_PreviewToolbar()
{
	int32 buffer = static_cast<int32>(m_PreviewBuffer);
	ImGui::SetNextItemWidth(280.f);
	if (ImGui::Combo("Buffer", &buffer, "Diffuse (albedo)\0Normal (world, n * 0.5 + 0.5)\0Material (R metal, G rough, B AO)\0Lit (game lighting, headlight)\0"))
		m_PreviewBuffer = static_cast<PreviewBuffer>(buffer);
	ImGui::SameLine();
	if (ImGui::Button("Front")) { m_PreviewYaw = XM_PI; m_PreviewPitch = 0.1f; }
	ImGui::SameLine();
	if (ImGui::Button("Back")) { m_PreviewYaw = 0.f; m_PreviewPitch = 0.1f; }
	ImGui::SameLine();
	if (ImGui::Button("Side")) { m_PreviewYaw = XM_PIDIV2; m_PreviewPitch = 0.1f; }
	ImGui::SameLine();
	if (ImGui::Button("Reset View")) Reset_PreviewView();
	ImGui::SameLine();
	if (ImGui::Button("All Meshes")) m_PreviewMeshIndex = -1;
	ImGui::SameLine();
	if (m_PreviewMeshIndex < 0) ImGui::TextUnformatted("All meshes");
	else ImGui::Text("Mesh %d only (framed)", m_PreviewMeshIndex);
	ImGui::TextDisabled("LMB drag rotate | RMB/MMB drag pan | Wheel zoom | Double-click reset");
}

void ModelViewer::View_Preview(Float width, Float height)
{
	width = (std::max)(width, 64.f);
	height = (std::max)(height, 64.f);
	ComPtr<ID3D11ShaderResourceView> preview;
	const HRESULT rendered = Preview_Model(m_pSelectedModel, preview, m_PreviewMeshIndex,
		static_cast<uint32>(width), static_cast<uint32>(height));
	ImGui::InvisibleButton("##ModelPreview", ImVec2(width, height),
		ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
	ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
	const ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	if (SUCCEEDED(rendered) && preview)
		Draw_Image(drawList, preview.Get(), min.x, min.y, max.x, max.y, ImageChannel::RGB);
	else
	{
		drawList->AddRectFilled(min, max, IM_COL32(20, 22, 28, 255));
		drawList->AddText(ImVec2(min.x + 12.f, min.y + 12.f), IM_COL32(230, 110, 90, 255),
			"Preview unavailable: check the material pass and texture slots.");
	}

	const ImGuiIO& io = ImGui::GetIO();
	if (ImGui::IsItemActive())
	{
		if (io.MouseDown[ImGuiMouseButton_Left])
		{
			m_PreviewYaw -= io.MouseDelta.x * 0.01f;
			m_PreviewPitch = std::clamp(m_PreviewPitch + io.MouseDelta.y * 0.01f, -1.5f, 1.5f);
		}
		else if (io.MouseDown[ImGuiMouseButton_Right] || io.MouseDown[ImGuiMouseButton_Middle])
		{
			// One pixel moves the target by one pixel's worth of world space at the target depth.
			BoundingBox bounds;
			if (m_pSelectedModel->Compute_PosedBounds(bounds, m_PreviewMeshIndex))
			{
				const Float distance = Compute_PreviewDistance(bounds, width / height);
				const Float perPixel = 2.f * distance * std::tan(XM_PIDIV4 * 0.5f) / height;
				m_PreviewPan.x -= io.MouseDelta.x * perPixel;
				m_PreviewPan.y += io.MouseDelta.y * perPixel;
			}
		}
	}
	if (ImGui::IsItemHovered())
	{
		if (io.MouseWheel != 0.f)
			m_PreviewZoom = std::clamp(m_PreviewZoom * std::pow(1.15f, io.MouseWheel), 0.2f, 60.f);
		if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			Reset_PreviewView();
	}
}

void ModelViewer::Draw_Image(ImDrawList* drawList, ID3D11ShaderResourceView* image, Float minX, Float minY,
	Float maxX, Float maxY, ImageChannel channel)
{
	if (!drawList || !image) return;
	if (!m_ChannelShadersTried)
	{
		m_ChannelShadersTried = true;
		const Char* swizzles[ETOI(ImageChannel::RGBA)]{ "rgb", "rrr", "ggg", "bbb", "aaa" };
		for (uint32 i = 0; i < ETOI(ImageChannel::RGBA); ++i)
		{
			const D3D_SHADER_MACRO macros[]{ { "CHANNEL", swizzles[i] }, { nullptr, nullptr } };
			ComPtr<ID3DBlob> code, errors;
			if (SUCCEEDED(D3DCompile(ChannelShaderSource, sizeof(ChannelShaderSource) - 1, "ModelViewerChannel",
				macros, nullptr, "main", "ps_5_0", 0, 0, code.GetAddressOf(), errors.GetAddressOf())))
				GAME_INSTANCE->Get_Device()->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(),
					nullptr, m_ChannelShaders[i].GetAddressOf());
		}
	}
	// Without a compiled shader the image falls back to ImGui's own blended draw.
	ID3D11PixelShader* shader = channel < ImageChannel::RGBA ? m_ChannelShaders[ETOI(channel)].Get() : nullptr;
	if (shader) drawList->AddCallback(Set_ImGuiPixelShader, shader);
	drawList->AddImage(reinterpret_cast<ImTextureID>(image), ImVec2(minX, minY), ImVec2(maxX, maxY));
	if (shader) drawList->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
}

void ModelViewer::View_TextureViewer()
{
	if (!m_ShowTextureViewer) return;
	ImGui::SetNextWindowSize(ImVec2(760.f, 820.f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Texture Viewer", &m_ShowTextureViewer))
	{
		const ComPtr<ID3D11ShaderResourceView> texture = Find_TexturePreview(m_TexturePreviews, m_InspectTexturePath);
		ImGui::TextWrapped("%s", m_InspectTextureSlot.c_str());
		ImGui::TextWrapped("%s", m_InspectTexturePath.c_str());
		if (!texture)
			ImGui::TextColored(ImVec4(1.f, 0.4f, 0.3f, 1.f), "The texture could not be loaded.");
		else
		{
			ComPtr<ID3D11Resource> resource;
			ComPtr<ID3D11Texture2D> texture2D;
			texture->GetResource(resource.GetAddressOf());
			D3D11_TEXTURE2D_DESC desc{};
			if (resource && SUCCEEDED(resource.As(&texture2D)))
				texture2D->GetDesc(&desc);
			ImGui::Text("%u x %u, DXGI format %d, %u mips", desc.Width, desc.Height, static_cast<int32>(desc.Format), desc.MipLevels);
			int32 channel = static_cast<int32>(m_InspectChannel);
			const Char* names[]{ "RGB", "R", "G", "B", "A", "RGBA (blended)" };
			for (int32 i = 0; i < static_cast<int32>(ImageChannel::END); ++i)
			{
				if (i > 0) ImGui::SameLine();
				ImGui::RadioButton(names[i], &channel, i);
			}
			m_InspectChannel = static_cast<ImageChannel>(channel);
			ImGui::TextDisabled("NieR mask: R metal, G roughness (Invert Green), B albedo mask/AO, A detail mask");

			// Fit the image into the remaining space, keeping its aspect ratio.
			const ImVec2 available = ImGui::GetContentRegionAvail();
			const Float aspect = desc.Height ? static_cast<Float>(desc.Width) / static_cast<Float>(desc.Height) : 1.f;
			Float drawWidth = (std::max)(available.x, 64.f), drawHeight = drawWidth / aspect;
			if (drawHeight > available.y && available.y > 64.f) { drawHeight = available.y; drawWidth = drawHeight * aspect; }
			ImGui::Dummy(ImVec2(drawWidth, drawHeight));
			const ImVec2 min = ImGui::GetItemRectMin();
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			if (m_InspectChannel == ImageChannel::RGBA)
			{
				// Checkerboard so blended alpha is visible.
				constexpr Float cell = 16.f;
				for (Float y = 0.f; y < drawHeight; y += cell)
					for (Float x = 0.f; x < drawWidth; x += cell)
						drawList->AddRectFilled(ImVec2(min.x + x, min.y + y),
							ImVec2(min.x + (std::min)(x + cell, drawWidth), min.y + (std::min)(y + cell, drawHeight)),
							((static_cast<int32>(x / cell) + static_cast<int32>(y / cell)) & 1) ? IM_COL32(90, 90, 90, 255) : IM_COL32(60, 60, 60, 255));
			}
			Draw_Image(drawList, texture.Get(), min.x, min.y, min.x + drawWidth, min.y + drawHeight, m_InspectChannel);
		}
	}
	ImGui::End();
}

void ModelViewer::View_AnimationList() 
{
	ImGui::Text("Animations");
	ImGui::Separator();

	if (!m_pSelectedModel)
	{
		ImGui::Text("No model selected.");
		return;
	}

	const string presetLabel = m_SelectedPresetPath.empty() ? "<No Preset>" : Helper::To_String(m_SelectedPresetPath.stem().wstring());
	if (ImGui::BeginCombo("Preset", presetLabel.c_str()))
	{
		std::error_code errorCode;
		for (filesystem::directory_iterator it(PATH.GetAnimationPresetSettingsDir(),
			filesystem::directory_options::skip_permission_denied, errorCode), end;
			it != end; it.increment(errorCode))
		{
			if (errorCode) { errorCode.clear(); continue; }
			if (!it->is_regular_file() || it->path().extension() != L".json")
				continue;
			const string label = Helper::To_String(it->path().stem().wstring());
			if (ImGui::Selectable(label.c_str(), it->path() == m_SelectedPresetPath))
			{
				m_SelectedPresetPath = it->path();
				Apply_AnimationPreset();
			}
		}
		ImGui::EndCombo();
	}
	if (!m_PresetStatus.empty())
		ImGui::TextWrapped("%s", m_PresetStatus.c_str());

	uint32 numAnims = m_pSelectedModel->Get_NumAnimations();
	if (numAnims == 0)
	{
		ImGui::Text("No animations found.");
		return;
	}

	ImGui::Checkbox("Play", &m_IsPlaying);
	ImGui::SameLine();
	Bool isLoop = m_pSelectedModel->Is_AnimLoop();
	if (ImGui::Checkbox("Loop", &isLoop))
		m_pSelectedModel->Set_AnimLoop(isLoop);
	ImGui::SameLine();
	if (ImGui::Button("Restart"))
		m_pSelectedModel->Set_AnimationProgress(0.f);
	ImGui::SliderFloat("Speed", &m_PlaySpeed, 0.f, 2.f, "%.2fx");
	Float progress = m_pSelectedModel->Get_AnimationProgress();
	if (ImGui::SliderFloat("Progress", &progress, 0.f, 1.f))
		m_pSelectedModel->Set_AnimationProgress(progress);

	// Keep the list usable under a tall preview; the Model Viewer window scrolls instead.
	if (ImGui::BeginChild("AnimationListChild", ImVec2(0, (std::max)(ImGui::GetContentRegionAvail().y, 320.f)), true))
	{
		for (uint32 i = 0; i < numAnims; ++i)
		{
			const wstring& animName = m_pSelectedModel->Get_AnimationNameByIndex(i);
			string utf8Name = std::to_string(i) + "  " + Helper::To_String(animName);

			bool isSelected = (m_SelectedAnimIndex == static_cast<int32>(i));
			if (ImGui::Selectable(utf8Name.c_str(), isSelected))
			{
				m_SelectedAnimIndex = i;
				m_pSelectedModel->Set_Animation(i, 0.f);
			}

			// Scroll to selected if it changed via keys
			if (isSelected && ImGui::IsWindowFocused())
				ImGui::SetScrollHereY();
		}
	}
	ImGui::EndChild();
}

void ModelViewer::Select_AnimationPreset()
{
	m_SelectedPresetPath.clear();
	m_PresetStatus.clear();
	if (!m_pSelectedModel || !m_pSelectedModel->Is_Skeletal())
		return;

	const wstring prefix = m_pSelectedModel->Get_ModelTag() + L"_";
	std::error_code errorCode;
	for (filesystem::directory_iterator it(PATH.GetAnimationPresetSettingsDir(),
		filesystem::directory_options::skip_permission_denied, errorCode), end;
		it != end; it.increment(errorCode))
	{
		if (errorCode) { errorCode.clear(); continue; }
		if (it->is_regular_file() && it->path().extension() == L".json" &&
			it->path().stem().wstring().starts_with(prefix) &&
			(m_SelectedPresetPath.empty() || it->path() < m_SelectedPresetPath))
			m_SelectedPresetPath = it->path();
	}

	// A model already showing clips keeps them; one without clips gets its preset right away.
	if (!m_SelectedPresetPath.empty() && m_pSelectedModel->Get_NumAnimations() == 0)
		Apply_AnimationPreset();
}

void ModelViewer::Apply_AnimationPreset()
{
	if (!m_pSelectedModel || m_SelectedPresetPath.empty())
		return;

	string error;
	if (FAILED(AnimationPresetEditor::Apply_PresetFile(*m_pSelectedModel, m_SelectedPresetPath, error)))
	{
		m_PresetStatus = "Preset apply failed: " + error;
		return;
	}
	m_SelectedAnimIndex = static_cast<int32>(m_pSelectedModel->Get_AnimationIndex());
	m_PresetStatus = "Applied " + std::to_string(m_pSelectedModel->Get_NumAnimations()) + " clips.";
}

void ModelViewer::Handle_KeyInput()
{
	if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
		return;

	if (!m_pSelectedModel) return;

	uint32 numAnims = m_pSelectedModel->Get_NumAnimations();
	if (numAnims == 0) return;

	if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
	{
		m_SelectedAnimIndex--;
		if (m_SelectedAnimIndex < 0) m_SelectedAnimIndex = numAnims - 1;
		m_pSelectedModel->Set_Animation(m_SelectedAnimIndex, 0.f);
	}

	if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
	{
		m_SelectedAnimIndex++;
		if (m_SelectedAnimIndex >= static_cast<int32>(numAnims)) m_SelectedAnimIndex = 0;
		m_pSelectedModel->Set_Animation(m_SelectedAnimIndex, 0.f);
	}
}

Shared<ModelViewer> ModelViewer::Create() 
{
	auto modelViewer = make_shared<ModelViewer>();
	if (FAILED(modelViewer->Initialize())) return nullptr;
	return modelViewer;
}

NS_END
