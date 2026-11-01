#include "pch.h"
#include "BossAlert.h"

#include <Game.h>
#include <SpdLogger.h>
#include <TextUI.h>
#include <UIImage.h>

#include "HpBarWorldUI.h"
#include "Monster.h"

namespace
{
	// The reference (1080p capture) in seconds from the band's
	// first frame and pixels on the 1920x1080 canvas.
	constexpr Float AlertOpenTime = 0.12f;		// the heading spreads out from tight, flickering
	constexpr Float AlertFoldStart = 1.05f;		// the band folds into a bright streak
	constexpr Float AlertFoldTime = 0.13f;
	constexpr Float AlertRiseTime = 0.47f;		// the streak, line and name rise onto the bar
	constexpr Float AlertNarrowTime = 0.2f;		// and narrow into it
	constexpr Float AlertBandY = 539.5f;
	constexpr Float AlertBandHeight = 180.f;
	// Alert_Block_Small (128x1024) has its band at y 432~592 and its framing lines 16 px off it, so at 9/8 its band is
	// the reference's 180 px and the lines land 18 px off as there.
	constexpr Float AlertTileWidth = 144.f;
	constexpr Float AlertTileHeight = 1152.f;
	constexpr uint32 AlertTileCount = 14;		// 2016 px, a little past both edges
	constexpr Float AlertHeadingY = 510.f;
	constexpr Float AlertHeadingScale = 0.82f;
	constexpr Float AlertHeadingSpacing = 27.f;
	constexpr Float AlertLineY = 558.f;
	constexpr Float AlertLineWidth = 1104.f;
	constexpr Float AlertLineDotX = 563.f;		// from the middle
	constexpr Float AlertLineDotSize = 7.f;
	constexpr Float AlertNameY = 585.f;
	constexpr Float AlertNameScale = 0.7f;
	constexpr Float AlertNameSpacing = 4.f;
	constexpr Float AlertStreakHeight = 3.f;
	constexpr Float AlertBarWidth = 236.f;		// a bar off screen at landing
	// On the orange band the writing takes Alien_Alert's dark grey; once folded it is the bar's ivory. The line and dots
	// are ivory textures, so they are tinted down to that grey.
	const Color AlertDarkText{ 0.31f, 0.294f, 0.259f, 1.f };
	const Color AlertIvoryText{ 0.92f, 0.90f, 0.86f, 1.f };
	const Color AlertDarkTint{ 0.337f, 0.327f, 0.301f, 1.f };
	const Color AlertOpaque{ 1.f, 1.f, 1.f, 1.f };
	constexpr Float AlertWarningVolume = 0.6f;

	Float Alert_Ease(Float t)
	{
		t = std::clamp(t, 0.f, 1.f);
		return t * t * (3.f - 2.f * t);
	}

	void Place(const Weak<UIImage>& image, Float x, Float y, Float width, Float height)
	{
		const Shared<Transform> transform = image.lock()->Get_Transform();
		transform->Set_LocalPosition(x, y, 0.f);
		transform->Set_LocalScale(width, height, 1.f);
	}

	void Tint(const Weak<UIImage>& image, const Color& color)
	{
		image.lock()->Get_Component<Texture>()->Set_RGBA(color);
	}
}

