#include "Texture.h"
#include <tchar.h>

#include "Shader.h"
#include "Game.h"
#include "GameObject.h"
#include "SpdLogger.h"

Texture::Texture() : Component {} {}

Texture::Texture(const ComPtr<ID3D11Device> &device, const ComPtr<ID3D11DeviceContext> &context)
    : Component{ device, context }, m_RGBA{ Vector4::One } {
}

Texture::Texture(const Texture& rhs)
	: Component{ rhs }, m_NumSRVs{ rhs.m_NumSRVs }, m_SRVs{ rhs.m_SRVs },
	m_FilePath{ rhs.m_FilePath }, m_RGBA{ rhs.m_RGBA }, m_TextureTag{ rhs.m_TextureTag }, m_levIndex{ rhs.m_levIndex } {
}

HRESULT Texture::Initialize_Prototype(const tChar* textureFilePath, uint32 numSRVs, const wstring& textureTag)
{
    return Component::Initialize_Prototype();
}

HRESULT Texture::Initialize_Prototype()
{
	return Component::Initialize_Prototype();
}

HRESULT Texture::Initialize(void* arg)
{
    if (arg == nullptr)
    {
        LOG_ERROR(L"There is no TextureDesc");
        MSG_BOX("There is no TextureDesc");
        return E_FAIL;
    }

    TEXTURE_DESC& desc = *static_cast<TEXTURE_DESC*>(arg);
    m_levIndex = desc.m_levIndex;
    m_SRVs.shrink_to_fit();

    if (!desc.m_TextureTag.empty())
    {
        const TEXTURE_DESC* registered = GAME_INSTANCE->Get_TextureDesc(desc.m_levIndex, desc.m_TextureTag);
        if (registered == nullptr)
            return E_FAIL;   // Get_TextureDesc가 이미 태그를 로그로 남긴다.
        const TEXTURE_DESC& registDesc = *registered;

        m_NumSRVs = registDesc.m_NumSRVs;
        m_FilePath = registDesc.m_FilePath;
        m_SRVs.clear();
        m_SRVs.reserve(m_NumSRVs);
        
        for (uint32 i = 0; i < m_NumSRVs; ++i)
        {
            tChar szFullPath[MAX_PATH] = TEXT("");
            _stprintf_s(szFullPath, m_FilePath.c_str(), i);
            const ComPtr<ID3D11ShaderResourceView>& srv = GAME_INSTANCE->Get_Texture(desc.m_levIndex, szFullPath);

            if (srv == nullptr) {
                LOG_ERROR(L"[Texture] : Failed to find Tag '{}' Texture At '{}'", desc.m_TextureTag, szFullPath);
                return E_FAIL;
            }
            m_SRVs.push_back(srv);
        }
        m_TextureTag = desc.m_TextureTag;
    }
   
    return Component::Initialize(arg);
}

void Texture::On_Destroy() {
	m_SRVs.clear();

	Component::On_Destroy();
}

HRESULT Texture::Bind_ShaderResourceView(const Shared<Shader>& shader, const Char* constantName, uint32 index) {
	if (index >= m_NumSRVs) {
		return E_INVALIDARG;
	}

	return shader->Bind_SRV(constantName, m_SRVs[index]);
}


void Texture::Set_TextureTag(const wstring& tag)
{
    if (tag.empty()) {
        m_TextureTag.clear();
        m_FilePath.clear();
        m_NumSRVs = 0;
        m_SRVs.clear();
        return;
    }

	// 1. 현재 레벨에서 검색
	const auto owner = Get_Owner();
    const uint32 ownerLevel = owner ? owner->Get_LevelIndex() : m_levIndex;
    const TEXTURE_DESC* pDesc = GAME_INSTANCE->Get_TextureDesc(ownerLevel, tag);
    if (!pDesc && ownerLevel != m_levIndex)
        pDesc = GAME_INSTANCE->Get_TextureDesc(m_levIndex, tag);
    // An editor image can switch back from a Static texture to its scene's texture.
    const uint32 currentLevel = GAME_INSTANCE->Get_CurrentLevelIndex();
    if (!pDesc && currentLevel != m_levIndex)
        pDesc = GAME_INSTANCE->Get_TextureDesc(currentLevel, tag);

	// 2. 못 찾았으면 Static 레벨(0)에서 검색
	if (pDesc == nullptr && m_levIndex != 0)
	{
		pDesc = GAME_INSTANCE->Get_TextureDesc(0, tag);
	}

	if (pDesc == nullptr)
	{
		LOG_ERROR(L"[Texture] Failed to find TextureDesc for Tag: {}", tag);
		return;
	}

	m_NumSRVs = pDesc->m_NumSRVs;
	m_FilePath = pDesc->m_FilePath;
	m_SRVs.clear();
	m_SRVs.reserve(m_NumSRVs);

	for (uint32 i = 0; i < m_NumSRVs; ++i)
	{
		tChar szFullPath[MAX_PATH] = TEXT("");
		_stprintf_s(szFullPath, m_FilePath.c_str(), i);
		
		// 레벨 인덱스도 체크하여 가져옴
		const ComPtr<ID3D11ShaderResourceView>& srv = GAME_INSTANCE->Get_Texture(pDesc->m_levIndex, szFullPath);

		if (srv == nullptr)
		{
			LOG_ERROR(L"[Texture] : Failed to find Texture SRV At '{}'", szFullPath);
			continue;
		}
		m_SRVs.push_back(srv);
	}

	m_TextureTag = tag;
    if (m_levIndex != pDesc->m_levIndex)
    {
        m_levIndex = pDesc->m_levIndex; // 실제 리소스가 있는 레벨로 업데이트
        LOG_INFO(L"[Texture] Tag '{}' assigned from Level {}", tag, m_levIndex);
    }
}

Shared<Texture> Texture::CreatePrototype()
{
    auto texture = make_shared<Texture>(
        GAME_INSTANCE->Get_Device(), GAME_INSTANCE->Get_Context());

    if (FAILED(texture->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : Texture");
        return nullptr;
    }

    return texture;
}

Shared<Texture> Texture::Create(const ComPtr<ID3D11Device> &device,
                                const ComPtr<ID3D11DeviceContext> &context,
                                const tChar* textureFilePath, uint32 numSRVs, const wstring& textureTag)
{
	auto texture = make_shared<Texture>(device, context);

	if (FAILED(texture->Initialize_Prototype(textureFilePath, numSRVs, textureTag))) {
		MSG_BOX("Failed to Created : Texture");
	}

	return texture;
}

Shared<Component> Texture::Clone(void* arg) 
{
    auto texture = make_shared<Texture>(*this);

    if (FAILED(texture->Initialize(arg))) 
    {
    	MSG_BOX("Failed to Created : Texture");
    }

    return texture;
}
