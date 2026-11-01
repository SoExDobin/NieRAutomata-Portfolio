#include "pch.h"
#include "HpBarWorldUI.h"
#include "Entity.h"
#include "ThirdPersonCamera.h"
#include <Game.h>
#include <SpdLogger.h>
#include <TextUI.h>
#include <UIImage.h>
#include <VIBuffer_Rect.h>

namespace
{
	constexpr Float BarSquareY = 17.f;			// the small squares under the fill's ends
	constexpr Float BarSquareInset = 1.5f;
	constexpr Float BarSquareSize = 4.f;
	constexpr Float BarLevelY = 22.f;
	// The pieces are 1x1 ivory / black (and ivory dots) drawn untinted; the backing only takes an alpha.
	const Color BarOpaque{ 1.f, 1.f, 1.f, 1.f };
	const Color BarBacking{ 1.f, 1.f, 1.f, 0.6f };
	const Color BarTextIvory{ 0.92f, 0.90f, 0.86f, 1.f };
}

HpBarWorldUI::HpBarWorldUI() : WorldUIObject{} {}
HpBarWorldUI::HpBarWorldUI(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: WorldUIObject{device, context} {}
HpBarWorldUI::HpBarWorldUI(const HpBarWorldUI& rhs)
	: WorldUIObject{rhs}, m_DisplayName{rhs.m_DisplayName}, m_Level{rhs.m_Level}, m_BarSize{rhs.m_BarSize} {}

HRESULT HpBarWorldUI::Initialize_Prototype()
{
	return WorldUIObject::Initialize_Prototype();
}

HRESULT HpBarWorldUI::Initialize(void* arg)
{
	if (FAILED(WorldUIObject::Initialize(arg)))
	{
		LOG_ERROR(L"Failed to Initialize HPBarWorldUI");
		return E_FAIL;
	}

	Set_AnchorState(UI_ANCHOR::TOP_LEFT);

	if (arg)
	{
		HP_BAR_WORLD_UI_DESC& desc = *static_cast<HP_BAR_WORLD_UI_DESC*>(arg);
		Set_TargetObjectGuid(desc.target ? desc.target->Get_ObjectGuid() : ObjectGuid{});
		m_WorldOffset = desc.worldOffset;
		if (desc.displayName)
			m_DisplayName = desc.displayName;
		if (desc.sizeX > 0.f && desc.sizeY > 0.f)
			m_BarSize = Vector2{ desc.sizeX, desc.sizeY };
	}

	if (FAILED(Ready_Components()))
	{
		LOG_ERROR(L"Failed to Ready_Components HPBarWorldUI");
		return E_FAIL;
	}

	return S_OK;
}



void HpBarWorldUI::Priority_Update(Float timeDelta)
{

}

void HpBarWorldUI::Update(Float timeDelta)
{
	UIObject::Update(timeDelta);
}

void HpBarWorldUI::Late_Update(Float timeDelta)
{
	// Updates only run in play, so the pieces never reach a saved scene.
	if (m_LayoutFailed || (m_Layout.expired() && FAILED(Ready_Layout())))
		return;

	// Nothing drawn while the target is behind the camera or past HideDistance, or while a cinematic (camera event,
	// boss cutscene) holds the camera.
	const Shared<UIPanel> layout = m_Layout.lock();
	const auto camera = Cast<ThirdPersonCamera>(GAME_INSTANCE->Get_MainCamera());
	Vector2 position{};
	const Float alpha = !(camera && camera->Is_Held()) && Get_CanvasPosition(position) ? Get_DistanceAlpha() : 0.f;
	layout->Set_Active(alpha > 0.f);
	if (alpha <= 0.f)
		return;
	m_Transform->Set_LocalPosition(position.x, position.y, 0.f);

	if (alpha != m_Alpha)
	{
		m_Alpha = alpha;
		for (const auto& [image, color] : m_Images)
			image.lock()->Get_Component<Texture>()->Set_RGBA(Color{ color.x, color.y, color.z, color.w * alpha });
		for (const auto& [text, color] : m_Texts)
			text.lock()->Set_Color(Color{ color.x, color.y, color.z, color.w * alpha });
	}

	const Shared<Entity> target = m_Target.lock();
	const Float width = Get_SizeX();
	const Float ratio = target->Get_MaxHP() > 0.f ? std::clamp(target->Get_HP() / target->Get_MaxHP(), 0.f, 1.f) : 0.f;
	const Shared<Transform> fill = m_Fill.lock()->Get_Transform();
	fill->Set_LocalPosition(width * (ratio - 1.f) * 0.5f, 0.f, 0.f);
	fill->Set_LocalScale(width * ratio, Get_SizeY(), 1.f);

	const Shared<TextUI> name = m_NameText.lock();
	name->Set_Text(m_DisplayName);
	const Shared<TextUI> level = m_LevelText.lock();
	level->Set_Text(m_Level > 0 ? L"Lv : " + to_wstring(m_Level) : L"");
}

Bool HpBarWorldUI::Get_CanvasPosition(Vector2& outPosition)
{
	const Shared<Entity> target = Resolve_Target();
	if (!target)
		return false;

	return To_Canvas(target->Get_Transform()->Get_Position() + m_WorldOffset, outPosition);
}

Float HpBarWorldUI::Get_DistanceAlpha()
{
	const Shared<Entity> target = Resolve_Target();
	if (!target)
		return 0.f;

	const Vector3 viewPosition = XMVector3TransformCoord(target->Get_Transform()->Get_Position() + m_WorldOffset,
		GAME_INSTANCE->Get_Transform(D3DTS::VIEW));
	return std::clamp((HideDistance - viewPosition.Length()) / (HideDistance - FadeStartDistance), 0.f, 1.f);
}

HRESULT HpBarWorldUI::Ready_Layout()
{
	// The bar stands at the owner's size, whatever rect the scene saved for the old bar (some saved 1x1).
	m_Transform->Set_LocalScale(m_BarSize.x, m_BarSize.y, 1.f);

	const uint32 level = Get_LevelIndex();
	const Float width = Get_SizeX(), height = Get_SizeY();

	UIObject::UI_DESC layoutDesc{};
	layoutDesc.sizeX = width;
	layoutDesc.sizeY = height;
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
		const Shared<UIImage> image = static_pointer_cast<UIImage>(layout->Find_Child(key));
		m_Images.emplace_back(image, color);
		return image;
	};
	const auto addText = [&](const tChar* key, const tChar* font, Float y, Float scale, Float spacing) -> Shared<TextUI> {
		TextUI::UI_TEXT_DESC desc{};
		desc.y = y;
		desc.fontTag = font;
		desc.color = BarTextIvory;
		desc.fontScale = scale;
		desc.spacing = spacing;
		desc.pivot = 0.5f;
		if (FAILED(layout->Add_Child(level, L"TextUI", key, &desc)))
			return nullptr;
		const Shared<TextUI> text = static_pointer_cast<TextUI>(layout->Find_Child(key));
		m_Texts.emplace_back(text, BarTextIvory);
		return text;
	};

	const Float dotX = width * 0.5f + LineDotGap;
	const Float squareX = width * 0.5f - BarSquareInset;
	const Bool frame =
		addImage(L"Backing", L"UI_HUD_Black", BarBacking, 0.f, 0.f, width, height) &&
		addImage(L"Line", L"UI_HUD_Ivory", BarOpaque, 0.f, LineY, width + LineOverhang * 2.f, LineHeight) &&
		addImage(L"LineDotL", L"UI_HUD_Dot", BarOpaque, -dotX, LineY, LineDotSize, LineDotSize) &&
		addImage(L"LineDotR", L"UI_HUD_Dot", BarOpaque, dotX, LineY, LineDotSize, LineDotSize) &&
		addImage(L"SquareL", L"UI_HUD_Ivory", BarOpaque, -squareX, BarSquareY, BarSquareSize, BarSquareSize) &&
		addImage(L"SquareR", L"UI_HUD_Ivory", BarOpaque, squareX, BarSquareY, BarSquareSize, BarSquareSize);
	const Shared<UIImage> fill = frame ? addImage(L"Fill", L"UI_HUD_Ivory", BarOpaque, 0.f, 0.f, width, height) : nullptr;
	const Shared<TextUI> name = fill ? addText(L"Name", L"Nier_KR_32", NameY, NameScale, 3.f) : nullptr;
	const Shared<TextUI> levelText = name ? addText(L"Level", L"Nier_32", BarLevelY, 0.45f, 0.f) : nullptr;
	if (!levelText)
	{
		LOG_ERROR(L"Failed to build the HP bar's pieces on {}", Get_Name());
		m_LayoutFailed = true;
		m_Images.clear();
		m_Texts.clear();
		GAME_INSTANCE->Destroy(layout->Get_ObjectGuid());
		return E_FAIL;
	}
	m_Layout = layout;
	m_Fill = fill;
	m_NameText = name;
	m_LevelText = levelText;
	return S_OK;
}