BossAlert::BossAlert(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: UIPanel(device, context) {}
BossAlert::BossAlert(const BossAlert& rhs) : UIPanel(rhs) {}

void BossAlert::Play(const Shared<Monster>& boss)
{
	if (!boss)
		return;
	BOSS_ALERT_DESC desc{};
	desc.boss = boss;
	desc.sizeX = 1920.f;
	desc.sizeY = 1080.f;
	GAME_INSTANCE->Instantiate<BossAlert>(L"BossAlert", boss->Get_LevelIndex(), &desc);
}

HRESULT BossAlert::Initialize(void* arg)
{
	if (!arg)
		return E_FAIL;
	BOSS_ALERT_DESC& desc = *static_cast<BOSS_ALERT_DESC*>(arg);
	if (!desc.boss || FAILED(UIPanel::Initialize(&desc)))
		return E_FAIL;

	m_Boss = desc.boss;
	m_HpBar = desc.boss->Get_HpBarUI();
	const Shared<HpBarWorldUI> hpBar = m_HpBar.lock();
	if (hpBar)
		hpBar->Set_Active(false);
	if (FAILED(Ready_Layout(hpBar ? hpBar->Get_DisplayName() : L"")))
		return E_FAIL;

	GAME_INSTANCE->PlaySoundFXOnce(L"core_warning", SOUNDCHANNEL::CHANNEL_27, AlertWarningVolume);
	GAME_INSTANCE->Start_ScreenGlitch(0.4f, AlertOpenTime);
	return S_OK;
}

HRESULT BossAlert::Ready_Layout(const wstring& name)
{
	const uint32 level = Get_LevelIndex();
	const auto addImage = [&](const wstring& key, const tChar* texture, const Color& color, Float x, Float y,
		Float sizeX, Float sizeY) -> Shared<UIImage> {
		UIImage::UI_IMAGE_DESC desc{};
		desc.anchor = UI_ANCHOR::TOP_LEFT;
		desc.x = x;
		desc.y = y;
		desc.sizeX = sizeX;
		desc.sizeY = sizeY;
		desc.textureTag = texture;
		desc.color = color;
		if (FAILED(Add_Child(level, L"UIImage", key, &desc)))
			return nullptr;
		return static_pointer_cast<UIImage>(Find_Child(key));
	};
	const auto addText = [&](const tChar* key, const tChar* font, const wstring& text, Float y, Float scale,
		Float spacing) -> Shared<TextUI> {
		TextUI::UI_TEXT_DESC desc{};
		desc.anchor = UI_ANCHOR::TOP_LEFT;
		desc.x = 960.f;
		desc.y = y;
		desc.fontTag = font;
		desc.text = text.c_str();
		desc.color = AlertDarkText;
		desc.fontScale = scale;
		desc.spacing = spacing;
		desc.pivot = 0.5f;
		if (FAILED(Add_Child(level, L"TextUI", key, &desc)))
			return nullptr;
		return static_pointer_cast<TextUI>(Find_Child(key));
	};

	Bool built = true;
	for (uint32 i = 0; i < AlertTileCount; ++i)
	{
		const Float x = 960.f + (static_cast<Float>(i) - (AlertTileCount - 1) * 0.5f) * AlertTileWidth;
		const Shared<UIImage> tile = addImage(L"BandTile" + to_wstring(i), L"UI_Alert_Block_Small", AlertOpaque, x,
			AlertBandY, AlertTileWidth, AlertTileHeight);
		built = built && tile;
		m_BandTiles.push_back(tile);
	}
	m_Streak = addImage(L"Streak", L"UI_HUD_Ivory", Color{ 1.f, 1.f, 1.f, 0.f }, 960.f, AlertBandY, 1920.f,
		AlertBandHeight);
	m_Heading = addText(L"Heading", L"Nier_KR_64", L"-WARNING-", AlertHeadingY, AlertHeadingScale, 0.f);
	m_Line = addImage(L"Line", L"UI_HUD_Ivory", AlertDarkTint, 960.f, AlertLineY, AlertLineWidth,
		HpBarWorldUI::LineHeight);
	m_LineDotL = addImage(L"LineDotL", L"UI_HUD_Dot", AlertDarkTint, 960.f - AlertLineDotX, AlertLineY,
		AlertLineDotSize, AlertLineDotSize);
	m_LineDotR = addImage(L"LineDotR", L"UI_HUD_Dot", AlertDarkTint, 960.f + AlertLineDotX, AlertLineY,
		AlertLineDotSize, AlertLineDotSize);
	m_Name = addText(L"Name", L"Nier_KR_32", name, AlertNameY, AlertNameScale, AlertNameSpacing);
	if (!built || m_Streak.expired() || m_Heading.expired() || m_Line.expired() || m_LineDotL.expired() ||
		m_LineDotR.expired() || m_Name.expired())
	{
		LOG_ERROR(L"Failed to build the boss WARNING's pieces");
		return E_FAIL;
	}
	return S_OK;
}

void BossAlert::Late_Update(Float timeDelta)
{
	UIPanel::Late_Update(timeDelta);
	m_Time += timeDelta;
	const Float t = m_Time;

	// Open: the heading spreads out while it flickers.
	const Shared<TextUI> heading = m_Heading.lock();
	heading->Set_Spacing(AlertHeadingSpacing * Alert_Ease(t / AlertOpenTime));
	Float headingAlpha = t < AlertOpenTime && fmodf(t, 0.04f) >= 0.02f ? 0.3f : 1.f;

	// Fold: the band thins away into a bright streak 16 px under the line (the bar's fill under its line), the name
	// moves up over the line, where the bar has it, and the writing turns the bar's ivory.
	const Float fold = Alert_Ease((t - AlertFoldStart) / AlertFoldTime);
	const Float streakY = AlertLineY - HpBarWorldUI::LineY;
	headingAlpha *= 1.f - fold;
	heading->Set_Color(Color{ AlertDarkText.x, AlertDarkText.y, AlertDarkText.z, headingAlpha });
	for (const Weak<UIImage>& tile : m_BandTiles)
	{
		const Shared<Transform> transform = tile.lock()->Get_Transform();
		transform->Set_LocalScale(AlertTileWidth, AlertTileHeight * (1.f - fold), 1.f);
		Tint(tile, Color{ 1.f, 1.f, 1.f, 1.f - fold });
	}
	Tint(m_Streak, Color{ 1.f, 1.f, 1.f, fold });
	const Color lineTint = Color::Lerp(AlertDarkTint, AlertOpaque, fold);
	Tint(m_Line, lineTint);
	Tint(m_LineDotL, lineTint);
	Tint(m_LineDotR, lineTint);

	// Rise and narrow onto the bar, wherever it is on screen now.
	const Float riseStart = AlertFoldStart + AlertFoldTime;
	const Float rise = Alert_Ease((t - riseStart) / AlertRiseTime);
	const Float narrow = Alert_Ease((t - riseStart - AlertRiseTime) / AlertNarrowTime);
	Vector2 bar{ 960.f, streakY };
	Float barWidth = AlertBarWidth, barHeight = 10.f, ratio = 1.f;
	if (const Shared<HpBarWorldUI> hpBar = m_HpBar.lock())
	{
		Vector2 position{};
		if (hpBar->Get_CanvasPosition(position))
			bar = position;
		barWidth = hpBar->Get_BarSize().x;
		barHeight = hpBar->Get_BarSize().y;
	}
	if (const Shared<Monster> boss = m_Boss.lock(); boss && boss->Get_MaxHP() > 0.f)
		ratio = std::clamp(boss->Get_HP() / boss->Get_MaxHP(), 0.f, 1.f);
	const Vector2 group = Vector2::Lerp(Vector2{ 960.f, streakY }, bar, rise);

	const Float streakTop = AlertBandY + (group.y - AlertBandY) * fold;
	const Float streakHeight = AlertBandHeight + (AlertStreakHeight - AlertBandHeight) * fold;
	const Float fillX = bar.x + barWidth * (ratio - 1.f) * 0.5f;
	Place(m_Streak, group.x + (fillX - group.x) * narrow, streakTop, 1920.f + (barWidth * ratio - 1920.f) * narrow,
		streakHeight + (barHeight - AlertStreakHeight) * narrow);

	const Float lineY = group.y + HpBarWorldUI::LineY;
	const Float lineWidth = AlertLineWidth + (barWidth + HpBarWorldUI::LineOverhang * 2.f - AlertLineWidth) * narrow;
	const Float dotX = AlertLineDotX + (barWidth * 0.5f + HpBarWorldUI::LineDotGap - AlertLineDotX) * narrow;
	const Float dotSize = AlertLineDotSize + (HpBarWorldUI::LineDotSize - AlertLineDotSize) * narrow;
	Place(m_Line, group.x, lineY, lineWidth, HpBarWorldUI::LineHeight);
	Place(m_LineDotL, group.x - dotX, lineY, dotSize, dotSize);
	Place(m_LineDotR, group.x + dotX, lineY, dotSize, dotSize);

	const Shared<TextUI> name = m_Name.lock();
	const Float nameY = AlertNameY + (group.y + HpBarWorldUI::NameY - AlertNameY) * fold;
	name->Get_Transform()->Set_LocalPosition(group.x, nameY, 0.f);
	name->Set_Color(Color::Lerp(AlertDarkText, AlertIvoryText, fold));
	name->Set_FontScale(AlertNameScale + (HpBarWorldUI::NameScale - AlertNameScale) * narrow);

	if (t >= riseStart + AlertRiseTime + AlertNarrowTime)
		Land();
}

void BossAlert::Land()
{
	// Killed while the band played, the boss keeps its bar hidden (OnDeath).
	if (const Shared<Monster> boss = m_Boss.lock(); boss && !boss->Is_Dead())
		if (const Shared<HpBarWorldUI> hpBar = m_HpBar.lock())
			hpBar->Set_Active(true);
	GAME_INSTANCE->Destroy(Get_ObjectGuid());
}

Shared<BossAlert> BossAlert::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto result = make_shared<BossAlert>(device, context);
	return SUCCEEDED(result->Initialize_Prototype()) ? result : nullptr;
}

Shared<GameObject> BossAlert::Clone(void* arg)
{
	auto result = make_shared<BossAlert>(*this);
	return SUCCEEDED(result->Initialize(arg)) ? result : nullptr;
}
