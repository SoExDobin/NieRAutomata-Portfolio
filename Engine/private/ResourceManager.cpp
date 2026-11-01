#include "ResourceManager.h"

#include <fstream>
#include <tchar.h>
#include "SpdLogger.h"
#include "Game.h"
#include "Material.h"

ResourceManager::ResourceManager(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context) 
    : m_Device{ device }, m_Context{ context } {}

ResourceManager::~ResourceManager() {}

HRESULT ResourceManager::Initialize_Prototype(uint32 levCount)
{
	std::lock_guard<std::recursive_mutex> lock(m_ResourceMutex);

    m_LevelCount = levCount;
    m_Shaders.resize(m_LevelCount);
    m_TextureDescTags.resize(m_LevelCount);
    m_SRVs.resize(m_LevelCount);
    m_Models.resize(m_LevelCount);

	return EngineManager::Initialize_Prototype();
}

void ResourceManager::On_Destroy()
{
    for (auto& shaders : m_Shaders)
        shaders.clear();
    m_Shaders.clear();

    for (auto& model : m_Models)
        model.clear();
    m_Models.clear();
    Material::Release_TextureCache(true);

    for (auto& textureDescs : m_TextureDescTags)
        textureDescs.clear();
    m_TextureDescTags.clear();

    for (auto& SRVs : m_SRVs)
        SRVs.clear();
    m_SRVs.clear();

	EngineManager::On_Destroy();
}

HRESULT ResourceManager::Load_Texture(uint32 levIndex, const tChar* texturePath, uint32 numSRVs, const wstring& descriptionTag, Bool allowMissing)
{
    // Textures load on JobManager workers in parallel: files are read outside m_ResourceMutex, only the maps are locked.
    if (levIndex >= m_LevelCount || !texturePath || !*texturePath || !numSRVs)
        return E_INVALIDARG;

    Texture::TEXTURE_DESC desc{};
    desc.m_levIndex = levIndex;
    desc.m_FilePath = texturePath;
    desc.m_NumSRVs = numSRVs;
    desc.m_TextureTag = descriptionTag;

    vector<pair<wstring, ComPtr<ID3D11ShaderResourceView>>> prepared;
    ComPtr<ID3D11ShaderResourceView> fallback;
    uint32 fallbackCount = 0;

    for (uint32 i = 0; i < numSRVs; ++i) {
    	tChar szFullPath[MAX_PATH] = TEXT("");
        if (_stprintf_s(szFullPath, texturePath, i) < 0)
            return E_INVALIDARG;

        std::error_code pathError;
        const Bool exists = filesystem::is_regular_file(szFullPath, pathError);
        if (pathError && pathError != std::errc::no_such_file_or_directory)
            return HRESULT_FROM_WIN32(pathError.value());
        if (!exists && !allowMissing)
            return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);

        {
            std::lock_guard<std::recursive_mutex> lock(m_ResourceMutex);
            if (m_SRVs[levIndex].contains(szFullPath))
                continue;
        }

        if (!exists)
        {
            if (!fallback)
            {
                const uint32 transparentPixel = 0;
                D3D11_TEXTURE2D_DESC fallbackDesc{};
                fallbackDesc.Width = fallbackDesc.Height = 1;
                fallbackDesc.MipLevels = fallbackDesc.ArraySize = 1;
                fallbackDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                fallbackDesc.SampleDesc.Count = 1;
                fallbackDesc.Usage = D3D11_USAGE_IMMUTABLE;
                fallbackDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                D3D11_SUBRESOURCE_DATA initialData{};
                initialData.pSysMem = &transparentPixel;
                initialData.SysMemPitch = sizeof(transparentPixel);
                ComPtr<ID3D11Texture2D> fallbackTexture;
                HRESULT hr = m_Device->CreateTexture2D(&fallbackDesc, &initialData,
                    fallbackTexture.GetAddressOf());
                if (FAILED(hr)) return hr;
                hr = m_Device->CreateShaderResourceView(fallbackTexture.Get(), nullptr,
                    fallback.GetAddressOf());
                if (FAILED(hr)) return hr;
            }
            prepared.emplace_back(szFullPath, fallback);
            ++fallbackCount;
            continue;
        }

        tChar szDrive[MAX_PATH] = TEXT("");
        tChar szDir[MAX_PATH] = TEXT("");
        tChar szName[MAX_PATH] = TEXT("");
        tChar szExt[MAX_PATH] = TEXT("");

        _tsplitpath_s(szFullPath, szDrive, MAX_PATH, szDir, MAX_PATH, szName,
            MAX_PATH, szExt, MAX_PATH);

        HRESULT hr = {};
        ComPtr<ID3D11Resource> texture{ nullptr };
        ComPtr<ID3D11ShaderResourceView> srv{ nullptr };

        if (!lstrcmp(szExt, TEXT(".dds"))) {
            hr = CreateDDSTextureFromFile(m_Device.Get(), szFullPath,
                texture.GetAddressOf(),
                srv.GetAddressOf());
        }
        else if (!lstrcmp(szExt, TEXT(".tga"))) {
            MSG_BOX("TGA Texture Loading Not Supported Yet");
            return E_FAIL;
        }
        else {
            hr = CreateWICTextureFromFile(m_Device.Get(), szFullPath,
                texture.GetAddressOf(),
                srv.GetAddressOf());
        }

        if (FAILED(hr)) { return hr; }

        prepared.emplace_back(szFullPath, srv);
    }

    {
        std::lock_guard<std::recursive_mutex> lock(m_ResourceMutex);
        for (auto& [path, srv] : prepared)
            m_SRVs[levIndex].emplace(path, std::move(srv));
        m_TextureDescTags[levIndex].insert_or_assign(descriptionTag, desc);
    }
    if (fallbackCount)
        LOG_WARN(L"[TextureSettings fallback] '{}' level {}: {}/{} missing images at '{}'; using transparent UI/Sprite texture. Add the files and reload the level/application.",
            descriptionTag, levIndex, fallbackCount, numSRVs,
            filesystem::absolute(texturePath).lexically_normal().wstring());
    return fallbackCount ? S_FALSE : S_OK;
}

