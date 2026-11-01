#include "pch.h"
#include "Terrain.h"

#include "Game.h"
#include "Shader.h"
#include "Texture.h"
#include "Transform.h"
#include "VIBuffer_Terrain.h"
#include <SpdLogger.h>

Terrain::Terrain() : GameObject{} {}
Terrain::Terrain(const ComPtr<ID3D11Device> &device, const ComPtr<ID3D11DeviceContext> &context)
    : GameObject{device, context} {}

Terrain::Terrain(const Terrain &rhs) : GameObject{rhs} {}

HRESULT Terrain::Initialize_Prototype() {

    if (FAILED(GameObject::Initialize_Prototype()))
        return E_FAIL;

    return S_OK;
}

HRESULT Terrain::Initialize(void *arg) {
	if (FAILED(GameObject::Initialize(arg)))
        return E_FAIL;

    if (FAILED(Ready_Components()))
        return E_FAIL;

	return S_OK;
}

void Terrain::On_Destroy()
{
	GameObject::On_Destroy();
}
void Terrain::On_Enable() { GameObject::On_Enable(); }
void Terrain::On_Disable() { GameObject::On_Disable(); }

void Terrain::Priority_Update(Float timeDelta) {}
void Terrain::Update(Float timeDelta) {}
void Terrain::Late_Update(Float timeDelta) {}
void Terrain::Fixed_Update(Float fixedDelta) {}
void Terrain::Submit_RenderGroup()
{
    GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::NONBLEND, shared_from_this());
}

HRESULT Terrain::Render() {
  // VtxNormTex has no ShadowCompatible pass; the terrain does not cast shadows yet.
  if (GAME_INSTANCE->Is_ShadowPass())
    return S_OK;

  if (FAILED(Bind_ShaderResources()))
    return E_FAIL;

  if (FAILED(m_Shader->Begin(0)))
    return E_FAIL;

  if (FAILED(m_VIBuffer->Bind_Resources()))
    return E_FAIL;

  if (FAILED(m_VIBuffer->Render()))
    return E_FAIL;

  return S_OK;
}

HRESULT Terrain::Ready_Components() {
    Shader::SHADER_DESC shaderDesc = Shader::SHADER_DESC{ VTXNORMTEX::Tag,VTXNORMTEX::Elements, VTXNORMTEX::numElements };
    m_Shader = Add_Component<Shader>(ETOI(LEVEL::STATIC), &shaderDesc);
	if (!m_Shader) {
		LOG_ERROR(L"Failed To Add Component Shader To Terrain");
		return E_FAIL;
	}
    Texture::TEXTURE_DESC textureDesc = Texture::TEXTURE_DESC{ 0, L"Tile_0" };
	m_Texture = Add_Component<Texture>(ETOI(LEVEL::STATIC), &textureDesc);
	if (!m_Texture) {
		LOG_ERROR(L"Failed To Add Component Texture To Terrain");
		return E_FAIL;
	}
	m_VIBuffer = Add_Component<VIBuffer_Terrain>();
	if (!m_VIBuffer) {
		LOG_ERROR(L"Failed To Add Component TerrainBuffer To Terrain");
		return E_FAIL;
	}

  return S_OK;
}

HRESULT Terrain::Bind_ShaderResources() {
    if (FAILED(m_Transform->Bind_ShaderResource(m_Shader, "g_WorldMatrix")))
      return E_FAIL;
    if (FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, "g_ViewMatrix",D3DTS::VIEW)))
      return E_FAIL;
    if (FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, "g_ProjMatrix",D3DTS::PROJ)))
      return E_FAIL;
    if (FAILED(m_Texture->Bind_ShaderResourceView(m_Shader, "g_DiffuseTexture", 0)))
      return E_FAIL;
    if (FAILED(GAME_INSTANCE->Bind_CameraPosition(m_Shader, "g_CameraPosition")))
      return E_FAIL;

    const LIGHT_DESC* lightDesc = GAME_INSTANCE->Get_LightDesc(0);
    if (nullptr == lightDesc)
    return E_FAIL;
    
    if (FAILED(m_Shader->Bind_RawValue("g_LightDir", &lightDesc->direction,
    sizeof(Float4)))) 	return E_FAIL; 
	if (FAILED(m_Shader->Bind_RawValue("g_LightDiffuse", &lightDesc->diffuse,
    sizeof(Float4)))) 	return E_FAIL; 
	if (FAILED(m_Shader->Bind_RawValue("g_LightAmbient", &lightDesc->ambient,
    sizeof(Float4)))) 	return E_FAIL; 
	if (FAILED(m_Shader->Bind_RawValue("g_LightSpecular", &lightDesc->specular,
    sizeof(Float4)))) 	return E_FAIL;

    return S_OK;
}

Shared<Terrain> Terrain::Create(const ComPtr<ID3D11Device> &device,
                                const ComPtr<ID3D11DeviceContext> &context) {
  auto terrain = make_shared<Terrain>(device, context);

  if (FAILED(terrain->Initialize_Prototype())) {
    LOG_ERROR(L"Failed To initialize Terrain Prototype");
    return nullptr;
  }
  return terrain;
}

Shared<GameObject> Terrain::Clone(void *arg) {
  auto terrain = make_shared<Terrain>(*this);

  if (FAILED(terrain->Initialize(arg))) {
    LOG_ERROR(L"Failed To CreateComponent Terrain Prototype");
    return nullptr;
  }

  return terrain;
}
