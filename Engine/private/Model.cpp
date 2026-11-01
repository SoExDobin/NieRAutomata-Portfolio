#include "Model.h"
#include "Game.h"
#include "Bone.h"
#include "Material.h"
#include "Mesh.h"
#include "SkeletalMesh.h"
#include "InstanceMesh.h"
#include "Animation.h"
#include "AnimationTracker.h"
#include <nlohmann/json.hpp>

#include <fstream>
#include <istream>
#include <filesystem>
#include "SpdLogger.h"

namespace
{
	using json = nlohmann::json;

	string To_Utf8Path(const filesystem::path& path)
	{
		const std::u8string value = path.generic_u8string();
		return string{ value.begin(), value.end() };
	}

	Bool Is_SafeResourcePath(const string& value)
	{
		if (value.empty() || value.find('\\') != string::npos)
			return false;

		const filesystem::path path = filesystem::path(Helper::To_wString(value));
		if (path.is_absolute() || path.has_root_name() || path.has_root_directory())
			return false;
		if (To_Utf8Path(path.lexically_normal()) != value)
			return false;

		Bool firstPart = true;
		for (const filesystem::path& part : path)
		{
			if (part == L"." || part == L"..")
				return false;
			if (firstPart)
			{
				wstring name = part.wstring();
				std::ranges::transform(name, name.begin(), [](wchar_t ch) {
					return static_cast<wchar_t>(::towlower(ch));
				});
				if (name == L"resources")
					return false;
				firstPart = false;
			}
		}
		return true;
	}

	// Optional "parameters" object of one material. Missing keys keep the MaterialParameters defaults.
	Bool Parse_MaterialParameters(const json& item, MaterialParameters& out, string& outError)
	{
		if (!item.contains("parameters"))
			return true;
		const json& values = item["parameters"];
		if (!values.is_object())
		{
			outError = "parameters must be an object";
			return false;
		}
		auto readFloat = [&](const char* key, Float& target) {
			if (!values.contains(key)) return true;
			if (!values[key].is_number()) { outError = string("parameter ") + key + " must be a number"; return false; }
			target = values[key].get<Float>();
			return true;
		};
		auto readBool = [&](const char* key, Bool& target) {
			if (!values.contains(key)) return true;
			if (!values[key].is_boolean()) { outError = string("parameter ") + key + " must be a boolean"; return false; }
			target = values[key].get<bool>();
			return true;
		};
		auto readVector4 = [&](const char* key, Vector4& target) {
			if (!values.contains(key)) return true;
			const json& v = values[key];
			if (!v.is_array() || v.size() != 4 || !std::ranges::all_of(v, [](const json& e) { return e.is_number(); }))
			{
				outError = string("parameter ") + key + " must be an array of 4 numbers";
				return false;
			}
			target = Vector4{ v[0].get<Float>(), v[1].get<Float>(), v[2].get<Float>(), v[3].get<Float>() };
			return true;
		};
		auto readTile = [&](const Char* key, Float& x, Float& y) {
			if (!values.contains(key)) return true;
			const json& v = values[key];
			if (!v.is_array() || v.size() != 2 || !v[0].is_number() || !v[1].is_number())
			{
				outError = string("parameter ") + key + " must be an array of 2 numbers";
				return false;
			}
			x = v[0].get<Float>();
			y = v[1].get<Float>();
			return true;
		};
		return readVector4("baseColorTint", out.baseColorTint) && readVector4("emissiveColor", out.emissiveColor) &&
			readFloat("emissiveIntensity", out.emissiveIntensity) && readFloat("normalStrength", out.normalStrength) &&
			readFloat("opacityMaskClipValue", out.opacityMaskClipValue) && readFloat("aoScale", out.aoScale) &&
			readFloat("roughnessScale", out.roughnessScale) && readFloat("metalnessScale", out.metalnessScale) &&
			readBool("metallicOverride", out.metallicOverride) && readFloat("metallic", out.metallic) &&
			readBool("roughnessOverride", out.roughnessOverride) && readFloat("roughness", out.roughness) &&
			readBool("maskSrgb", out.maskSrgb) &&
			readTile("detailNormalTile", out.detailNormalTileX, out.detailNormalTileY) &&
			readFloat("detailNormalWeight", out.detailNormalWeight) &&
			readFloat("intensity", out.intensity) && readTile("uvTile", out.uvTileX, out.uvTileY) &&
			readTile("uvTile2", out.uvTile2X, out.uvTile2Y) && readBool("useUV2", out.useUV2) &&
			readBool("unpremultiplyAlpha", out.unpremultiplyAlpha);
	}

	Bool Parse_MaterialSettingsDocument(const filesystem::path& settingsPath,
		const wstring& expectedModelTag, const string& expectedModelPath,
		const vector<string>& materialNames, MODEL_MATERIAL_SETTINGS& outSettings,
		string& outError)
	{
		outError.clear();
		try
		{
			ifstream in(settingsPath, ios::binary);
			if (!in.is_open())
			{
				outError = "unable to open material settings";
				return false;
			}

			json document;
			in >> document;
			if (!document.is_object() || document.value("schemaVersion", 0u) != 1u)
			{
				outError = "unsupported material settings schema";
				return false;
			}

			const string expectedTag = Helper::To_String(expectedModelTag);
			if (!document.contains("modelTag") || !document["modelTag"].is_string() ||
				document["modelTag"].get<string>() != expectedTag)
			{
				outError = "material settings modelTag mismatch";
				return false;
			}

			if (!document.contains("modelPath") || !document["modelPath"].is_string())
			{
				outError = "material settings modelPath is missing";
				return false;
			}
			const string modelPath = document["modelPath"].get<string>();
			if (!Is_SafeResourcePath(modelPath) || modelPath != expectedModelPath)
			{
				outError = "material settings modelPath is unsafe or does not match the model";
				return false;
			}

			if (!document.contains("materials") || !document["materials"].is_array() ||
				document["materials"].size() != materialNames.size())
			{
				outError = "material settings must contain exactly one entry per model material";
				return false;
			}

			MODEL_MATERIAL_SETTINGS candidate;
			candidate.modelTag = expectedTag;
			candidate.modelPath = modelPath;
			if (document.contains("shaderTag"))
			{
				if (!document["shaderTag"].is_string())
				{
					outError = "material settings shaderTag must be a string";
					return false;
				}
				candidate.shaderTag = document["shaderTag"].get<string>();
			}
			candidate.materials.reserve(materialNames.size());
			unordered_set<uint32> usedIndices;
			for (const json& item : document["materials"])
			{
				if (!item.is_object() || !item.contains("materialIndex") ||
					!item["materialIndex"].is_number_unsigned())
				{
					outError = "materialIndex must be an unsigned integer";
					return false;
				}

				MODEL_MATERIAL_SETTING setting;
				setting.materialIndex = item["materialIndex"].get<uint32>();
				if (setting.materialIndex >= materialNames.size() ||
					!usedIndices.insert(setting.materialIndex).second)
				{
					outError = "duplicate or out-of-range materialIndex";
					return false;
				}

				if (!item.contains("sourceMaterialName") || !item["sourceMaterialName"].is_string())
				{
					outError = "sourceMaterialName is missing";
					return false;
				}
				setting.sourceMaterialName = item["sourceMaterialName"].get<string>();
				if (setting.sourceMaterialName != materialNames[setting.materialIndex])
				{
					outError = "sourceMaterialName does not match the model material index";
					return false;
				}

				setting.sourceShaderName = item.value("sourceShaderName", string{});
				setting.sourceTechniqueName = item.value("sourceTechniqueName", string{});
				setting.passName = item.value("passName", string{});
				if (setting.passName.empty() || setting.passName == "Unresolved")
				{
					outError = "passName must be resolved before saving";
					return false;
				}

				const string renderGroup = item.value("renderGroup", string{});
				if (renderGroup == "NONBLEND")
					setting.renderGroup = RENDERGROUP::NONBLEND;
				else if (renderGroup == "DECAL")
					setting.renderGroup = RENDERGROUP::DECAL;
				else if (renderGroup == "BLEND")
					setting.renderGroup = RENDERGROUP::BLEND;
				else
				{
					outError = "renderGroup must be NONBLEND, DECAL or BLEND";
					return false;
				}

				if (!item.contains("textures") || !item["textures"].is_object())
				{
					outError = "textures must be an object";
					return false;
				}
				for (auto textureIt = item["textures"].begin(); textureIt != item["textures"].end(); ++textureIt)
				{
					if (textureIt.key().empty() || !textureIt.value().is_string())
					{
						outError = "texture slot and path must be strings";
						return false;
					}
					const string resourcePath = textureIt.value().get<string>();
					if (!Is_SafeResourcePath(resourcePath))
					{
						outError = "unsafe texture resource path";
						return false;
					}
					setting.textures.push_back({ textureIt.key(), resourcePath });
				}
				if (!Parse_MaterialParameters(item, setting.parameters, outError))
					return false;
				candidate.materials.push_back(std::move(setting));
			}

			std::ranges::sort(candidate.materials, {}, &MODEL_MATERIAL_SETTING::materialIndex);
			outSettings = std::move(candidate);
			return true;
		}
		catch (const std::exception& exception)
		{
			outError = exception.what();
			return false;
		}
	}

