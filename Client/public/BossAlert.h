#pragma once
#include "UIPanel.h"

NS_BEGIN(Engine)
class UIImage;
class TextUI;
NS_END

NS_BEGIN(Client)

class HpBarWorldUI;
class Monster;

/* The big bosses' WARNING (Em3000 when its spots come on, Em8000 when its entrance ends), laid out and timed as the
   reference plays it (0:36~0:38) but in the user's orange Alert set
   (Texture/UI/Alert): a full-width band of Alert_Block_Small tiles (band, framing lines and dots) with "- WARNING -", a
   line with a dot at each end and the boss's name under it, all in Alien_Alert's dark text colour. It holds for a
   second, folds into a bright ivory streak, rises with the line and the name to where the boss's HP bar floats and
   narrows into that bar, which shows from then on. Only the bar's name differs between bosses. One per entrance, made
   at runtime and gone when it lands; it plays core_warning as it opens. */
class CLIENT_DLL BossAlert final : public UIPanel
{
public:
	typedef struct tagBossAlertDesc : public UI_DESC
	{
		Shared<Monster> boss{ nullptr };
	} BOSS_ALERT_DESC;

public:
	// Hides the boss's bar until the band lands in it.
	static void Play(const Shared<Monster>& boss);

public:
	BossAlert() = default;
	BossAlert(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	BossAlert(const BossAlert& rhs);
	HRESULT Initialize(void* arg) override;
	void Late_Update(Float timeDelta) override;

private:
	HRESULT Ready_Layout(const wstring& name);
	void Land();

private:
	Weak<Monster> m_Boss{};
	Weak<HpBarWorldUI> m_HpBar{};
	Float m_Time{ 0.f };
	Vector2 m_RiseFrom{};		// the streak's spot when it starts to rise

	vector<Weak<UIImage>> m_BandTiles{};
	Weak<UIImage> m_Streak{};	// what the band folds into; becomes the bar's fill
	Weak<TextUI> m_Heading{};
	Weak<UIImage> m_Line{};
	Weak<UIImage> m_LineDotL{};
	Weak<UIImage> m_LineDotR{};
	Weak<TextUI> m_Name{};

public:
	static Shared<BossAlert> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg = nullptr) override;
};

NS_END
