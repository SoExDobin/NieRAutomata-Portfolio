#include "UIImage.h"
#include "Game.h"
#include "Shader.h"
#include "Texture.h"
#include "VIBuffer_Rect.h"

UIImage::UIImage(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
    : UIPanel(device, context) {}
UIImage::UIImage(const UIImage& rhs) : UIPanel(rhs) {}

HRESULT UIImage::Initialize(void* arg)
{
    UI_DESC defaults{};
    defaults.sizeX = 400.f;
    defaults.sizeY = 200.f;
    if (FAILED(UIPanel::Initialize(arg ? arg : &defaults))) return E_FAIL;
    auto shaderDesc = Shader::SHADER_DESC{ VTXTEX::Tag, VTXTEX::Elements, VTXTEX::numElements };
    auto textureDesc = Texture::TEXTURE_DESC{ GAME_INSTANCE->Get_CurrentLevelIndex(), L"" };
    if (!Add_Component<Shader>(0, &shaderDesc) ||
        !Add_Component<Texture>(0, &textureDesc) || !Add_Component<VIBuffer_Rect>(0)) return E_FAIL;
    if (arg)
    {
        const UI_IMAGE_DESC& desc = *static_cast<UI_IMAGE_DESC*>(arg);
        const auto texture = Get_Component<Texture>();
        texture->Set_TextureTag(desc.textureTag);
        texture->Set_RGBA(desc.color);
    }
    return S_OK;
}

HRESULT UIImage::Render()
{
    const auto texture = Get_Component<Texture>();
    const auto shader = Get_Component<Shader>();
    const auto buffer = Get_Component<VIBuffer_Rect>();
    if (!texture || texture->Get_Textures().empty()) return S_OK;
    if (!shader || !buffer) return E_FAIL;
    // Match the editor's reference canvas at every Scene View/window size.
    Update_UITransform(1920.f, 1080.f);
    const Matrix view = Matrix::Identity;
    const Matrix projection = XMMatrixOrthographicLH(1920.f, 1080.f, 0.f, 1.f);
    if (FAILED(m_Transform->Bind_ShaderResource(shader, "g_WorldMatrix")) ||
        FAILED(shader->Bind_Matrix("g_ViewMatrix", &view)) ||
        FAILED(shader->Bind_Matrix("g_ProjMatrix", &projection)) ||
        FAILED(texture->Bind_ShaderResourceView(shader, "g_Texture", 0)) ||
        FAILED(shader->Bind_RawValue("g_RGBA", texture->Get_RGBA_Absolute(), sizeof(Color))) ||
        FAILED(shader->Begin(0)) || FAILED(buffer->Bind_Resources())) return E_FAIL;
    return buffer->Render();
}

void UIImage::Submit_RenderGroup()
{
    GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::UI, shared_from_this());
}

Shared<UIImage> UIImage::CreatePrototype()
{
    auto result = make_shared<UIImage>(GAME_INSTANCE->Get_Device(), GAME_INSTANCE->Get_Context());
    return SUCCEEDED(result->Initialize_Prototype()) ? result : nullptr;
}

Shared<GameObject> UIImage::Clone(void* arg)
{
    auto result = make_shared<UIImage>(*this);
    return SUCCEEDED(result->Initialize(arg)) ? result : nullptr;
}
