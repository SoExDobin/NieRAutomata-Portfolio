#include "pch.h"
#include "Pl0000HpBar.h"
#include "Entity.h"
#include <Game.h>
#include <SpdLogger.h>
#include <TextUI.h>
#include <UIImage.h>
#include <VIBuffer_Rect.h>

namespace
{
	// The reference's corner HP (1920x1080): the fill spans x 92~412, y 60~69. The rest is measured round the fill's
	// middle, y down.
	const Vector2 PlayerFillCenter{ 252.f, 64.5f };
	constexpr Float PlayerFillWidth = 320.f;
	constexpr Float PlayerFillHeight = 10.f;
	constexpr Float PlayerSquareY = -14.f;		// the small squares over the fill's ends
	constexpr Float PlayerSquareInset = 3.f;
	constexpr Float PlayerSquareSize = 4.f;
	constexpr Float PlayerFaintLineY = 11.f;
	constexpr Float PlayerLineY = 17.f;			// the bright line, with the ring and the dot on it
	constexpr Float PlayerRingX = -174.f;
	constexpr Float PlayerRingSize = 14.f;
	constexpr Float PlayerDotX = 170.f;
	constexpr Float PlayerDotSize = 7.f;
	constexpr Float PlayerLevelY = 39.5f;
	constexpr Float PlayerLevelLabelX = -157.f;	// "Lv :", then the number a size up
	constexpr Float PlayerLevelNumberX = -112.f;
	// The pieces are 1x1 ivory / black (and an ivory ring and dot) drawn untinted but for their alpha.
	const Color PlayerOpaque{ 1.f, 1.f, 1.f, 1.f };
	const Color PlayerFaint{ 1.f, 1.f, 1.f, 0.5f };
	const Color PlayerBacking{ 1.f, 1.f, 1.f, 0.7f };
	const Color PlayerTextIvory{ 0.92f, 0.90f, 0.86f, 1.f };
}