	wstring Find_ResourceRoot(const tChar* modelFilePath)
	{
		if (!modelFilePath || !*modelFilePath)
			return {};
		std::error_code errorCode;
		filesystem::path current = filesystem::weakly_canonical(
			filesystem::absolute(filesystem::path(modelFilePath), errorCode), errorCode).parent_path();
		if (errorCode)
			return {};
		while (!current.empty())
		{
			wstring name = current.filename().wstring();
			std::ranges::transform(name, name.begin(), [](wchar_t ch) {
				return static_cast<wchar_t>(::towlower(ch));
			});
			if (name == L"resources")
				return current.wstring();
			const filesystem::path parent = current.parent_path();
			if (parent == current)
				break;
			current = parent;
		}
		return {};
	}

	Bool Is_SafeAnimationPath(const wstring& relativePath)
	{
		const filesystem::path path(relativePath);
		if (relativePath.empty() || path.is_absolute() || path.extension() != L".anim")
			return false;
		return std::ranges::none_of(path, [](const filesystem::path& part) {
			return part == L".." || part == L".";
		});
	}
}

struct Model::ANIMATION_CLIP_CACHE
{
	struct ENTRY
	{
		// 같은 경로를 다시 변환한 파일은 시각·크기가 달라져 새로 파싱한다.
		filesystem::file_time_type writeTime{};
		uintmax_t size{};
		Shared<Animation> animation;
	};
	mutex lock;
	unordered_map<wstring, ENTRY> entries;
};

