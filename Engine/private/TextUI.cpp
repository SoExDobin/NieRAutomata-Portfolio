#include "TextUI.h"

#include "Game.h"
#include "Renderer.h"

TextUI::TextUI()
{
}

TextUI::TextUI(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: UIObject{device, context} {}

TextUI::TextUI(const TextUI& rhs)
	: UIObject{rhs}, m_fontTag{rhs.m_fontTag}, m_Text{rhs.m_Text}, m_Color{rhs.m_Color},
	  m_FontScale{rhs.m_FontScale}, m_Spacing{rhs.m_Spacing}, m_Pivot{rhs.m_Pivot} {}

HRESULT TextUI::Initialize_Prototype()
{
	return UIObject::Initialize_Prototype();
}

HRESULT TextUI::Initialize(void* arg)
{
	if (FAILED(UIObject::Initialize(arg)))
		return E_FAIL;
	if (arg)
	{
		// The desc's text is copied: callers hand in temporaries.
		const UI_TEXT_DESC& desc = *static_cast<UI_TEXT_DESC*>(arg);
		m_fontTag = desc.fontTag;
		m_Text = desc.text ? desc.text : L"";
		m_Color = desc.color;
		m_FontScale = desc.fontScale;
		m_Spacing = desc.spacing;
		m_Pivot = desc.pivot;
	}
	return S_OK;
}

Float TextUI::Get_CanvasWidth() const
{
	return GAME_INSTANCE->Measure_Font(m_fontTag, m_Text.c_str(), m_Spacing).x * m_FontScale;
}

void TextUI::Priority_Update(Float timeDelta)
{
	UIObject::Priority_Update(timeDelta);
}

void TextUI::Update(Float timeDelta)
{
	UIObject::Update(timeDelta);
}

void TextUI::Late_Update(Float timeDelta)
{
	UIObject::Late_Update(timeDelta);
}

void TextUI::Fixed_Update(Float fixedDelta)
{
	UIObject::Fixed_Update(fixedDelta);
}

HRESULT TextUI::Render()
{
	if (m_fontTag.empty() || m_Text.empty() || m_Color.w <= 0.f) return S_OK;
	if (!Is_Active()) return S_OK;

	// The anchor point on UIImage's 1920x1080 canvas (its world origin is the canvas centre, y up), then on the view.
	const Matrix world = Get_UIWorldMatrix(1920.f, 1080.f);
	D3D11_VIEWPORT viewport{};
	uint32 numViewports = 1;
	m_Context->RSGetViewports(&numViewports, &viewport);
	const Float toView = viewport.Height / 1080.f;
	const Vector2 point{ (world._41 + 960.f) * viewport.Width / 1920.f, (540.f - world._42) * toView };

	const Float scale = m_FontScale * toView;
	const Vector2 size = GAME_INSTANCE->Measure_Font(m_fontTag, m_Text.c_str(), m_Spacing);
	const Vector2 topLeft{ point.x - size.x * scale * m_Pivot, point.y - size.y * scale };
	GAME_INSTANCE->Draw_Font(m_fontTag, m_Text.c_str(), topLeft, m_Color, scale, m_Spacing);

	return S_OK;
}

void TextUI::Submit_RenderGroup()
{
	GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::UI, shared_from_this());
}

Shared<TextUI> TextUI::CreatePrototype()
{
	return Create(GAME_INSTANCE->Get_Device(), GAME_INSTANCE->Get_Context());
}

Shared<TextUI> TextUI::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<TextUI>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : TextUI");
		return nullptr;
	}

	return prototype;
}

Shared<GameObject> TextUI::Clone(void* arg)
{
	auto instance = make_shared<TextUI>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : TextUI");
		return nullptr;
	}

	return instance;
}


