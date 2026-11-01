#include "pch.h"
#include "LoadingPixelPanel.h"

#include <Game.h>
#include "Shader.h"
#include "Texture.h"
#include "VIBuffer_Rect.h"
#include "SpdLogger.h"

LoadingPixelPanel::LoadingPixelPanel() : UIObject() {}

LoadingPixelPanel::LoadingPixelPanel(const ComPtr<ID3D11Device>& device,
    const ComPtr<ID3D11DeviceContext>& context)
    : UIObject(device, context) {
}

LoadingPixelPanel::LoadingPixelPanel(const LoadingPixelPanel& rhs) : UIObject(rhs) {}

HRESULT LoadingPixelPanel::Initialize_Prototype()
{
	return UIObject::Initialize_Prototype();
}

HRESULT LoadingPixelPanel::Initialize(void* arg)
{
    UI_DESC desc{};
    desc.anchor = UI_ANCHOR::CENTER;
    desc.x = 0.f;
    desc.y = 0.f;
    desc.sizeX = 2240.f;
    desc.sizeY = 1080.f;

    if (FAILED(UIObject::Initialize(&desc)))
        return E_FAIL;

    if (FAILED(Ready_Components()))
        return E_FAIL;

    return S_OK;
}

void LoadingPixelPanel::On_Destroy() { UIObject::On_Destroy(); }

void LoadingPixelPanel::Priority_Update(Float timeDelta) {
    UIObject::Priority_Update(timeDelta);
}

void LoadingPixelPanel::Update(Float timeDelta) {
    Update_UITransform();
    UIObject::Update(timeDelta);
}

void LoadingPixelPanel::Late_Update(Float timeDelta) {
    UIObject::Late_Update(timeDelta);
}

void LoadingPixelPanel::Fixed_Update(Float fixedDelta) {
    UIObject::Fixed_Update(fixedDelta);
}

HRESULT LoadingPixelPanel::Render() {
    if (FAILED(m_Transform->Bind_ShaderResource(m_Shader, "g_WorldMatrix")))
        return E_FAIL;
    if (FAILED(Bind_ShaderResource(m_Shader, "g_ViewMatrix", D3DTS::VIEW)))
        return E_FAIL;
    if (FAILED(Bind_ShaderResource(m_Shader, "g_ProjMatrix", D3DTS::PROJ)))
        return E_FAIL;
    if (FAILED(m_Texture->Bind_ShaderResourceView(m_Shader, "g_Texture", 0)))
        return E_FAIL;
    if (FAILED(m_Shader->Bind_RawValue("g_RGBA", m_Texture->Get_RGBA_Absolute(), sizeof(Color))))
        return E_FAIL;

    if (FAILED(m_Shader->Begin(0)))
        return E_FAIL;
    if (FAILED(m_BufferRect->Bind_Resources()))
        return E_FAIL;
    if (FAILED(m_BufferRect->Render()))
        return E_FAIL;

    return S_OK;;
}

void LoadingPixelPanel::Submit_RenderGroup()
{
    GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::UI, shared_from_this());
}

HRESULT LoadingPixelPanel::Ready_Components()
{
    auto shaderDesc = Shader::SHADER_DESC{ VTXTEX::Tag, VTXTEX::Elements, VTXTEX::numElements };
    m_Shader = Add_Component<Shader>(ETOI(LEVEL::STATIC), &shaderDesc);
    if (nullptr == m_Shader)
        return E_FAIL;

    auto textureDesc = Texture::TEXTURE_DESC{ ETOI(LEVEL::LOADING), L"UI_Loading_Pixel" };
    m_Texture = Add_Component<Texture>(ETOI(LEVEL::STATIC), &textureDesc);
    if (nullptr == m_Texture)
        return E_FAIL;

    m_Texture->Set_RGBA(Color{ 1.f, 1.f, 1.f, 0.2f });

    m_BufferRect = Add_Component<VIBuffer_Rect>(ETOI(LEVEL::STATIC));
    if (nullptr == m_BufferRect)
        return E_FAIL;

    return S_OK;
}

Shared<LoadingPixelPanel> LoadingPixelPanel::Create(const ComPtr<ID3D11Device>& device,
    const ComPtr<ID3D11DeviceContext>& context) {
    auto logo = make_shared<LoadingPixelPanel>(device, context);

    if (FAILED(logo->Initialize_Prototype())) {
        LOG_ERROR(L"Failed To Create LoadingLogo");
        return nullptr;
    }

    return logo;
}

Shared<GameObject> LoadingPixelPanel::Clone(void* arg) {
    auto logo = make_shared<LoadingPixelPanel>(*this);

    if (FAILED(logo->Initialize(arg))) {
        LOG_ERROR(L"Failed To CreateComponent LoadingPixelPanel");
        return nullptr;
    }

    return logo;
}