Model::Model() : Component{} {}
Model::Model(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: Component{device, context} {}
Model::Model(const Model& rhs)
	: Component{ rhs }, 
	m_PreLocalTransformMatrix{ rhs.m_PreLocalTransformMatrix }, 
	m_IsSkeletal{rhs.m_IsSkeletal},
	m_IsAnimLoop{rhs.m_IsAnimLoop}, 
	m_NumMeshes{ rhs.m_NumMeshes },
	m_Meshes{ rhs.m_Meshes },
	m_NumMaterials{ rhs.m_NumMaterials },
	m_Materials{ rhs.m_Materials },
	m_AnimationNames{ rhs.m_AnimationNames },
	m_NumBones {rhs.m_NumBones}, m_NumAnimation{rhs.m_NumAnimation},
	m_BindBoneMatrices{ rhs.m_BindBoneMatrices },
	m_ModelTag { rhs.m_ModelTag },
	m_ResourceRootPath{ rhs.m_ResourceRootPath },
	m_ModelFilePath{ rhs.m_ModelFilePath },
	m_ModelResourcePath{ rhs.m_ModelResourcePath },
	m_MaterialSettings{ rhs.m_MaterialSettings },
	m_AnimationPreset{ rhs.m_AnimationPreset },
	m_ClipCache{ rhs.m_ClipCache },
	m_Tracker{ nullptr }
{
	for (auto& prototypeAnim : rhs.m_Animations)
		m_Animations.push_back(static_pointer_cast<Animation>(prototypeAnim->Clone()));

	for (auto& prototypeBone : rhs.m_Bones)
		m_Bones.push_back(static_pointer_cast<Bone>(prototypeBone->Clone()));

	// ★ 버그 수정: 클론할 때마다 Tracker도 새로 생성 (nullptr로 남겨두면 Reset시 크래시)
	m_Tracker = AnimationTracker::Create(m_Device, m_Context);
}

HRESULT Model::Initialize_Prototype(const tChar* modelFilePath, const Matrix& preLocalTransformMatrix)
{
	m_PreLocalTransformMatrix = preLocalTransformMatrix;
	m_ResourceRootPath = Find_ResourceRoot(modelFilePath);
	std::error_code modelPathError;
	m_ModelFilePath = filesystem::weakly_canonical(
		filesystem::absolute(filesystem::path(modelFilePath), modelPathError), modelPathError).wstring();
	if (modelPathError)
		m_ModelFilePath = modelFilePath;
	if (!m_ResourceRootPath.empty())
	{
		const filesystem::path relativePath = filesystem::relative(
			filesystem::path(m_ModelFilePath), filesystem::path(m_ResourceRootPath), modelPathError);
		if (!modelPathError)
			m_ModelResourcePath = To_Utf8Path(relativePath);
	}

	ifstream in(modelFilePath, std::ios::binary);
	if (!in.is_open())
	{
		LOG_ERROR(L"Failed to open model binary : {}", modelFilePath);
		return E_FAIL;
	}

	/* Header */
	MODEL_HEADER header{};
	in.read(reinterpret_cast<Char*>(&header), sizeof(header));
	if (!in || memcmp(header.magic, MODEL_MAGIC, sizeof(header.magic)) != 0 ||
		header.version == 0 || header.version > MODEL_VERSION)
	{
		LOG_ERROR(L"Invalid model header or unsupported version : {}", modelFilePath);
		return E_FAIL;
	}

	m_IsSkeletal = header.isAnim;
	m_ModelVersion = header.version;
	m_NumMeshes = header.numMeshes;
	m_NumMaterials = header.numMaterials;
	m_NumBones = header.numBones;
	m_NumAnimation = header.numAnimations;
	if (m_IsSkeletal)
		m_ClipCache = make_shared<ANIMATION_CLIP_CACHE>();

	std::string currentModelDirectory = std::filesystem::path(modelFilePath).parent_path().string() + "\\";

	if (FAILED(Ready_Bones(in)))
		return E_FAIL;
	// The bones still hold the file's rest transforms; the first animation update below poses them.
	m_BindBoneMatrices.clear();
	for (const auto& bone : m_Bones)
	{
		bone->Update_CombinedTransformationMatrix(m_Bones, m_PreLocalTransformMatrix);
		m_BindBoneMatrices.push_back(*bone->Get_CombinedTransformationMatrixPtr());
	}

	if (FAILED(Ready_Meshes(in, header.isAnim)))
		return E_FAIL;

	if (FAILED(Ready_Materials(in, currentModelDirectory)))
		return E_FAIL;

	if (FAILED(Ready_Animation(in)))
		return E_FAIL;

	m_Tracker = AnimationTracker::Create(m_Device, m_Context);
	if (m_Tracker == nullptr)
		return E_FAIL;
	
	Update_ModelAnimation(0.f);

	return Component::Initialize_Prototype();
}

HRESULT Model::Initialize_Prototype()
{
	return Component::Initialize_Prototype();
}

HRESULT Model::Initialize(void* arg)
{
	return Component::Initialize(arg);
}

HRESULT Model::Post_Load()
{
	if (m_AnimationPreset.Is_Empty() &&
		(m_PendingAnimIndex != UINT32_MAX || !m_AnimationNotifyDefinitions.empty()))
		LOG_WARN(L"[AnimationPreset] Model {} has no snapshot; animation requests remain pending", m_ModelTag);
	return m_AnimationPreset.Is_Empty() ? S_OK : Apply_AnimationPreset(m_AnimationPreset);
}

HRESULT Model::Set_ModelTag(const wstring& tag)
{
	if (m_ModelTag == tag) return S_OK;

	// 1. 새로운 프로토타입 검색
	int32 levIndex = GAME_INSTANCE->Get_ContainLevelByModelTag(tag);
	if (levIndex == -1)
	{
		LOG_ERROR(L"Failed to find Model {} in levels", tag);
		return E_INVALIDARG;
	}

	auto prototype = GAME_INSTANCE->Get_Model(levIndex, tag.c_str());
	if (!prototype) return E_FAIL;

	// ── 핵심 방어 코드 ──
	// 1. 자기 자신이 복사 대상(프로토타입)일 경우
	if (prototype.get() == this)
	{
		m_ModelTag = tag;
		return S_OK;
	}

	// 2. 스켈레탈(애니메이션) 타입 호환성 체크
	if (m_IsSkeletal != prototype->m_IsSkeletal)
	{
		LOG_WARN(L"[Model] Skeletal mismatch! Prev: {}, New: {}. Assignment aborted.", 
			m_IsSkeletal ? L"Skeletal" : L"Static", prototype->m_IsSkeletal ? L"Skeletal" : L"Static");
		return E_INVALIDARG;
	}

	// 2. 기존 리소스 정리
	On_Destroy();

	// 3. 데이터 깊은 복사 (프로토타입으로부터)
	// A replaced tag (Level 4 asked for em0010) resolves to the stand-in, which keeps its own tag and material settings.
	m_ModelTag = prototype->m_ModelTag;
	m_PreLocalTransformMatrix = prototype->m_PreLocalTransformMatrix;
	m_ResourceRootPath = prototype->m_ResourceRootPath;
	m_ModelFilePath = prototype->m_ModelFilePath;
	m_ModelResourcePath = prototype->m_ModelResourcePath;
	m_MaterialSettings = prototype->m_MaterialSettings;
	m_AnimationPreset = prototype->m_AnimationPreset;
	m_IsSkeletal = prototype->m_IsSkeletal;
	m_NumMeshes = prototype->m_NumMeshes;
	m_Meshes = prototype->m_Meshes; // Mesh는 공유
	m_NumMaterials = prototype->m_NumMaterials;
	m_Materials = prototype->m_Materials; // Material은 공유
	m_AnimationNames = prototype->m_AnimationNames;
	m_NumBones = prototype->m_NumBones;
	m_NumAnimation = prototype->m_NumAnimation;
	m_ClipCache = prototype->m_ClipCache;

	// 본과 애니메이션은 상태를 가지므로 클론(Clone) 필수
	m_Bones.clear();
	for (auto& pBone : prototype->m_Bones)
		m_Bones.push_back(static_pointer_cast<Bone>(pBone->Clone()));
	m_SkinDirty = true;

	m_Animations.clear();
	for (auto& pAnim : prototype->m_Animations)
		m_Animations.push_back(static_pointer_cast<Animation>(pAnim->Clone()));

	if (m_Tracker)
		m_Tracker->Clear();

	// 애니메이션 초기 바인딩 업데이트
	Update_ModelAnimation(0.f);
	return S_OK;
}

HRESULT Model::Get_MeshMaterialInfo(uint32 meshIndex, string& meshName, uint32& materialIndex) const
{
	if (meshIndex >= m_Meshes.size() || !m_Meshes[meshIndex]) return E_INVALIDARG;
	meshName = m_Meshes[meshIndex]->Get_MeshName();
	materialIndex = m_Meshes[meshIndex]->Get_MaterialIndex();
	return S_OK;
}

vector<string> Model::Get_MaterialNames() const
{
	vector<string> names;
	names.reserve(m_Materials.size());
	for (const Shared<Material>& material : m_Materials)
		names.push_back(material ? material->Get_MaterialName() : string{});
	return names;
}

wstring Model::Make_MaterialSettingsFileName(const wstring& modelTag)
{
	wstring fileName = modelTag;
	for (wchar_t& ch : fileName)
	{
		if (ch < 0x20 || ch == L'<' || ch == L'>' || ch == L':' || ch == L'"' ||
			ch == L'/' || ch == L'\\' || ch == L'|' || ch == L'?' || ch == L'*')
			ch = L'_';
	}
	for (size_t index = fileName.size(); index > 0 &&
		(fileName[index - 1] == L' ' || fileName[index - 1] == L'.'); --index)
		fileName[index - 1] = L'_';
	if (fileName.empty() || fileName == L"." || fileName == L"..")
		fileName = L"_";
	return fileName + L".json";
}

HRESULT Model::Validate_MaterialSettings(const filesystem::path& settingsPath,
	const wstring& expectedModelTag) const
{
	if (!filesystem::is_regular_file(settingsPath))
		return S_FALSE;

	MODEL_MATERIAL_SETTINGS candidate;
	string error;
	if (!Parse_MaterialSettingsDocument(settingsPath, expectedModelTag,
		m_ModelResourcePath, Get_MaterialNames(), candidate, error))
	{
		LOG_ERROR(L"Invalid material settings {} : {}", settingsPath.wstring(), Helper::To_wString(error));
		return E_FAIL;
	}
	return S_OK;
}

HRESULT Model::Prepare_MaterialSettings(const filesystem::path& settingsPath,
	const wstring& expectedModelTag, Shared<const MODEL_MATERIAL_SETTINGS>& prepared,
	const Shared<Shader>& shader) const
{
	if (!filesystem::is_regular_file(settingsPath))
		return S_FALSE;

	MODEL_MATERIAL_SETTINGS candidate;
	string error;
	if (!Parse_MaterialSettingsDocument(settingsPath, expectedModelTag,
		m_ModelResourcePath, Get_MaterialNames(), candidate, error))
	{
		LOG_ERROR(L"Invalid material settings {} : {}", settingsPath.wstring(), Helper::To_wString(error));
		return E_FAIL;
	}

	try
	{
		for (const auto& setting : candidate.materials)
		{
			vector<pair<string, filesystem::path>> paths;
			for (const auto& texture : setting.textures)
			{
				const filesystem::path path = filesystem::weakly_canonical(
					filesystem::path(m_ResourceRootPath) /
					filesystem::path(std::u8string(texture.resourcePath.begin(), texture.resourcePath.end())));
				const string relative = To_Utf8Path(filesystem::relative(path, m_ResourceRootPath));
				if (!Is_SafeResourcePath(relative) || !filesystem::is_regular_file(path))
				{
					LOG_ERROR(L"Missing or unsafe material texture {}", path.wstring());
					return E_FAIL;
				}
				paths.emplace_back(texture.slotName, path);
				if (std::ranges::find(candidate.textureSlots, texture.slotName) == candidate.textureSlots.end())
					candidate.textureSlots.push_back(texture.slotName);
			}
			auto material = make_shared<Material>(m_Device, m_Context);
			if (FAILED(material->Prepare_NamedTextures(paths, setting.parameters))) return E_FAIL;
			candidate.preparedMaterials.push_back(std::move(material));
		}
	}
	catch (const std::exception& exception)
	{
		LOG_ERROR(L"Material preparation failed: {}", Helper::To_wString(exception.what()));
		return E_FAIL;
	}
	std::ranges::sort(candidate.textureSlots);
	Shared<const MODEL_MATERIAL_SETTINGS> snapshot = make_shared<MODEL_MATERIAL_SETTINGS>(std::move(candidate));
	if (shader && FAILED(Validate_MaterialBindings(shader, snapshot))) return E_FAIL;
	prepared = std::move(snapshot);
	return S_OK;
}

HRESULT Model::Load_MaterialSettings(const filesystem::path& settingsPath,
	const wstring& expectedModelTag, const Shared<Shader>& shader)
{
	Shared<const MODEL_MATERIAL_SETTINGS> prepared;
	const HRESULT hr = Prepare_MaterialSettings(settingsPath, expectedModelTag, prepared, shader);
	if (hr != S_OK) return hr;
	return Apply_MaterialSettings(prepared);
}

HRESULT Model::Apply_MaterialSettings(const Shared<const MODEL_MATERIAL_SETTINGS>& prepared)
{
	if (!prepared || (!m_ModelTag.empty() && prepared->modelTag != Helper::To_String(m_ModelTag)) ||
		prepared->modelPath != m_ModelResourcePath || prepared->preparedMaterials.size() != m_Materials.size() ||
		prepared->materials.size() != m_Materials.size()) return E_INVALIDARG;
	for (size_t i = 0; i < m_Materials.size(); ++i)
		if (!prepared->preparedMaterials[i] || prepared->materials[i].materialIndex != i ||
			prepared->materials[i].sourceMaterialName != m_Materials[i]->Get_MaterialName()) return E_INVALIDARG;
	if (m_ModelTag.empty()) m_ModelTag = Helper::To_wString(prepared->modelTag);
	m_MaterialSettings = prepared;
	return S_OK;
}

HRESULT Model::Validate_MaterialBindings(const Shared<Shader>& shader,
	const Shared<const MODEL_MATERIAL_SETTINGS>& settings) const
{
	if (!shader) return E_INVALIDARG;
	if (!settings)
		return shader->Has_Pass("Default_Pass") && shader->Has_SRV("g_AlbedoMap") ? S_OK : E_FAIL;
	if (settings->preparedMaterials.size() != m_Materials.size()) return E_FAIL;
	for (const auto& setting : settings->materials)
	{
		if (!shader->Has_Pass(setting.passName))
		{
			LOG_ERROR(L"Model {} material {} has unknown pass {}", m_ModelTag,
				setting.materialIndex, Helper::To_wString(setting.passName));
			return E_FAIL;
		}
		for (const auto& texture : setting.textures)
			if (!shader->Has_SRV(texture.slotName))
			{
				LOG_ERROR(L"Model {} material {} has unknown slot {}", m_ModelTag,
					setting.materialIndex, Helper::To_wString(texture.slotName));
				return E_FAIL;
			}
	}
	return S_OK;
}

HRESULT Model::Validate_MaterialBindings(const Shared<Shader>& shader) const
{
	return Validate_MaterialBindings(shader, m_MaterialSettings);
}

RENDERGROUP Model::Get_MeshRenderGroup(uint32 meshIndex) const
{
	if (!m_MaterialSettings || meshIndex >= m_Meshes.size() || !m_Meshes[meshIndex])
		return RENDERGROUP::NONBLEND;
	const uint32 materialIndex = m_Meshes[meshIndex]->Get_MaterialIndex();
	return materialIndex < m_MaterialSettings->materials.size() ?
		m_MaterialSettings->materials[materialIndex].renderGroup : RENDERGROUP::NONBLEND;
}

HRESULT Model::BindAndBeginMaterial(const Shared<Shader>& shader, uint32 meshIndex) const
{
	if (!shader || meshIndex >= m_Meshes.size() || !m_Meshes[meshIndex]) return E_INVALIDARG;
	const uint32 materialIndex = m_Meshes[meshIndex]->Get_MaterialIndex();
	if (materialIndex >= m_Materials.size() || !m_Materials[materialIndex]) return E_INVALIDARG;
	/* 그림자 패스는 재질을 평소처럼 묶고(알파 컷용 알베도·clip 값) 깊이 전용 ShadowCompatible로 시작한다.
	   모델 셰이더는 모두 이 패스를 가지므로, 없으면 설정 오류로 실패시킨다. */
	static const string shadowPassName{ "ShadowCompatible" };
	const Bool shadowPass = GAME_INSTANCE->Is_ShadowPass();
	if (shadowPass && !shader->Has_Pass(shadowPassName)) return E_FAIL;
	if (!m_MaterialSettings)
	{
		static const vector<string> defaultSlots{ "g_AlbedoMap" };
		if (!shader->Has_Pass("Default_Pass") || FAILED(shader->Clear_MaterialSlots(defaultSlots)) ||
			FAILED(m_Materials[materialIndex]->Bind_DefaultTexture(shader, !shadowPass))) return E_FAIL;
		return shader->Begin(shadowPass ? shadowPassName : string{ "Default_Pass" });
	}
	if (materialIndex >= m_MaterialSettings->preparedMaterials.size() ||
		materialIndex >= m_MaterialSettings->materials.size()) return E_FAIL;
	const auto& setting = m_MaterialSettings->materials[materialIndex];
	const auto& material = m_MaterialSettings->preparedMaterials[materialIndex];
	if (!material || !shader->Has_Pass(setting.passName) ||
		FAILED(shader->Clear_MaterialSlots(m_MaterialSettings->textureSlots)) ||
		FAILED(material->Bind_NamedTextures(shader))) return E_FAIL;
	return shader->Begin(shadowPass ? shadowPassName : setting.passName);
}

void Model::On_Destroy()
{
	m_Meshes.clear();
	m_Materials.clear();
	m_Bones.clear();
	m_MaterialSettings.reset();
	m_Animations.clear();
	m_AnimationNames.clear();
	Component::On_Destroy();
}

int32 Model::Get_AnimationIndexByName(const wstring& name)
{
	if (!m_AnimationNames.contains(name))
	{
		LOG_ERROR(L"Failed to Get Animation Index by Name {}", name); 
		return - 1;
	}

	return m_AnimationNames[name];
}

const wstring& Model::Get_AnimationNameByIndex(uint32 index)
{
	static wstring emptyString = L"";
	if (index >= m_Animations.size())
	{
		LOG_ERROR(L"Failed to Get Animation Name by Index {}", index);
		return emptyString;
	}

	return m_Animations[index]->Get_AnimationName();
}

void Model::Update_ModelAnimation(Float timeDelta)
{
	if (!m_IsActive || !m_IsSkeletal) return;
	// Without clips the bones keep their bind values. Pose them there again so a last-frame override (a part's cloth)
	// is not read back as this frame's pose.
	if (m_Animations.empty())
	{
		for (auto& bone : m_Bones)
			bone->Update_CombinedTransformationMatrix(m_Bones, m_PreLocalTransformMatrix);
		m_HasPose = true;
		m_SkinDirty = true;
		return;
	}
	if (m_CurrentAnimIndex >= m_Animations.size() || m_NextAnimIndex >= m_Animations.size())
		return;

	if (m_IsBlending)
	{
		m_BlendingElapsed += timeDelta;
		Float ratio = m_BlendingElapsed / m_BlendingDuration;

		if (ratio >= 1.f)
		{
			m_IsBlending = false;
			m_CurrentAnimIndex = m_NextAnimIndex;
			m_Animations[m_CurrentAnimIndex]->Update_TransformationMatrix(timeDelta, m_Bones, m_IsAnimLoop, m_RootLocalNode);
		}
		else
		{
			m_Animations[m_CurrentAnimIndex]->Blend_TransformationMatrix(timeDelta, m_Animations[m_NextAnimIndex], ratio, m_Bones, m_IsPrevAnimLoop, m_IsAnimLoop, m_RootLocalNode);
		}
	}
	else
	{
		m_IsAnimEnd = m_Animations[m_CurrentAnimIndex]->Update_TransformationMatrix(timeDelta, m_Bones, m_IsAnimLoop, m_RootLocalNode);
	}

	for (auto& bone : m_Bones)
	{
		bone->Update_CombinedTransformationMatrix(m_Bones, m_PreLocalTransformMatrix);
	}
	m_HasPose = true;
	m_SkinDirty = true;

	uint32 activeAnimIdx = m_IsBlending ? m_NextAnimIndex : m_CurrentAnimIndex;
	
	if (m_Tracker)
		m_Tracker->Update(activeAnimIdx, Get_AnimationProgress());
}

void Model::Set_Animation(uint32 index, Float blendDuration)
{
	if (m_IsSkeletal && m_Animations.empty() && m_Tracker)
	{
		if (m_PendingAnimIndex == UINT32_MAX)
			LOG_WARN(L"[AnimationPreset] Deferring initial animation {} for model {} until clips are applied", index, m_ModelTag);
		m_PendingAnimIndex = index;
		return;
	}
	if (index >= m_Animations.size() || !m_Tracker)
	{
		LOG_ERROR(L"Failed to set animation index {} on model {}", index, m_ModelTag);
		return;
	}
	if (!m_IsBlending && m_CurrentAnimIndex == index) return;
	// A zero-blend request snaps even onto the clip being blended in, so a part left mid-blend can be put back in step.
	if (m_IsBlending && m_NextAnimIndex == index && blendDuration > 0.0001f) return;

	m_Tracker->Reset();

	if (m_IsBlending)
	{
		m_CurrentAnimIndex = m_NextAnimIndex;
	}

	m_IsPrevAnimLoop = m_IsAnimLoop;
 	m_NextAnimIndex = index;

	if (blendDuration <= 0.0001f)
	{
		m_IsBlending = false;
		m_CurrentAnimIndex = index; // 즉시 현재 타겟으로 할당
		m_BlendingElapsed = 0.f;
		m_BlendingDuration = 0.f;
	}
	else
	{
		m_IsBlending = true;
		m_BlendingElapsed = 0.f;
		m_BlendingDuration = blendDuration;
	}
	m_IsAnimEnd = false;

	m_Animations[m_NextAnimIndex]->Set_Progress(0.f);
}

void Model::Set_AnimationProgress(Float progress)
{
	if (m_IsBlending || m_CurrentAnimIndex >= m_Animations.size()) return;

	m_Animations[m_CurrentAnimIndex]->Set_Progress(progress);
	m_IsAnimEnd = false;
	// Notifies already fired before the seek point must fire again when the clip passes them.
	if (m_Tracker)
		m_Tracker->Reset();
	Update_ModelAnimation(0.f);
}

void Model::Add_AnimNotify(uint32 animIndex, const AnimationTracker::ANIMATION_NOTIFY& notify)
{
	if (m_Tracker == nullptr || !m_IsSkeletal ||
		(!m_Animations.empty() && animIndex >= m_Animations.size()))
	{
		LOG_ERROR(L"Failed to Add Animation Notify {} : {}", notify.notifyTag, animIndex);
		return;
	}
	if (m_Animations.empty() && m_AnimationNotifyDefinitions.empty())
		LOG_WARN(L"[AnimationPreset] Deferring notify registration for model {} until clips are applied", m_ModelTag);
	m_AnimationNotifyDefinitions.emplace_back(animIndex, notify);
	if (!m_Animations.empty()) Bind_AnimNotify(animIndex, notify);
}

void Model::Bind_AnimNotify(uint32 animIndex, const AnimationTracker::ANIMATION_NOTIFY& notify) const
{
	Float duration = m_Animations[animIndex]->Get_Duration();

	if (duration <= 0.f)
	{
		duration = 1.f;
	}

	AnimationTracker::ANIMATION_TRACKER_NOTIFY trackerNotify;

	trackerNotify.notifyTag = notify.notifyTag;
	trackerNotify.type = notify.type;
	trackerNotify.invokeProgress = notify.invokeFrame / duration;
	trackerNotify.endProgress = notify.endFrame / duration;
	trackerNotify.onNotify = notify.onNotify;
	trackerNotify.onEndNotify = notify.onEndNotify;

	m_Tracker->Add_Notify(animIndex, trackerNotify);
}

void Model::Add_AnimNotify(uint32 animIndex, std::initializer_list<AnimationTracker::ANIMATION_NOTIFY> notifies)
{
	for (auto notify : notifies)
		Add_AnimNotify(animIndex, notify);
}

void Model::Clear_AnimNotifies()
{
	if (m_Tracker) m_Tracker->Clear();
	m_AnimationNotifyDefinitions.clear();
}

Bool Model::Validate_AnimationRequests(size_t clipCount) const
{
	if (m_PendingAnimIndex != UINT32_MAX && m_PendingAnimIndex >= clipCount)
	{
		LOG_ERROR(L"[AnimationPreset] Pending animation {} is outside {} clips on {}", m_PendingAnimIndex, clipCount, m_ModelTag);
		return false;
	}
	for (const auto& [index, notify] : m_AnimationNotifyDefinitions)
		if (index >= clipCount)
		{
			LOG_ERROR(L"[AnimationPreset] Notify {} index {} is outside {} clips on {}", notify.notifyTag, index, clipCount, m_ModelTag);
			return false;
		}
	return true;
}

void Model::Restore_AnimationRequests()
{
	if (m_Tracker) m_Tracker->Clear();
	for (const auto& [index, notify] : m_AnimationNotifyDefinitions)
		Bind_AnimNotify(index, notify);
	if (m_PendingAnimIndex != UINT32_MAX)
		m_CurrentAnimIndex = m_NextAnimIndex = m_PendingAnimIndex;
	m_PendingAnimIndex = UINT32_MAX;
	m_IsAnimEnd = false;
	if (m_RootLocalNode >= 0)
		Set_LocalRootNode(static_cast<uint32>(m_RootLocalNode));
}

Bool Model::Is_NotifyActive(uint32 animIndex, const wstring& notifyTag) const
{
	return m_Tracker->Is_ActiveNotify(animIndex, notifyTag);
}

Bool Model::Is_NotifyActive(const wstring& notifyTag) const
{
	uint32 activeIndex = m_IsBlending ? m_NextAnimIndex : m_CurrentAnimIndex;
	return m_Tracker->Is_ActiveNotify(activeIndex, notifyTag);
}

Bool Model::Compute_LocalBounds(BoundingBox& outBounds) const
{
	Bool hasBounds = false;
	for (const auto& mesh : m_Meshes)
	{
		if (!mesh || !mesh->Has_LocalBounds())
			continue;

		BoundingBox meshBounds = mesh->Get_LocalBounds();

		// Skinned vertices stay in bind space; the pre-transform is applied through the bones.
		if (m_IsSkeletal)
			meshBounds.Transform(meshBounds, m_PreLocalTransformMatrix);

		if (hasBounds)
			BoundingBox::CreateMerged(outBounds, outBounds, meshBounds);
		else
			outBounds = meshBounds;
		hasBounds = true;
	}

	return hasBounds;
}

Bool Model::Get_MeshLocalBounds(uint32 meshIndex, BoundingBox& outBounds) const
{
	if (meshIndex >= m_Meshes.size() || !m_Meshes[meshIndex] || !m_Meshes[meshIndex]->Has_LocalBounds())
		return false;

	outBounds = m_Meshes[meshIndex]->Get_LocalBounds();
	// Skinned vertices stay in bind space; the pre-transform is applied through the bones.
	if (m_IsSkeletal)
		outBounds.Transform(outBounds, m_PreLocalTransformMatrix);

	return true;
}

Bool Model::Intersect_MeshSegment(uint32 meshIndex, const Vector3& start, const Vector3& end,
	Vector3& outHit, Vector3& outNormal) const
{
	if (m_IsSkeletal || meshIndex >= m_Meshes.size() || !m_Meshes[meshIndex])
		return false;
	Vector3 direction = end - start;
	const Float length = direction.Length();
	if (length < 1e-6f) return false;
	direction /= length;
	BoundingBox bounds;
	Float distance{};
	if (!Get_MeshLocalBounds(meshIndex, bounds) || !bounds.Intersects(start, direction, distance) || distance > length)
		return false;
	const auto& positions = m_Meshes[meshIndex]->Get_RawPositions();
	const auto& indices = m_Meshes[meshIndex]->Get_RawIndices();
	Float nearest = length + 1e-5f;
	Bool found = false;
	for (size_t i = 0; i + 2 < indices.size(); i += 3)
	{
		const auto vertex = [&](int32 index) -> Vector3 {
			const size_t offset = static_cast<size_t>(index) * 3;
			return { positions[offset], positions[offset + 1], positions[offset + 2] };
		};
		if (indices[i] < 0 || indices[i + 1] < 0 || indices[i + 2] < 0 ||
			static_cast<size_t>(indices[i]) >= positions.size() / 3 ||
			static_cast<size_t>(indices[i + 1]) >= positions.size() / 3 ||
			static_cast<size_t>(indices[i + 2]) >= positions.size() / 3) continue;
		const Vector3 a = vertex(indices[i]), b = vertex(indices[i + 1]), c = vertex(indices[i + 2]);
		if (!DirectX::TriangleTests::Intersects(start, direction, a, b, c, distance) || distance > nearest)
			continue;
		Vector3 normal = (b - a).Cross(c - a);
		if (normal.LengthSquared() < 1e-12f) continue;
		normal.Normalize();
		nearest = distance;
		outHit = start + direction * distance;
		outNormal = normal;
		found = true;
	}
	return found;
}

Bool Model::Get_MeshPartCenters(uint32 meshIndex, vector<Vector3>& outCenters) const
{
	outCenters.clear();
	const Shared<SkeletalMesh> mesh = Get_SkeletalMesh(meshIndex);
	if (!mesh || 0 == mesh->Get_NumMeshBones())
		return false;

	outCenters.assign(mesh->Get_NumMeshBones(), Vector3::Zero);
	for (const auto& [boneIndex, bindBounds] : mesh->Get_DominantBoneBounds())
		outCenters[boneIndex] = bindBounds.Center;
	return true;
}

Bool Model::Compute_PosedBounds(BoundingBox& outBounds, int32 meshIndex)
{
	if (meshIndex >= static_cast<int32>(m_Meshes.size()))
		return false;
	if (!m_IsSkeletal)
		return meshIndex >= 0 ? Get_MeshLocalBounds(static_cast<uint32>(meshIndex), outBounds) : Compute_LocalBounds(outBounds);

	Update_SkinMatrices();

	// Each bone moves the bind-space box of the vertices it dominates. Blending keeps vertices close to
	// that box, so this frames the current pose (root motion and the pre-transform included) tightly.
	Bool hasBounds = false;
	for (uint32 i = 0; i < m_Meshes.size(); ++i)
	{
		if (meshIndex >= 0 && i != static_cast<uint32>(meshIndex))
			continue;
		const Shared<SkeletalMesh> mesh = Get_SkeletalMesh(i);
		if (!mesh || !mesh->Has_LocalBounds() || 0 == mesh->Get_NumMeshBones())
			continue;
		const Matrix* boneMatrices = m_SkinMatrices[i].data();
		for (const auto& [boneIndex, bindBounds] : mesh->Get_DominantBoneBounds())
		{
			BoundingBox posed;
			bindBounds.Transform(posed, boneMatrices[boneIndex]);
			if (hasBounds)
				BoundingBox::CreateMerged(outBounds, outBounds, posed);
			else
				outBounds = posed;
			hasBounds = true;
		}
	}
	return hasBounds || (meshIndex >= 0 ? Get_MeshLocalBounds(static_cast<uint32>(meshIndex), outBounds) : Compute_LocalBounds(outBounds));
}

void Model::Extract_RawMeshData(_Out_ vector<Float>& outPositions, _Out_ vector<int32>& outIndices) const
{
	outPositions.clear();
	outIndices.clear();
	int32 vertexOffset = 0;
	for (const auto& mesh : m_Meshes)
	{
		const auto& meshPos = mesh->Get_RawPositions();
		const auto& meshInd = mesh->Get_RawIndices();
		
		outPositions.insert(outPositions.end(), meshPos.begin(), meshPos.end());
		// 인덱스는 이전에 쌓인 정점 개수(vertexOffset)만큼 더해서 연결!
		for (int32 idx : meshInd)
		{
			outIndices.push_back(idx + vertexOffset);
		}
		
		vertexOffset += static_cast<int32>(meshPos.size() / 3);
	}
}

vector<BONE_SNAPSHOT> Model::Get_SnapShot_BoneMatrices()
{
	vector<BONE_SNAPSHOT> snapShots;
	snapShots.resize(m_NumMeshes);
	Update_SkinMatrices();

	for (uint32 i = 0 ; i < m_NumMeshes; ++i)
	{
		const Shared<SkeletalMesh> mesh = Get_SkeletalMesh(i);
		if (!mesh)
			continue;

		const uint32 numBones = mesh->Get_NumMeshBones();
		snapShots[i].numBones = numBones;

		memcpy(snapShots[i].matrices,
			m_SkinMatrices[i].data(),
			sizeof(Matrix) * numBones);
	}

	return snapShots;
}

int32 Model::Get_BoneIndexByName(const string& boneName) const
{
	for (size_t i = 0; i < m_Bones.size(); ++i)
	{
		if (m_Bones[i]->Is_SameBone(boneName.c_str()))
			return static_cast<int32>(i);
	}
	return -1;
}

Matrix Model::Get_BindBoneMatrix(uint32 boneIndex) const
{
	return boneIndex < m_BindBoneMatrices.size() ? m_BindBoneMatrices[boneIndex] : Matrix::Identity;
}

Matrix Model::Get_BoneMatrix(uint32 boneIndex) const
{
	if (boneIndex >= m_Bones.size())
		return Matrix::Identity;
	// Before the first animation update or draw the combined matrices are not built; answer with the bones' local
	// transforms as they stand (the first clip's first frame once clips are loaded, see Get_BindBoneMatrix).
	if (!m_HasPose)
		for (const auto& bone : m_Bones)
			bone->Update_CombinedTransformationMatrix(m_Bones, m_PreLocalTransformMatrix);
	return *m_Bones[boneIndex]->Get_CombinedTransformationMatrixPtr();
}

void Model::Override_BoneMatrices(const vector<uint32>& boneIndices, const vector<Matrix>& combinedMatrices)
{
	if (boneIndices.size() != combinedMatrices.size() || boneIndices.empty())
		return;
	if (!m_HasPose)
		for (const auto& bone : m_Bones)
			bone->Update_CombinedTransformationMatrix(m_Bones, m_PreLocalTransformMatrix);

	// 본은 부모가 앞에 오는 순서라 한 번 훑으면 덮어쓴 본 아래가 모두 새 자리를 따라간다.
	vector<int32> overrideOf(m_Bones.size(), -1);
	for (size_t i = 0; i < boneIndices.size(); ++i)
		if (boneIndices[i] < m_Bones.size())
			overrideOf[boneIndices[i]] = static_cast<int32>(i);

	vector<Bool> moved(m_Bones.size(), false);
	for (size_t i = 0; i < m_Bones.size(); ++i)
	{
		const int32 parent = m_Bones[i]->Get_ParentBoneIndex();
		if (overrideOf[i] >= 0)
			m_Bones[i]->Set_CombinedTransformationMatrix(combinedMatrices[overrideOf[i]]);
		else if (parent >= 0 && moved[parent])
			m_Bones[i]->Update_CombinedTransformationMatrix(m_Bones, m_PreLocalTransformMatrix);
		else
			continue;
		moved[i] = true;
	}
	m_HasPose = true;
	m_SkinDirty = true;
}

const TRANSFORM_FRAME& Model::Get_RootTransformVelocity(uint32 nodeIndex)
{
	// 프리셋 클립이 아직 적용되지 않은 모델(에디터에서 막 만든 객체 등)은 움직이지 않는다.
	if (m_CurrentAnimIndex >= m_Animations.size())
	{
		m_TransformFrame = TRANSFORM_FRAME{};
		m_TransformFrame.scale = Vector3::One;
		return m_TransformFrame;
	}

	if (m_IsBlending && m_NextAnimIndex < m_Animations.size())
	{
		Float ratio = m_BlendingElapsed / m_BlendingDuration;

		const TRANSFORM_FRAME& curDelta = m_Animations[m_CurrentAnimIndex]->Get_TransformVelocity(nodeIndex);
		const TRANSFORM_FRAME& nextDelta = m_Animations[m_NextAnimIndex]->Get_TransformVelocity(nodeIndex);

		m_TransformFrame.position = Vector3::Lerp(curDelta.position, nextDelta.position, ratio);
		m_TransformFrame.rotation = Quaternion::Slerp(curDelta.rotation, nextDelta.rotation, ratio);
	}
	else
	{
		const TRANSFORM_FRAME& delta = m_Animations[m_CurrentAnimIndex]->Get_TransformVelocity(nodeIndex);
		m_TransformFrame.position = delta.position;
		m_TransformFrame.rotation = delta.rotation;
	}
	m_TransformFrame.scale = Vector3::One;

	/* 채널 델타는 루트 본의 부모 공간 값이다. 부모의 결합 행렬(모델 설정의 스케일·회전 포함)로 옮겨
	   이번 프레임에 모델이 움직인 거리를 모델 공간으로 돌려준다. 루트 결합 = 루트 로컬 × 부모 결합. */
	if (nodeIndex < m_Bones.size())
	{
		const Shared<Bone>& root = m_Bones[nodeIndex];
		const Matrix parentToModel = root->Get_TransformationMatrix().Invert() * *root->Get_CombinedTransformationMatrixPtr();
		m_TransformFrame.position = Vector3::TransformNormal(m_TransformFrame.position, parentToModel);
	}

	return m_TransformFrame;
}

void Model::Set_LocalRootNode(uint32 nodeIndex)
{
	m_RootLocalNode = static_cast<int32>(nodeIndex);
	for (uint32 i = 0; i < m_NumAnimation; ++i)
	{
		m_Animations[i]->Set_LocalTransformationPresent(true);
		// 모든 animation의 순회로 루트에 영향을 받는 걸 명시
	}
}

HRESULT Model::Render_Meshes(const Shared<Shader>& shader, const vector<uint32>& meshIndices)
{
	/* 호출 한 번이 한 오브젝트의 그리기다. 호출자가 넣어 둔 월드 행렬 같은 값은 첫 메시의 Begin이
	   적용하고, 그 뒤로는 메시 사이에 바뀌는 셰이더 값이 없으므로 재질이 같으면 텍스처·패스가 이미
	   적용된 상태로 그린다. 스켈레탈 모델은 메시마다 본 행렬이 바뀌므로 매번 바인딩한다. */
	const Bool skipSameMaterial = !m_IsSkeletal;
	uint32 boundMaterial = UINT32_MAX;
	for (const uint32 meshIndex : meshIndices)
	{
		if (meshIndex >= m_Meshes.size() || !m_Meshes[meshIndex]) return E_INVALIDARG;

		const uint32 material = m_Meshes[meshIndex]->Get_MaterialIndex();
		if (!skipSameMaterial || material != boundMaterial)
		{
			if (FAILED(BindAndBeginMaterial(shader, meshIndex))) return E_FAIL;
			boundMaterial = material;
		}

		if (FAILED(Render(meshIndex))) return E_FAIL;
	}
	return S_OK;
}

HRESULT Model::Set_Instances(uint32 meshIndex, const vector<INSTANCE_DATA>& instances)
{
	if (m_IsSkeletal || meshIndex >= m_Meshes.size() || !m_Meshes[meshIndex]) return E_FAIL;
	Shared<InstanceMesh> mesh = dynamic_pointer_cast<InstanceMesh>(m_Meshes[meshIndex]);
	if (!mesh)
	{
		// 메시는 프로토타입과 공유하므로(Clone) 인스턴스 버퍼는 이 Model의 사본에만 둔다.
		const Shared<StaticMesh> source = dynamic_pointer_cast<StaticMesh>(m_Meshes[meshIndex]);
		if (!source) return E_FAIL;
		mesh = make_shared<InstanceMesh>(*source);
		m_Meshes[meshIndex] = mesh;
	}
	return mesh->Set_Instances(instances);
}

HRESULT Model::Render(uint32 meshIndex)
{
	if (meshIndex >= m_Meshes.size() || !m_Meshes[meshIndex]) return E_INVALIDARG;
	if (FAILED(m_Meshes[meshIndex]->Bind_Resources())) return E_FAIL;
	return m_Meshes[meshIndex]->Render();
}

HRESULT Model::Ready_Meshes(ifstream& in, Bool isAnim)
{
	for (uint32 i = 0; i < m_NumMeshes; ++i)
	{
		MODEL_MESH modelData{};
		uint32 nameLength, numBones, numOffsetMatrices, numVertex, numIndex;
		in.read(reinterpret_cast<Char*>(&nameLength), sizeof(uint32));
		if (nameLength > 0)
		{
			modelData.name.resize(nameLength);
			in.read(&modelData.name[0], nameLength);
		}
		in.read(reinterpret_cast<Char*>(&modelData.materialIndex), sizeof(int32));

		in.read(reinterpret_cast<Char*>(&numBones), sizeof(uint32));
		if (numBones > 0)
		{
			// 1. 본 인덱스 배열 로드
			modelData.boneIndices.resize(numBones);
			in.read(reinterpret_cast<Char*>(modelData.boneIndices.data()), numBones * sizeof(uint32));

			// 2. 오프셋 행렬 배열 로드 
			in.read(reinterpret_cast<Char*>(&numOffsetMatrices), sizeof(uint32));
			modelData.offsetMatrices.resize(numOffsetMatrices);
			in.read(reinterpret_cast<Char*>(modelData.offsetMatrices.data()), numOffsetMatrices * sizeof(Matrix));
		}

		in.read(reinterpret_cast<Char*>(&numVertex), sizeof(uint32));
		if (isAnim)
		{
			modelData.animVertices.resize(numVertex);
			in.read(reinterpret_cast<Char*>(modelData.animVertices.data()), numVertex * sizeof(VTXANIMMESH));
		}
		else if (m_ModelVersion >= MODEL_VERSION_STATIC_COLOR)
		{
			modelData.vertices.resize(numVertex);
			in.read(reinterpret_cast<Char*>(modelData.vertices.data()), numVertex * sizeof(VTXMESH));
		}
		else
		{
			// Version 2 static vertex: no second UV or color. The second UV repeats the first; color is white.
			vector<VTXMESH_V2> legacy(numVertex);
			in.read(reinterpret_cast<Char*>(legacy.data()), numVertex * sizeof(VTXMESH_V2));
			modelData.vertices.resize(numVertex);
			for (uint32 v = 0; v < numVertex; ++v)
			{
				VTXMESH& vertex = modelData.vertices[v];
				vertex.position = legacy[v].position;
				vertex.normal = legacy[v].normal;
				vertex.tangent = legacy[v].tangent;
				vertex.texcoord = legacy[v].texcoord;
				vertex.texcoord1 = legacy[v].texcoord;
				vertex.color = 0xFFFFFFFFu;
			}
		}

		in.read(reinterpret_cast<Char*>(&numIndex), sizeof(uint32));
		modelData.indices.resize(numIndex);
		in.read(reinterpret_cast<Char*>(modelData.indices.data()), numIndex * sizeof(uint32));
		auto mesh = Mesh::Create(m_Device, m_Context, isAnim, modelData, m_PreLocalTransformMatrix);
		if (mesh == nullptr) return E_FAIL;
		m_Meshes.push_back(mesh);
	}

	return S_OK;
}

HRESULT Model::Ready_Materials(ifstream& in, const std::string& directoryPath)
{
	for (size_t i = 0; i < m_NumMaterials; ++i)
	{
		MODEL_MATERIAL materialData{};
		uint32 nameLength = 0;
		in.read(reinterpret_cast<Char*>(&nameLength), sizeof(uint32));
		if (nameLength > 0)
		{
			materialData.name.resize(nameLength);
			in.read(&materialData.name[0], nameLength);
		}

		// textureTypeMax 읽기
		in.read(reinterpret_cast<Char*>(&materialData.textureTypeMax), sizeof(uint32));

		// directoryPath 세팅 (바이너리에서 읽지 않고 파라미터 복사)
		materialData.directoryPath = directoryPath;

		// textures 읽기
		uint32 numTex = 0;
		in.read(reinterpret_cast<Char*>(&numTex), sizeof(uint32));
		for (uint32 t = 0; t < numTex; ++t)
		{
			MODEL_ENTRY entry{};
			in.read(reinterpret_cast<Char*>(&entry.typeIndex), sizeof(uint32));
			uint32 pathLength = 0;
			in.read(reinterpret_cast<Char*>(&pathLength), sizeof(uint32));
			if (pathLength > 0)
			{
				entry.path.resize(pathLength);
				in.read(entry.path.data(), pathLength);
			}
			materialData.textures.push_back(entry);
		}

		auto material = Material::Create(m_Device, m_Context, materialData);
		if (material == nullptr)
			return E_FAIL;
		
		m_Materials.push_back(material);
	}

	return S_OK;
}

HRESULT Model::Ready_Animation(ifstream& in)
{
	for (uint32 i = 0; i < m_NumAnimation; ++i)
	{
		MODEL_ANIMATION animationData{};
		if (FAILED(Read_AnimationData(in, animationData, true)))
			return E_FAIL;

		auto animation = Animation::Create(m_Device, m_Context, animationData);
		if (animation == nullptr)
			return E_FAIL;

		m_AnimationNames.emplace(Helper::To_wString(animationData.name), i);
		m_Animations.push_back(animation);
	}

	return S_OK;
}

HRESULT Model::Read_AnimationData(ifstream& in, MODEL_ANIMATION& animationData,
	Bool hasStoredChannelCount, uint32 channelCount) const
{
	constexpr uint32 MaxNameLength = 4096;
	constexpr uint32 MaxChannelCount = MODEL_BONE_MAX * 4;
	constexpr uint32 MaxKeyFrameCount = 10'000'000;

	uint32 animNameLength = 0;
	in.read(reinterpret_cast<Char*>(&animNameLength), sizeof(animNameLength));
	if (!in || animNameLength == 0 || animNameLength > MaxNameLength)
		return E_FAIL;

	animationData.name.resize(animNameLength);
	in.read(animationData.name.data(), animNameLength);
	in.read(reinterpret_cast<Char*>(&animationData.duration), sizeof(animationData.duration));
	in.read(reinterpret_cast<Char*>(&animationData.tickPerSecond), sizeof(animationData.tickPerSecond));
	if (hasStoredChannelCount)
		in.read(reinterpret_cast<Char*>(&channelCount), sizeof(channelCount));
	in.read(reinterpret_cast<Char*>(&animationData.rootTotalTranslation), sizeof(animationData.rootTotalTranslation));
	in.read(reinterpret_cast<Char*>(&animationData.rootTotalRotation), sizeof(animationData.rootTotalRotation));
	if (!in || channelCount == 0 || channelCount > MaxChannelCount)
		return E_FAIL;
	if (!std::isfinite(animationData.duration) || animationData.duration < 0.f ||
		!std::isfinite(animationData.tickPerSecond) || animationData.tickPerSecond <= 0.f)
		return E_FAIL;

	animationData.channels.clear();
	animationData.channels.reserve(channelCount);
	unordered_set<string> channelNames;

	for (uint32 i = 0; i < channelCount; ++i)
	{
		MODEL_CHANNEL channelData{};
		uint32 channelNameLength = 0;
		in.read(reinterpret_cast<Char*>(&channelNameLength), sizeof(channelNameLength));
		if (!in || channelNameLength == 0 || channelNameLength > MaxNameLength)
			return E_FAIL;

		channelData.name.resize(channelNameLength);
		in.read(channelData.name.data(), channelNameLength);
		if (!in || !channelNames.emplace(channelData.name).second)
			return E_FAIL;

		if (hasStoredChannelCount)
			in.read(reinterpret_cast<Char*>(&channelData.boneIndex), sizeof(channelData.boneIndex));
		else
			channelData.boneIndex = Get_BoneIndexByName(channelData.name);

		in.read(reinterpret_cast<Char*>(&channelData.numKeyFrames), sizeof(channelData.numKeyFrames));
		if (!in || channelData.numKeyFrames == 0 || channelData.numKeyFrames > MaxKeyFrameCount)
			return E_FAIL;

		// A shared animation bank (2B .anim on A2) drives bones this model does not have; those channels are skipped.
		if (!hasStoredChannelCount && channelData.boneIndex < 0)
		{
			in.seekg(static_cast<std::streamoff>(channelData.numKeyFrames) * sizeof(KEYFRAME), std::ios::cur);
			if (!in)
				return E_FAIL;
			continue;
		}

		channelData.keyFrames.resize(channelData.numKeyFrames);
		in.read(reinterpret_cast<Char*>(channelData.keyFrames.data()),
			static_cast<std::streamsize>(channelData.numKeyFrames) * sizeof(KEYFRAME));
		if (!in)
			return E_FAIL;

		animationData.channels.push_back(std::move(channelData));
	}

	animationData.numChannel = static_cast<uint32>(animationData.channels.size());
	if (animationData.channels.empty())
	{
		LOG_ERROR(L"Animation {} has no channel for this model's bones", Helper::To_wString(animationData.name));
		return E_FAIL;
	}

	return S_OK;
}

HRESULT Model::Build_Animations(const vector<wstring>& animationFilePaths,
	const unordered_set<wstring>& existingNames,
	vector<Shared<Animation>>& outAnimations, vector<wstring>& outNames) const
{
	if (!m_IsSkeletal || m_Bones.empty() || animationFilePaths.empty())
		return E_INVALIDARG;

	outAnimations.clear();
	outNames.clear();
	unordered_set<wstring> allNames = existingNames;

	for (const wstring& animationFilePath : animationFilePaths)
	{
		std::error_code statError;
		const filesystem::file_time_type writeTime = filesystem::last_write_time(animationFilePath, statError);
		const uintmax_t fileSize = statError ? 0 : filesystem::file_size(animationFilePath, statError);
		const Bool canCache = m_ClipCache && !statError;

		Shared<Animation> source;
		if (canCache)
		{
			lock_guard guard(m_ClipCache->lock);
			const auto entryIt = m_ClipCache->entries.find(animationFilePath);
			if (entryIt != m_ClipCache->entries.end() &&
				entryIt->second.writeTime == writeTime && entryIt->second.size == fileSize)
				source = entryIt->second.animation;
		}

		if (!source)
		{
			ifstream in(animationFilePath, std::ios::binary);
			if (!in.is_open())
			{
				LOG_ERROR(L"Failed to open animation binary : {}", animationFilePath);
				return E_FAIL;
			}

			ANIMATION_HEADER header{};
			in.read(reinterpret_cast<Char*>(&header), sizeof(header));
			if (!in || memcmp(header.magic, ANIMATION_MAGIC, sizeof(header.magic)) != 0 ||
				header.version != ANIMATION_VERSION)
			{
				LOG_ERROR(L"Invalid animation header or unsupported version : {}", animationFilePath);
				return E_FAIL;
			}

			MODEL_ANIMATION animationData{};
			if (FAILED(Read_AnimationData(in, animationData, false, header.numChannels)))
			{
				LOG_ERROR(L"Failed to read animation binary : {}", animationFilePath);
				return E_FAIL;
			}

			source = Animation::Create(m_Device, m_Context, animationData);
			if (!source)
				return E_FAIL;
			if (canCache)
			{
				lock_guard guard(m_ClipCache->lock);
				m_ClipCache->entries.insert_or_assign(animationFilePath,
					ANIMATION_CLIP_CACHE::ENTRY{ writeTime, fileSize, source });
			}
		}

		const wstring animationName = source->Get_AnimationName();
		if (!allNames.emplace(animationName).second)
		{
			LOG_ERROR(L"Duplicate animation name {} from {}", animationName, animationFilePath);
			return E_FAIL;
		}

		// 캐시 원본은 재생 상태를 갖지 않게 그대로 두고 복제본을 쓴다.
		Shared<Animation> animation = canCache ? static_pointer_cast<Animation>(source->Clone()) : source;
		if (!animation)
			return E_FAIL;

		outNames.push_back(animationName);
		outAnimations.push_back(std::move(animation));
	}
	return S_OK;
}

HRESULT Model::Load_Animations(const vector<wstring>& animationFilePaths)
{
	unordered_set<wstring> existingNames;
	for (const auto& [name, index] : m_AnimationNames)
		existingNames.emplace(name);

	vector<Shared<Animation>> pendingAnimations;
	vector<wstring> pendingNames;
	if (FAILED(Build_Animations(animationFilePaths, existingNames, pendingAnimations, pendingNames)))
		return E_FAIL;
	if (!Validate_AnimationRequests(m_Animations.size() + pendingAnimations.size()))
		return E_INVALIDARG;

	for (size_t i = 0; i < pendingAnimations.size(); ++i)
	{
		const uint32 index = static_cast<uint32>(m_Animations.size());
		m_AnimationNames.emplace(pendingNames[i], index);
		m_Animations.push_back(std::move(pendingAnimations[i]));
	}
	m_NumAnimation = static_cast<uint32>(m_Animations.size());
	m_CurrentAnimIndex = 0;
	m_NextAnimIndex = 0;
	m_IsBlending = false;
	Restore_AnimationRequests();
	Update_ModelAnimation(0.f);
	return S_OK;
}

Bool Model::Validate_AnimationPreset(const AnimationPresetSnapshot& preset) const
{
	if (preset.schemaVersion != 2 || preset.animationEnum.empty() || preset.animations.empty())
		return false;

	ReflectedEnumInfo enumInfo;
	if (FAILED(GAME_INSTANCE->Find_ReflectedEnum(preset.animationEnum, enumInfo)) ||
		enumInfo.category != "AnimationState" || enumInfo.values.empty())
		return false;

	unordered_map<string, int64_t> enumValues;
	for (const ReflectedEnumValue& value : enumInfo.values)
	{
		if (value.name.empty() || value.value < 0 ||
			static_cast<size_t>(value.value) >= preset.animations.size() ||
			!enumValues.emplace(value.name, value.value).second)
			return false;
	}

	unordered_set<wstring> animationPaths;
	unordered_set<string> coveredStates;
	for (size_t index = 0; index < preset.animations.size(); ++index)
	{
		const AnimationPresetClip& clip = preset.animations[index];
		filesystem::path normalizedPath = filesystem::path(clip.relativePath).lexically_normal();
		wstring pathKey = normalizedPath.generic_wstring();
		std::ranges::transform(pathKey, pathKey.begin(), [](wchar_t ch) {
			return static_cast<wchar_t>(::towlower(ch));
		});
		if (!Is_SafeAnimationPath(clip.relativePath) ||
			!animationPaths.emplace(std::move(pathKey)).second)
			return false;

		for (const string& state : clip.states)
		{
			const auto stateIt = enumValues.find(state);
			if (stateIt == enumValues.end() || stateIt->second != static_cast<int64_t>(index) ||
				!coveredStates.emplace(state).second)
				return false;
		}
	}
	return coveredStates.size() == enumValues.size();
}

HRESULT Model::Apply_AnimationPreset(const AnimationPresetSnapshot& preset)
{
	if (preset.schemaVersion == 2 && preset.Is_Empty())
	{
		if (!m_Animations.empty())
			m_PendingAnimIndex = m_IsBlending ? m_NextAnimIndex : m_CurrentAnimIndex;
		if (m_Tracker) m_Tracker->Clear();
		m_Animations.clear();
		m_AnimationNames.clear();
		m_NumAnimation = 0;
		m_CurrentAnimIndex = m_NextAnimIndex = 0;
		m_IsBlending = false;
		m_IsAnimEnd = false;
		m_AnimationPreset = preset;
		LOG_WARN(L"[AnimationPreset] Cleared snapshot on model {}; requests retained for reapply", m_ModelTag);
		return S_OK;
	}
	if (!Validate_AnimationPreset(preset) || m_ResourceRootPath.empty() ||
		!Validate_AnimationRequests(preset.animations.size()))
		return E_INVALIDARG;

	vector<wstring> fullPaths;
	fullPaths.reserve(preset.animations.size());
	const filesystem::path resourceRoot(m_ResourceRootPath);
	for (const AnimationPresetClip& clip : preset.animations)
	{
		const filesystem::path fullPath = (resourceRoot / clip.relativePath).lexically_normal();
		if (!filesystem::is_regular_file(fullPath))
		{
			LOG_ERROR(L"AnimationPreset clip does not exist: {}", fullPath.wstring());
			return E_FAIL;
		}
		fullPaths.push_back(fullPath.wstring());
	}

	vector<Shared<Animation>> pendingAnimations;
	vector<wstring> pendingNames;
	if (FAILED(Build_Animations(fullPaths, {}, pendingAnimations, pendingNames)))
		return E_FAIL;

	map<wstring, uint32> pendingAnimationNames;
	for (size_t index = 0; index < pendingNames.size(); ++index)
		pendingAnimationNames.emplace(pendingNames[index], static_cast<uint32>(index));

	m_Animations = std::move(pendingAnimations);
	m_AnimationNames = std::move(pendingAnimationNames);
	m_NumAnimation = static_cast<uint32>(m_Animations.size());
	m_CurrentAnimIndex = 0;
	m_NextAnimIndex = 0;
	m_IsBlending = false;
	m_AnimationPreset = preset;
	Restore_AnimationRequests();
	Update_ModelAnimation(0.f);
	LOG_INFO(L"[AnimationPreset] Applied {} clips and {} notifies to model {}", m_NumAnimation, m_AnimationNotifyDefinitions.size(), m_ModelTag);
	return S_OK;
}

HRESULT Model::Ready_Bones(ifstream& in)
{
	for (uint32 i = 0; i < m_NumBones; ++i)
	{
		MODEL_BONE boneData{};
		uint32 nameLength = 0;
		in.read(reinterpret_cast<Char*>(&nameLength), sizeof(uint32));
		boneData.name.resize(nameLength);
		in.read(&boneData.name[0], nameLength);
		in.read(reinterpret_cast<Char*>(&boneData.parentIndex), sizeof(int32));
		in.read(reinterpret_cast<Char*>(&boneData.transform), sizeof(Matrix));

		auto bone = Bone::Create(m_Device, m_Context, boneData);
		if (bone == nullptr)
			return E_FAIL;

		m_Bones.push_back(bone);
	}

	return S_OK;
}

HRESULT Model::Bind_Material(const Shared<Shader>& shader, const Char* constantName, uint32 meshIndex, uint32 materialType, uint32 textureIndex)
{
	return m_Materials[m_Meshes[meshIndex]->Get_MaterialIndex()]->Bind_Material(shader, constantName, materialType, textureIndex);
}

HRESULT Model::Bind_BoneMatrices(const Shared<Shader>& shader, const Char* constantName, uint32 meshIndex)
{
	if (!shader || !constantName)
		return E_INVALIDARG;

	const Shared<SkeletalMesh> mesh = Get_SkeletalMesh(meshIndex);
	if (!mesh)
		return E_INVALIDARG;

	Update_SkinMatrices();
	// 팔레트 전체(본이 없으면 단위 행렬 한 칸)를 올린다. 뒤쪽은 지우지 않으므로 정점 인덱스는 이 안에 있어야 한다.
	const vector<Matrix>& palette = m_SkinMatrices[meshIndex];
	return shader->Bind_Matrices(constantName, palette.data(), static_cast<uint32>(palette.size()));
}

void Model::Update_SkinMatrices()
{
	// Without an animation update (edit mode) the combined matrices were never built; use the bind pose.
	if (!m_HasPose)
	{
		for (auto& bone : m_Bones)
			bone->Update_CombinedTransformationMatrix(m_Bones, m_PreLocalTransformMatrix);
		m_HasPose = true;
		m_SkinDirty = true;
	}
	if (!m_SkinDirty)
		return;

	m_SkinMatrices.resize(m_Meshes.size());
	for (uint32 i = 0; i < m_Meshes.size(); ++i)
	{
		const Shared<SkeletalMesh> mesh = Get_SkeletalMesh(i);
		if (!mesh)
			continue;
		// 본이 없는 메시도 한 칸(단위 행렬)은 둔다. Bind_BoneMatrices는 이 크기만큼 올린다.
		m_SkinMatrices[i].resize(std::max(1u, mesh->Get_NumMeshBones()));
		mesh->Fill_BoneMatrices(m_Bones, m_SkinMatrices[i].data());
	}
	m_SkinDirty = false;
}

Shared<SkeletalMesh> Model::Get_SkeletalMesh(uint32 meshIndex) const
{
	if (!m_IsSkeletal || meshIndex >= m_Meshes.size() || !m_Meshes[meshIndex])
		return nullptr;

	return static_pointer_cast<SkeletalMesh>(m_Meshes[meshIndex]);
}

Shared<Model> Model::CreatePrototype()
{
	auto model = make_shared<Model>(GAME_INSTANCE->Get_Device(), GAME_INSTANCE->Get_Context());

	if (FAILED(model->Initialize_Prototype()))
	{
		MSG_BOX("Failed To CreatePrototype Model");
		return nullptr;
	}

	return model;
}

Shared<Model> Model::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context, const tChar* path, const Matrix& preLocalTransformMatrix)
{
	auto model = make_shared<Model>(device, context);

	if (FAILED(model->Initialize_Prototype(path, preLocalTransformMatrix)))
	{
		LOG_ERROR(L"Failed To Create Model Resource Path : {}", path);
		MSG_BOX("Failed To Create Model Resource");
		return nullptr;
	}

	return model;
}

Shared<Component> Model::Clone(void* arg)
{
	if (arg == nullptr)
	{
		LOG_ERROR(L"There is no ModelDesc");
		MSG_BOX("There is no ModelDesc");
		return nullptr;
	}

	MODEL_DESC& desc = *static_cast<MODEL_DESC*>(arg);

	// An empty tag is a model with no resource yet; the owner fills it from its saved ModelTag (WorldMap tiles).
	if (desc.modelTag.empty())
	{
		auto model = make_shared<Model>(*this);
		return SUCCEEDED(model->Initialize(arg)) ? model : nullptr;
	}

	int32 levIndex = GAME_INSTANCE->Get_ContainLevelByModelTag(desc.modelTag);
	if (levIndex == -1)
	{
		LOG_ERROR(L"Failed to find Model {} in level searching", desc.modelTag);
		return nullptr;
	}

	auto prototype = GAME_INSTANCE->Get_Model(levIndex, desc.modelTag.c_str());

	auto model = make_shared<Model>(*prototype);


	if (FAILED(model->Initialize(arg)))
	{
		MSG_BOX("Failed To Clone Model");
		return nullptr;
	}

	return model;
}
