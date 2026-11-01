#include "pch.h"
#include "TitleBackground.h"

#include <Game.h>
#include <SpdLogger.h>
#include <VIBuffer_Rect.h>

#include "LoadingPixelPanel.h"

TitleBackground::TitleBackground() : UIObject() {}

TitleBackground::TitleBackground(const ComPtr<ID3D11Device>& device,
    const ComPtr<ID3D11DeviceContext>& context)
    : UIObject(device, context) {
}

TitleBackground::TitleBackground(const TitleBackground& rhs) : UIObject(rhs) {}

HRESULT TitleBackground::Initialize_Prototype()
{
    return UIObject::Initialize_Prototype();
}

HRESULT TitleBackground::Initialize(void* arg)
{
    m_IntroTime = 0.f;
    UI_DESC desc{};
    desc.anchor = UI_ANCHOR::CENTER;
    desc.x = 0.f;
    desc.y = 0.f;
    desc.sizeX = 1920.f;
    desc.sizeY = 1920.f;

    if (FAILED(UIObject::Initialize(&desc)))
        return E_FAIL;

    if (FAILED(Ready_Components()))
        return E_FAIL;

    return S_OK;
}

void TitleBackground::On_Destroy() { UIObject::On_Destroy(); }

void TitleBackground::Priority_Update(Float timeDelta) {
    UIObject::Priority_Update(timeDelta);
}

void TitleBackground::Update(Float timeDelta) {
    m_IntroTime = (std::min)(m_IntroTime + (std::max)(timeDelta, 0.f), 3.f);
    Update_UITransform();
    UIObject::Update(timeDelta);
}

void TitleBackground::Late_Update(Float timeDelta) {
    UIObject::Late_Update(timeDelta);
}

void TitleBackground::Fixed_Update(Float fixedDelta) {
    UIObject::Fixed_Update(fixedDelta);
}

HRESULT TitleBackground::Render() {
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

    // A local effect keeps Title's UI background independent of world post-processing.
    const Float remaining = 1.f - m_IntroTime / 3.f;
    const Float2 intro{ m_IntroTime, remaining * remaining };
    if (FAILED(m_Shader->Bind_RawValue("g_TitleIntro", &intro, sizeof(intro))))
        return E_FAIL;
    if (FAILED(m_Shader->Begin("TitleIntro")))
        return E_FAIL;
    if (FAILED(m_BufferRect->Bind_Resources()))
        return E_FAIL;
    if (FAILED(m_BufferRect->Render()))
        return E_FAIL;

    return S_OK;;
}

void TitleBackground::Submit_RenderGroup()
{
    GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::UI, shared_from_this());
}

HRESULT TitleBackground::Ready_Components()
{
    auto shaderDesc = Shader::SHADER_DESC{ VTXTEX::Tag, VTXTEX::Elements, VTXTEX::numElements };
    m_Shader = Add_Component<Shader>(ETOI(LEVEL::STATIC), &shaderDesc);
    if (nullptr == m_Shader)
        return E_FAIL;

    auto textureDesc = Texture::TEXTURE_DESC{ ETOI(LEVEL::TITLE), L"UI_Title_BackGround" };
    m_Texture = Add_Component<Texture>(ETOI(LEVEL::STATIC), &textureDesc);
    if (nullptr == m_Texture)
        return E_FAIL;

    m_BufferRect = Add_Component<VIBuffer_Rect>(ETOI(LEVEL::STATIC));
    if (nullptr == m_BufferRect)
        return E_FAIL;

    return S_OK;
}

Shared<TitleBackground> TitleBackground::Create(const ComPtr<ID3D11Device>& device,
    const ComPtr<ID3D11DeviceContext>& context) {
    auto instance = make_shared<TitleBackground>(device, context);

    if (FAILED(instance->Initialize_Prototype())) {
        LOG_ERROR(L"Failed To Create TitleBackground");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> TitleBackground::Clone(void* arg) {
    auto instance = make_shared<TitleBackground>(*this);

    if (FAILED(instance->Initialize(arg))) {
        LOG_ERROR(L"Failed To CreateComponent TitleBackground");
        return nullptr;
    }

    return instance;
}
