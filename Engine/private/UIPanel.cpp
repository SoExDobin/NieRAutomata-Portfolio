#include "UIPanel.h"
#include "Game.h"

UIPanel::UIPanel(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
    : UIObject(device, context) {}
UIPanel::UIPanel(const UIPanel& rhs) : UIObject(rhs) {}

HRESULT UIPanel::Initialize(void* arg)
{
    UI_DESC defaults{};
    defaults.sizeX = 1920.f;
    defaults.sizeY = 1080.f;
    return UIObject::Initialize(arg ? arg : &defaults);
}

Shared<UIPanel> UIPanel::CreatePrototype()
{
    auto result = make_shared<UIPanel>(GAME_INSTANCE->Get_Device(), GAME_INSTANCE->Get_Context());
    return SUCCEEDED(result->Initialize_Prototype()) ? result : nullptr;
}

void UIPanel::Submit_RenderGroup()
{
    GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::UI, shared_from_this());
}

Shared<GameObject> UIPanel::Clone(void* arg)
{
    auto result = make_shared<UIPanel>(*this);
    return SUCCEEDED(result->Initialize(arg)) ? result : nullptr;
}