const Texture::TEXTURE_DESC* ResourceManager::Get_TextureDescByTag(uint32 levIndex, const wstring& descriptionTag)
{
    if (m_TextureDescTags[0].contains(descriptionTag))
        return &m_TextureDescTags[0][descriptionTag];

    if (false == m_TextureDescTags[levIndex].contains(descriptionTag))
    {
        LOG_ERROR(L" \"{}\" has no Texture Description", descriptionTag);
        return nullptr;
    }

    return &m_TextureDescTags[levIndex][descriptionTag];
}

vector<wstring> ResourceManager::Get_TextureTags(uint32 levIndex)
{
    vector<wstring> tags;
    for (auto& [tag, desc] : m_TextureDescTags[levIndex])
        tags.push_back(tag);
    return tags;
}


const ComPtr<ID3D11ShaderResourceView>& ResourceManager::Get_Texture(uint32 levIndex, const tChar* texturePath)
{
    if (m_SRVs[0].contains(texturePath))
        return m_SRVs[0][texturePath];

    if (m_SRVs[levIndex].contains(texturePath) == false)
    {
        LOG_ERROR(L"{} is not texture path", texturePath);
    }

    return m_SRVs[levIndex][texturePath];
}

HRESULT ResourceManager::Load_Shader(uint32 levIndex, const tChar* shaderPath, const D3D11_INPUT_ELEMENT_DESC* elements, uint32 numElements, const wstring& descriptionTag)
{
    auto shader = Shader::Create(GAME_INSTANCE->Get_Device(), GAME_INSTANCE->Get_Context(), shaderPath, elements, numElements);
    if (nullptr == shader)
    {
        LOG_ERROR(L"Failed to Create Shader Resource. Path : {}", shaderPath);
        return E_FAIL;
    }

    {
        std::lock_guard<std::recursive_mutex> lock(m_ResourceMutex);
        m_Shaders[levIndex].emplace(descriptionTag, shader);
    }
    Shader::SHADER_DESC desc{};
    desc.m_VertexTag = shaderPath;
    desc.m_elementsDesc = elements;
    desc.m_numPasses = numElements;

    return S_OK;
}

