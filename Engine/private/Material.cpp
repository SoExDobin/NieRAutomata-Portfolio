#include "Material.h"

#include "Game.h"
#include "Shader.h"
#include "SpdLogger.h"

Material::Material() : Component{} {}
Material::Material(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: Component{device, context}
{
}
Material::Material(const Material& rhs)
	: Component{rhs}, m_MaterialName{rhs.m_MaterialName}, m_TextureTypeMax{rhs.m_TextureTypeMax},
	  m_TextureMask{rhs.m_TextureMask}, m_MaterialTextures{rhs.m_MaterialTextures},
	  m_NamedTextures{rhs.m_NamedTextures}, m_Parameters{rhs.m_Parameters}
{
}

namespace
{
	// Material::Load_SharedTexture cache, keyed by the lower-case absolute path. Levels load on a worker thread.
	std::mutex g_TextureCacheMutex;
	std::unordered_map<wstring, ComPtr<ID3D11ShaderResourceView>> g_TextureCache;

	// ModelConverter stores the NieR g_MaskMap (R metal, G gloss, B cavity) in this slot.
	constexpr uint32 MASK_TEXTURE_TYPE = 15; // aiTextureType_METALNESS
	constexpr const Char* MASK_TEXTURE_SLOT = "g_MaskMap";

	uint32 To_MaterialTextureMask(uint32 textureTypeIndex)
	{
		switch (textureTypeIndex)
		{
		case 1:  return MATERIAL_TEXTURE_BASE_COLOR; // aiTextureType_DIFFUSE
		case 4:  return MATERIAL_TEXTURE_EMISSIVE;   // aiTextureType_EMISSIVE
		case 6:  return MATERIAL_TEXTURE_NORMAL;     // aiTextureType_NORMALS
		case 8:  return MATERIAL_TEXTURE_OPACITY;    // aiTextureType_OPACITY
		case 14: return MATERIAL_TEXTURE_EMISSIVE;   // aiTextureType_EMISSION_COLOR
		case 15: // aiTextureType_METALNESS
		case 16: // aiTextureType_DIFFUSE_ROUGHNESS
		case 17: return MATERIAL_TEXTURE_ORM;        // aiTextureType_AMBIENT_OCCLUSION
		default: return 0;
		}
	}
}

void Material::On_Destroy()
{
	for (size_t i = 0; i < m_TextureTypeMax; i++)
	{
		m_MaterialTextures[i].clear();
	}

	Component::On_Destroy();
}

HRESULT Material::Initialize_Prototype(const MODEL_MATERIAL& materialData)
{
	m_MaterialName = materialData.name;
	m_TextureTypeMax = materialData.textureTypeMax;
	m_MaterialTextures = make_shared<vector<ComPtr<ID3D11ShaderResourceView>>[]>(m_TextureTypeMax);


	for (const auto& texEntry : materialData.textures)
	{
		uint32 typeIndex = texEntry.typeIndex;
		// 유효한 텍스처 타입 인덱스인지 안전 검사
		if (typeIndex >= m_TextureTypeMax)
			continue;
		filesystem::path texturePath = filesystem::path(materialData.directoryPath) / texEntry.path;
		if (!filesystem::is_regular_file(texturePath))
		{
			texturePath = filesystem::path(materialData.directoryPath) /
				"Textures" / filesystem::path(texEntry.path).filename();
		}
		const string extension = texturePath.extension().string();
		const wstring textureFilePath = texturePath.wstring();
		// 컨버터가 저장한 텍스처 파일 이름과 확장자 분리
		// Model에서 주입해준 디렉토리 경로 + 텍스처 파일명 + 확장자로 절대/상대 경로 완성

		// DirectX 텍스처 로더는 wstring을 요구하므로 변환

		HRESULT hr = S_OK;
		ComPtr<ID3D11ShaderResourceView> srv = nullptr;
		
		if (false == _stricmp(extension.c_str(), ".tga"))
		{	
			LOG_ERROR(L"TGA File Service not available");
			hr = E_FAIL;
		}
		else
		{
			hr = Load_SharedTexture(m_Device.Get(), texturePath, srv);
		}
		
		if (FAILED(hr))
		{
			LOG_ERROR(L"Failed To Load Texture : {}", textureFilePath);
			continue; // 혹은 return E_FAIL;
		}
		

		m_MaterialTextures[typeIndex].push_back(srv);
		m_TextureMask |= To_MaterialTextureMask(typeIndex);
	}

	return Component::Initialize_Prototype();
}

HRESULT Material::Initialize_Prototype()
{
	return Component::Initialize_Prototype();
}


HRESULT Material::Initialize(void* arg)
{
	return Component::Initialize(arg);
}

HRESULT Material::Bind_Material(const Shared<Shader>& shader, const Char* constantName, uint32 textureTypeIndex, uint32 textureIndex)
{
	if (!shader || textureTypeIndex >= m_TextureTypeMax || !m_MaterialTextures ||
		textureIndex >= m_MaterialTextures[textureTypeIndex].size() ||
		nullptr == m_MaterialTextures[textureTypeIndex][textureIndex])
		return E_FAIL;

	if (FAILED(shader->Bind_SRV(constantName, m_MaterialTextures[textureTypeIndex][textureIndex])))
	{
		LOG_ERROR(L"Failed to Bind srv to {}", Helper::To_wString(constantName));
		return E_FAIL;
	}

	if (shader->Supports_CBuffer(ConstantBuffer::Material))
	{
		MaterialCB materialBuffer{};
		materialBuffer.textureMask = m_TextureMask;
		if (FAILED(shader->Bind_CBufferData(materialBuffer)))
			return E_FAIL;
	}

	return S_OK;
}

HRESULT Material::Prepare_NamedTextures(const vector<pair<string, filesystem::path>>& textures,
	const MaterialParameters& parameters)
{
	vector<pair<string, ComPtr<ID3D11ShaderResourceView>>> candidate;
	uint32 mask = 0;
	for (const auto& [slot, path] : textures)
	{
		ComPtr<ID3D11ShaderResourceView> srv;
		const HRESULT hr = Load_SharedTexture(m_Device.Get(), path, srv);
		if (FAILED(hr))
		{
			LOG_ERROR(L"Failed to prepare material texture {} : {}", Helper::To_wString(slot), path.wstring());
			return hr;
		}
		candidate.emplace_back(slot, std::move(srv));
		if (slot == "g_AlbedoMap") mask |= MATERIAL_TEXTURE_BASE_COLOR;
		else if (slot == "g_NormalMap") mask |= MATERIAL_TEXTURE_NORMAL;
		else if (slot == MASK_TEXTURE_SLOT) mask |= MATERIAL_TEXTURE_ORM;
		else if (slot == "g_EmissiveMap") mask |= MATERIAL_TEXTURE_EMISSIVE;
		else if (slot == "g_OpacityMap") mask |= MATERIAL_TEXTURE_OPACITY;
	}
	m_NamedTextures = std::move(candidate);
	m_TextureMask = mask;
	m_Parameters = parameters;
	return S_OK;
}

HRESULT Material::Bind_NamedTextures(const Shared<Shader>& shader) const
{
	if (!shader) return E_INVALIDARG;
	for (const auto& [slot, srv] : m_NamedTextures)
		if (FAILED(shader->Bind_SRV(slot.c_str(), srv))) return E_FAIL;
	return shader->Bind_CBufferData(m_Parameters.To_MaterialCB(m_TextureMask));
}

HRESULT Material::Bind_DefaultTexture(const Shared<Shader>& shader, Bool alphaClip) const
{
	if (!shader) return E_INVALIDARG;
	ComPtr<ID3D11ShaderResourceView> srv;
	if (m_MaterialTextures && m_TextureTypeMax > 1 && !m_MaterialTextures[1].empty())
		srv = m_MaterialTextures[1][0];
	if (FAILED(shader->Bind_SRV("g_AlbedoMap", srv))) return E_FAIL;
	MaterialCB data{};
	data.textureMask = srv ? MATERIAL_TEXTURE_BASE_COLOR : 0;
	if (!alphaClip)
		data.opacityMaskClipValue = 0.f;

	// Bound every time, null included, so the previous material's mask never carries over.
	if (shader->Has_SRV(MASK_TEXTURE_SLOT))
	{
		ComPtr<ID3D11ShaderResourceView> mask;
		if (m_MaterialTextures && m_TextureTypeMax > MASK_TEXTURE_TYPE && !m_MaterialTextures[MASK_TEXTURE_TYPE].empty())
			mask = m_MaterialTextures[MASK_TEXTURE_TYPE][0];
		if (FAILED(shader->Bind_SRV(MASK_TEXTURE_SLOT, mask))) return E_FAIL;
		if (mask) data.textureMask |= MATERIAL_TEXTURE_ORM;
	}
	return shader->Bind_CBufferData(data);
}

HRESULT Material::Load_SharedTexture(ID3D11Device* device, const filesystem::path& path,
	ComPtr<ID3D11ShaderResourceView>& outSRV)
{
	std::error_code error;
	wstring key = filesystem::absolute(path, error).lexically_normal().wstring();
	std::ranges::transform(key, key.begin(), [](wchar_t ch) { return static_cast<wchar_t>(::towlower(ch)); });
	{
		std::lock_guard lock(g_TextureCacheMutex);
		if (const auto found = g_TextureCache.find(key); found != g_TextureCache.end())
		{
			outSRV = found->second;
			return S_OK;
		}
	}
	ComPtr<ID3D11ShaderResourceView> srv;
	const HRESULT hr = _wcsicmp(path.extension().c_str(), L".dds") == 0
		? CreateDDSTextureFromFile(device, path.c_str(), nullptr, srv.GetAddressOf())
		: CreateWICTextureFromFile(device, path.c_str(), nullptr, srv.GetAddressOf());
	if (FAILED(hr))
		return hr;
	std::lock_guard lock(g_TextureCacheMutex);
	// Another thread may have loaded the same file meanwhile; keep the first so both share it.
	outSRV = g_TextureCache.emplace(std::move(key), srv).first->second;
	return S_OK;
}

void Material::Release_TextureCache(Bool releaseAll)
{
	std::lock_guard lock(g_TextureCacheMutex);
	if (releaseAll)
	{
		g_TextureCache.clear();
		return;
	}
	for (auto it = g_TextureCache.begin(); it != g_TextureCache.end();)
	{
		// Reference count 1 = only the cache holds it.
		it->second->AddRef();
		if (it->second->Release() == 1)
			it = g_TextureCache.erase(it);
		else
			++it;
	}
}

Shared<Material> Material::CreatePrototype()
{
	auto material = make_shared<Material>(
		GAME_INSTANCE->Get_Device(), GAME_INSTANCE->Get_Context());

	return material;
}

Shared<Material> Material::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context, const MODEL_MATERIAL& modelMaterial)
{
	auto material = make_shared<Material>(device, context);

	if (FAILED(material->Initialize_Prototype(modelMaterial)))
	{
		LOG_ERROR(L"Failed to Created : Material {}", Helper::To_wString(modelMaterial.name));
		MSG_BOX("Failed to Created : Material");
	}
	return material;
}
