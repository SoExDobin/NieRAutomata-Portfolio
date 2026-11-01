#pragma once

#include "Component.h"

NS_BEGIN(Engine)

class Shader;

class Material final : public Component
{
public:
	explicit Material();
	explicit Material(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Material(const Material& rhs);
	~Material() override = default;

public:
	COMPONENT_TYPE Get_ComponentType() const override { return COMPONENT_TYPE::MATERIAL; }
	const string& Get_MaterialName() const { return m_MaterialName; }
	void On_Destroy() override;
	void On_Disable() override { Component::On_Disable(); }
	void On_Enable() override { Component::On_Enable(); }
	void Set_Active(Bool isActive) override { Component::Set_Active(isActive); }
	
public:
	HRESULT Initialize_Prototype(const MODEL_MATERIAL& materialData);
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;

public:
	HRESULT Bind_Material(const Shared<Shader>& shader, const Char* constantName, uint32 textureTypeIndex, uint32 textureIndex);
	HRESULT Prepare_NamedTextures(const vector<pair<string, filesystem::path>>& textures,
		const MaterialParameters& parameters = {});
	HRESULT Bind_NamedTextures(const Shared<Shader>& shader) const;
	/* alphaClip false binds clip value 0: the shadow pass draws preset-less models (the far _SHADOW
	   proxies) solid, since only preset materials carry a clip value meant for shadows. */
	HRESULT Bind_DefaultTexture(const Shared<Shader>& shader, Bool alphaClip = true) const;

private:
	string m_MaterialName{};
	uint32 m_TextureTypeMax{};
	uint32 m_TextureMask{};
	Shared<vector<ComPtr<ID3D11ShaderResourceView>>[]> m_MaterialTextures;
	vector<pair<string, ComPtr<ID3D11ShaderResourceView>>> m_NamedTextures;
	MaterialParameters m_Parameters{};


public:
	/* Texture files are shared: every material naming the same file (a tile model, its _LOD model and their
	   presets all point at the tile's Textures folder) gets the same SRV, so each file is uploaded once. */
	static HRESULT Load_SharedTexture(ID3D11Device* device, const filesystem::path& path,
		ComPtr<ID3D11ShaderResourceView>& outSRV);
	// Drops cached textures no material holds any more (level unload), or every entry (shutdown).
	static void Release_TextureCache(Bool releaseAll);

public:
	static Shared<Material> CreatePrototype();
	static Shared<Material> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context, const MODEL_MATERIAL& modelMaterial);
	Shared<Component> Clone(void* arg = nullptr) override { return nullptr; }
};

NS_END