HRESULT ResourceManager::Load_Shader(uint32 levIndex, const tChar* shaderPath)
{
    // "Shader_<VtxLayout>[_<Variant>].hlsl": the layout token picks the input layout, and a suffix after
    // it registers another shader of that layout as "<layout tag><suffix>"
    // (Shader_VtxAnimMesh_Weapon.hlsl -> VTX_Animation_Mesh_Weapon). Longer tokens come first.
    struct SHADER_LAYOUT
    {
        const tChar* token;
        const D3D11_INPUT_ELEMENT_DESC* elements;
        uint32 numElements;
        const wstring& tag;
    };
    const SHADER_LAYOUT layouts[] =
    {
        { L"vtxanimmesh", VTXANIMMESH::Elements, VTXANIMMESH::numElements, VTXANIMMESH::Tag },
        { L"vtxmeshinstance", VTXMESH_INSTANCE::Elements, VTXMESH_INSTANCE::numElements, VTXMESH_INSTANCE::Tag },
        { L"vtxmesh", VTXMESH::Elements, VTXMESH::numElements, VTXMESH::Tag },
        { L"vtxnormtex", VTXNORMTEX::Elements, VTXNORMTEX::numElements, VTXNORMTEX::Tag },
        { L"vtxtex", VTXTEX::Elements, VTXTEX::numElements, VTXTEX::Tag },
        { L"vtxcube", VTXCUBE::Elements, VTXCUBE::numElements, VTXCUBE::Tag },
        { L"vtxsphere", VTXSPHERE::Elements, VTXSPHERE::numElements, VTXSPHERE::Tag },
        { L"vtxparticlepoint", VTXPARTICLE_POINT_DESC::Elements, VTXPARTICLE_POINT_DESC::numElements, VTXPARTICLE_POINT_DESC::Tag },
        { L"vtxparticlerect", VTXPARTICLE_RECT_DESC::Elements, VTXPARTICLE_RECT_DESC::numElements, VTXPARTICLE_RECT_DESC::Tag },
    };
    const auto caseInsensitiveCompare = [](tChar a, tChar b) { return ::towlower(a) == ::towlower(b); };

    const wstring stem = filesystem::path(shaderPath).stem().wstring();
    for (const SHADER_LAYOUT& layout : layouts)
    {
        const wstring_view token(layout.token);
        const auto found = std::search(stem.begin(), stem.end(), token.begin(), token.end(), caseInsensitiveCompare);
        if (found != stem.end())
            return Load_Shader(levIndex, shaderPath, layout.elements, layout.numElements, layout.tag + wstring(found + token.size(), stem.end()));
    }

    LOG_WARN(L"Shader File {} Does Not Follow Naming Convention", shaderPath);
    return E_FAIL;
}

vector<wstring> ResourceManager::Get_ShaderTags(uint32 levIndex) const
{
    vector<wstring> tags;
    if (levIndex < m_Shaders.size())
        for (const auto& [tag, shader] : m_Shaders[levIndex])
            tags.push_back(tag);
    std::ranges::sort(tags);
    return tags;
}

Shared<Shader> ResourceManager::Get_Shader(uint32 levIndex, const tChar* vertexTag)
{
    if (m_Shaders[levIndex].contains(vertexTag) == false)
    {
        LOG_ERROR(L"{} is not shader Tag", vertexTag);
        return nullptr;
    }

    return m_Shaders[levIndex][vertexTag];
}

HRESULT ResourceManager::Load_Model(uint32 levIndex, const tChar* modelPath, const wstring& descriptionTag,
	const Matrix& preTransformMatrix, const tChar* materialSettingsPath, const wstring& replacedTag)
{
    auto model = Model::Create(GAME_INSTANCE->Get_Device(), GAME_INSTANCE->Get_Context(), modelPath, preTransformMatrix);
    if (nullptr == model)
    {
        LOG_ERROR(L"Failed to Create Shader Resource. Path : {}", modelPath);
        return E_FAIL;
    }
	if (materialSettingsPath && *materialSettingsPath &&
		FAILED(model->Load_MaterialSettings(filesystem::path(materialSettingsPath), descriptionTag)))
	{
		LOG_ERROR(L"Failed to load material settings for {} : {}", descriptionTag, materialSettingsPath);
		return E_FAIL;
	}

    // Models load on JobManager workers in parallel; only registration takes the lock.
    std::lock_guard<std::recursive_mutex> lock(m_ResourceMutex);
    // A tag shared by two stages (wp0070, CityOfRuinManHole) belongs to the level loaded last: the next level
    // loads before the previous one is cleared, and that clear must not drop the new level's entry.
    if (levIndex == 0)
        m_StaticModelContainLev.emplace(descriptionTag, levIndex);
    else
        m_ModelContainLev.insert_or_assign(descriptionTag, levIndex);

    m_Models[levIndex].emplace(descriptionTag, model);
    // Looks the tag up in the maps above, so it runs after registration and under the same lock.
    model->Set_ModelTag(descriptionTag);

    // The replaced tag is only another key for the same prototype; it follows the shared-tag rule above.
    if (!replacedTag.empty())
    {
        if (levIndex == 0)
            m_StaticModelContainLev.emplace(replacedTag, levIndex);
        else
            m_ModelContainLev.insert_or_assign(replacedTag, levIndex);
        m_Models[levIndex].emplace(replacedTag, model);
    }

    return S_OK;
}