HRESULT HpBarWorldUI::Set_TargetObjectGuid(ObjectGuid targetGuid)
{
	m_TargetGuid = targetGuid;
	m_Target.reset();
	Resolve_Target();
	return S_OK;
}

Shared<Entity> HpBarWorldUI::Resolve_Target()
{
	if (!m_TargetGuid.Is_Valid())
	{
		m_Target.reset();
		return nullptr;
	}
	if (const Shared<Entity> target = m_Target.lock();
		target && !target->Is_Destroy() && target->Get_ObjectGuid() == m_TargetGuid)
		return target;
	const Shared<Entity> target = GAME_INSTANCE->Find<Entity>(m_TargetGuid);
	if (!target || target->Is_Destroy())
	{
		m_Target.reset();
		return nullptr;
	}
	m_Target = target;
	return target;
}

HRESULT HpBarWorldUI::Ready_Components()
{
	// No longer drawn with (the pieces are children); kept because the scene documents list these components.
	auto shaderDesc = Shader::SHADER_DESC{ VTXTEX::Tag, VTXTEX::Elements, VTXTEX::numElements };
	m_Shader = Add_Component<Shader>(ETOI(LEVEL::STATIC), &shaderDesc);
	if (nullptr == m_Shader)
		return E_FAIL;

	auto textureDesc = Texture::TEXTURE_DESC{ Get_LevelIndex(), L"UI_Hp_Bar" };
	m_Texture = Add_Component<Texture>(ETOI(LEVEL::GAMEPLAY), &textureDesc);
	if (nullptr == m_Texture)
		return E_FAIL;

	m_BufferRect = Add_Component<VIBuffer_Rect>();
	if (nullptr == m_BufferRect)
		return E_FAIL;

	return S_OK;

}

Shared<HpBarWorldUI> HpBarWorldUI::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<HpBarWorldUI>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : HpBarWorldUI");
		return nullptr;
	}

	return prototype;
}

Shared<GameObject> HpBarWorldUI::Clone(void* arg)
{
	auto instance = make_shared<HpBarWorldUI>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : HpBarWorldUI");
		return nullptr;
	}

	return instance;
}