Pl0000HpBar::Pl0000HpBar() : UIObject{} {}
Pl0000HpBar::Pl0000HpBar(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: UIObject{device, context} {}
Pl0000HpBar::Pl0000HpBar(const Pl0000HpBar& rhs)
	: UIObject{rhs}, m_Level{rhs.m_Level} {}

HRESULT Pl0000HpBar::Initialize_Prototype()
{
	return UIObject::Initialize_Prototype();
}

HRESULT Pl0000HpBar::Initialize(void* arg)
{
	if (nullptr == arg)
	{
		LOG_ERROR(L"Failed to Initialize Pl0000HpBar : nullptr arg");
		return E_FAIL;
	}

	PL0000_HP_BAR_DESC& desc = *static_cast<PL0000_HP_BAR_DESC*>(arg);
	if (FAILED(UIObject::Initialize(&desc)))
	{
		LOG_ERROR(L"Failed to Initialize Pl0000HpBar");
		return E_FAIL;
	}

	m_Target = desc.target;

	if (FAILED(Ready_Components()))
	{
		LOG_ERROR(L"Failed to Ready_Components Pl0000HpBar");
		return E_FAIL;
	}

	return S_OK;
}

void Pl0000HpBar::Priority_Update(Float timeDelta)
{
	if (m_Target.expired() || m_Target.lock()->Is_Destroy())
		GAME_INSTANCE->Destroy(Get_ObjectGuid());
}

void Pl0000HpBar::Update(Float timeDelta)
{
	Update_UITransform();
	UIObject::Update(timeDelta);
}

void Pl0000HpBar::Late_Update(Float timeDelta)
{
	UIObject::Late_Update(timeDelta);

	// Updates only run in play, so the pieces never reach a saved scene.
	if (m_LayoutFailed || (m_Layout.expired() && FAILED(Ready_Layout())))
		return;
	const Shared<Entity> target = m_Target.lock();
	if (!target)
		return;

	const Float ratio = target->Get_MaxHP() > 0.f ? std::clamp(target->Get_HP() / target->Get_MaxHP(), 0.f, 1.f) : 0.f;
	const Shared<Transform> fill = m_Fill.lock()->Get_Transform();
	fill->Set_LocalPosition(PlayerFillWidth * (ratio - 1.f) * 0.5f, 0.f, 0.f);
	fill->Set_LocalScale(PlayerFillWidth * ratio, PlayerFillHeight, 1.f);

	m_LevelLabel.lock()->Set_Text(m_Level > 0 ? L"Lv :" : L"");
	m_LevelText.lock()->Set_Text(m_Level > 0 ? to_wstring(m_Level) : L"");
}

HRESULT Pl0000HpBar::Ready_Layout()
{
	// The corner HUD stands where the reference has it, whatever rect the scene saved for the old bar.
	Set_AnchorState(UI_ANCHOR::TOP_LEFT);
	m_Transform->Set_LocalPosition(PlayerFillCenter.x, PlayerFillCenter.y, 0.f);
	m_Transform->Set_LocalScale(PlayerFillWidth, PlayerFillHeight, 1.f);

	const uint32 level = Get_LevelIndex();
	UIObject::UI_DESC layoutDesc{};
	layoutDesc.sizeX = PlayerFillWidth;
	layoutDesc.sizeY = PlayerFillHeight;
	if (FAILED(Add_Child(level, L"UIPanel", L"HpBarLayout", &layoutDesc)))
	{
		m_LayoutFailed = true;
		return E_FAIL;
	}
	const Shared<UIPanel> layout = static_pointer_cast<UIPanel>(Find_Child(L"HpBarLayout"));

	const auto addImage = [&](const tChar* key, const tChar* texture, const Color& color, Float x, Float y, Float sizeX,
		Float sizeY) -> Shared<UIImage> {
		UIImage::UI_IMAGE_DESC desc{};
		desc.x = x;
		desc.y = y;
		desc.sizeX = sizeX;
		desc.sizeY = sizeY;
		desc.textureTag = texture;
		desc.color = color;
		if (FAILED(layout->Add_Child(level, L"UIImage", key, &desc)))
			return nullptr;
		return static_pointer_cast<UIImage>(layout->Find_Child(key));
	};
	const auto addText = [&](const tChar* key, Float x, Float scale) -> Shared<TextUI> {
		TextUI::UI_TEXT_DESC desc{};
		desc.x = x;
		desc.y = PlayerLevelY;
		desc.fontTag = L"Nier_32";
		desc.color = PlayerTextIvory;
		desc.fontScale = scale;
		if (FAILED(layout->Add_Child(level, L"TextUI", key, &desc)))
			return nullptr;
		return static_pointer_cast<TextUI>(layout->Find_Child(key));
	};

	const Float squareX = PlayerFillWidth * 0.5f - PlayerSquareInset;
	const Bool frame =
		addImage(L"SquareL", L"UI_HUD_Ivory", PlayerOpaque, -squareX, PlayerSquareY, PlayerSquareSize, PlayerSquareSize) &&
		addImage(L"SquareR", L"UI_HUD_Ivory", PlayerOpaque, squareX, PlayerSquareY, PlayerSquareSize, PlayerSquareSize) &&
		addImage(L"Backing", L"UI_HUD_Black", PlayerBacking, 0.f, 0.f, PlayerFillWidth, PlayerFillHeight) &&
		addImage(L"FaintLine", L"UI_HUD_Ivory", PlayerFaint, 0.f, PlayerFaintLineY, PlayerFillWidth, 1.5f) &&
		addImage(L"Line", L"UI_HUD_Ivory", PlayerOpaque, 0.f, PlayerLineY, PlayerFillWidth, 2.f) &&
		addImage(L"Ring", L"UI_HUD_Ring", PlayerOpaque, PlayerRingX, PlayerLineY, PlayerRingSize, PlayerRingSize) &&
		addImage(L"Dot", L"UI_HUD_Dot", PlayerOpaque, PlayerDotX, PlayerLineY, PlayerDotSize, PlayerDotSize);
	const Shared<UIImage> fill = frame
		? addImage(L"Fill", L"UI_HUD_Ivory", PlayerOpaque, 0.f, 0.f, PlayerFillWidth, PlayerFillHeight) : nullptr;
	const Shared<TextUI> label = fill ? addText(L"LevelLabel", PlayerLevelLabelX, 0.4f) : nullptr;
	const Shared<TextUI> number = label ? addText(L"Level", PlayerLevelNumberX, 0.5f) : nullptr;
	if (!number)
	{
		LOG_ERROR(L"Failed to build the player HP bar's pieces");
		m_LayoutFailed = true;
		GAME_INSTANCE->Destroy(layout->Get_ObjectGuid());
		return E_FAIL;
	}
	m_Layout = layout;
	m_Fill = fill;
	m_LevelLabel = label;
	m_LevelText = number;
	return S_OK;
}

HRESULT Pl0000HpBar::Ready_Components()
{
	// No longer drawn with (the pieces are children); kept because the scene documents list these components.
	auto shaderDesc = Shader::SHADER_DESC{ VTXTEX::Tag, VTXTEX::Elements, VTXTEX::numElements };
	m_Shader = Add_Component<Shader>(ETOI(LEVEL::STATIC), &shaderDesc);
	if (nullptr == m_Shader)
		return E_FAIL;

	auto textureDesc = Texture::TEXTURE_DESC{ Get_LevelIndex(), L"UI_Hp_Bar" };
	m_Texture = Add_Component<Texture>(Get_LevelIndex(), &textureDesc);
	if (nullptr == m_Texture)
		return E_FAIL;

	m_BufferRect = Add_Component<VIBuffer_Rect>();
	if (nullptr == m_BufferRect)
		return E_FAIL;

	return S_OK;
}

Shared<Pl0000HpBar> Pl0000HpBar::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<Pl0000HpBar>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Pl0000HpBar");
		return nullptr;
	}

	return prototype;
}

Shared<GameObject> Pl0000HpBar::Clone(void* arg)
{
	auto instance = make_shared<Pl0000HpBar>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : Pl0000HpBar");
		return nullptr;
	}

	return instance;
}
