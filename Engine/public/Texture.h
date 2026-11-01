#pragma once
#include "Component.h"

NS_BEGIN(Engine)

class Shader;

class ENGINE_DLL Texture final : public Component, enable_shared_from_this<Texture> 
{
public:
	typedef struct tagTextureDesc : public COMPONENT_DESC {
		tagTextureDesc(){}
		tagTextureDesc(uint32 levIndex, const wstring& textureTag) : m_levIndex(levIndex), m_TextureTag(textureTag) {}
		tagTextureDesc(uint32 levIndex, const wstring& filePath, uint32 numSRVs) : m_levIndex(levIndex), m_FilePath(filePath), m_NumSRVs(numSRVs) {}

		uint32 m_levIndex{};
		wstring m_TextureTag{};
		wstring m_FilePath{};
		uint32 m_NumSRVs{};
		Color m_RGBA{};
	} TEXTURE_DESC;

public:
	Texture();
	Texture(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Texture(const Texture& rhs);
	~Texture() override = default;

public:
	Color Get_RGBAByValue() const { return m_RGBA; }
	const Color* Get_RGBA_Absolute() const { 
		return &m_RGBA;
	}
	void Set_RGBA(Color rgba)
	{
		if (rgba.x <= 0.f) m_RGBA.x = 0.f;
		else if (rgba.x >= 255.f) m_RGBA.x = 255.f;
		else m_RGBA.x = rgba.x;

		if (rgba.y <= 0.f) m_RGBA.y = 0.f;
		else if (rgba.y >= 255.f) m_RGBA.y = 255.f;
		else m_RGBA.y = rgba.y;

		if (rgba.z <= 0.f) m_RGBA.z = 0.f;
		else if (rgba.z >= 255.f) m_RGBA.z = 255.f;
		else m_RGBA.z = rgba.z;

		if (rgba.w <= 0.f) m_RGBA.w = 0.f;
		else if (rgba.w >= 255.f) m_RGBA.w = 255.f;
		else m_RGBA.w = rgba.w;
	}

public:
	COMPONENT_TYPE Get_ComponentType() const override { return COMPONENT_TYPE::TEXTURE; }
	HRESULT Initialize_Prototype(const tChar* textureFilePath, uint32 numSRVs, const wstring& textureTag);
	HRESULT	Initialize_Prototype() override;
	HRESULT Initialize(void *arg) override;
	void On_Destroy() override;

public:
	wstring Get_TextureFilePath() const { return m_FilePath; }
	uint32 Get_TextureNumSRVs() const { return m_NumSRVs; }
	HRESULT Bind_ShaderResourceView(const Shared<Shader> &shader, const Char *constantName, uint32 index);

	/* 씬 직렬화용 태그 접근자 */
	const wstring& Get_TextureTag() const { return m_TextureTag; }
	void Set_TextureTag(const wstring& tag);

public:
	const vector<ComPtr<ID3D11ShaderResourceView>>& Get_Textures() { return m_SRVs; }

private:
	uint32 m_levIndex{};
	wstring m_FilePath{};
	uint32 m_NumSRVs{};
	Color m_RGBA{};
	wstring m_TextureTag{};		/* 씬 직렬화용 태그 */
	vector<ComPtr<ID3D11ShaderResourceView>> m_SRVs;

public:
	static Shared<Texture> CreatePrototype();
	static Shared<Texture> Create(const ComPtr<ID3D11Device>& device,
								const ComPtr<ID3D11DeviceContext>& context,
                                const tChar* textureFilePath, uint32 numSRVs, const wstring& textureTag);
	Shared<Component> Clone(void *arg = nullptr) override;
};

NS_END