HRESULT ResourceManager::Load_ModelAnimations(uint32 levIndex, const wstring& modelTag,
	const vector<wstring>& animationFilePaths)
{
	std::lock_guard<std::recursive_mutex> lock(m_ResourceMutex);
	if (levIndex >= m_Models.size())
		return E_INVALIDARG;

	const auto modelIt = m_Models[levIndex].find(modelTag);
	if (modelIt == m_Models[levIndex].end() || !modelIt->second)
	{
		LOG_ERROR(L"Failed to find model prototype {} in level {}", modelTag, levIndex);
		return E_FAIL;
	}

	return modelIt->second->Load_Animations(animationFilePaths);
}

Shared<Model> ResourceManager::Get_Model(uint32 levIndex, const tChar* modelTag)
{
    std::lock_guard<std::recursive_mutex> lock(m_ResourceMutex);
    if (m_Models[levIndex].contains(modelTag) == false)
    {
        LOG_ERROR(L"{} is not shader Tag", modelTag);
        return nullptr;
    }

    return m_Models[levIndex][modelTag];
}

int32 ResourceManager::Get_ContainLevelByModelTag(const wstring& tag)
{
    std::lock_guard<std::recursive_mutex> lock(m_ResourceMutex);
    if (!m_ModelContainLev.contains(tag))
    {
        if (m_StaticModelContainLev.contains(tag))
        {
            return m_StaticModelContainLev[tag];
        }
        /* -1은 이 질의의 정상적인 반환값이다. WorldCollider와 WorldMap은 <태그>_COL / <태그>_LOD가
           있는지 알아보려고 여기를 부르고, 없는 것이 정상인 자산도 있다. 실패로 취급하는 호출자는
           (Model.cpp:351, :1395, PhysicsManager.cpp:123) 각자 LOG_ERROR를 남기므로 심각도는
           호출자가 정한다. 여기서는 사실만 적는다. */
        LOG_INFO(L"Model {} is not in such level", tag);
        return -1;
    }

    return m_ModelContainLev[tag];
}

vector<Shared<Model>> ResourceManager::Get_Models(uint32 levIndex)
{
    vector<Shared<Model>> vector;
    
    if (levIndex >= m_Models.size())
        return vector;

    for (auto [tag, model] : m_Models[levIndex])
    {
        // A replaced tag repeats a model already listed under its own tag.
        if (model && model->Get_ModelTag() == tag)
            vector.push_back(model);
    }

    return vector;
}

HRESULT ResourceManager::Clear_AllResources() {
    // 레벨 0 (Static)에 로드된 엔진 기본 리소스 및 모델들은 보존합니다.
    // 레벨 1 이상의 게임 레벨 리소스들만 삭제하여 메모리 효율과 안정성을 확보합니다.
    for (uint32 i = 1; i < m_LevelCount; ++i) {
        m_Shaders[i].clear();
        m_TextureDescTags[i].clear();
        m_SRVs[i].clear();
        m_Models[i].clear();
    }

    m_ModelContainLev.clear();
    Material::Release_TextureCache(false);

	return S_OK;
}

HRESULT ResourceManager::Clear_Resource(uint32 levIndex)
{
    if (levIndex >= m_LevelCount)
        return E_FAIL;

    {
        // The next level's Loader is already registering models on JobManager workers while this runs.
        std::lock_guard<std::recursive_mutex> lock(m_ResourceMutex);
        m_Shaders[levIndex].clear();
        m_TextureDescTags[levIndex].clear();
        m_SRVs[levIndex].clear();

        m_Models[levIndex].clear();

        for (auto it = m_ModelContainLev.begin(); it != m_ModelContainLev.end();)
        {
            if (it->second == levIndex)
                it = m_ModelContainLev.erase(it);
            else
                ++it;
        }

        if (levIndex == 0)
        {
            m_StaticModelContainLev.clear();
        }
    }
    Material::Release_TextureCache(false);

    return S_OK;
}

Unique<ResourceManager> ResourceManager::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context, uint32 levCount)
{
	auto resourceManager = make_unique<ResourceManager>(device, context);

	if (FAILED(resourceManager->Initialize_Prototype(levCount)))
	{
		MSG_BOX("Failed to Create ResourceManager");
		return nullptr;
	}

	return resourceManager;
}

