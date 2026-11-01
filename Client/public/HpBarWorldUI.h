#pragma once
#include "WorldUIObject.h"

NS_BEGIN(Engine)
class Shader;
class Texture;
class VIBuffer_Rect;
class UIImage;
class UIPanel;
class TextUI;
NS_END

NS_BEGIN(Client)

class Entity;

/* The bar over a monster, as the reference draws it (1080p capture): the name, a thin line with a
   dot past each end, the HP fill over a dark backing, a small square under each end and "Lv : n" between them. Its UI
   rect is the fill (centred on the target's point WorldOffset above it, projected onto the 1920x1080 canvas); the rest
   sits round it. The pieces are UIImage / TextUI children built on the first Late_Update, so they only exist while the
   game runs: the scene documents keep the bar childless (and keep the Shader/Texture/VIBuffer it once drew with). */
class CLIENT_DLL HpBarWorldUI final : public WorldUIObject
{
public:
	// The reference's boss bar (236 px fill) measured round the fill's middle, in canvas pixels, y down; BossAlert lands
	// its line and name on these.
	static constexpr Float LineY = -16.f;			// the thin line over the fill
	static constexpr Float LineHeight = 2.f;
	static constexpr Float LineOverhang = 2.f;		// each side past the fill
	static constexpr Float LineDotGap = 13.5f;		// the line's end dots, past the fill's ends
	static constexpr Float LineDotSize = 6.f;
	static constexpr Float NameY = -33.f;
	static constexpr Float NameScale = 0.64f;
	// Fully drawn up to FadeStartDistance from the camera, fading out to nothing at HideDistance and hidden past it.
	static constexpr Float FadeStartDistance = 20.f;
	static constexpr Float HideDistance = 50.f;

public:
	typedef struct tagHpBarWorldUIDesc : public WORLD_UI_DESC
	{
		Shared<Entity> target{nullptr};
		Vector3 worldOffset{0.f, 0.f, 0.f};
		const tChar* displayName{ nullptr };
	} HP_BAR_WORLD_UI_DESC;

public:
	explicit HpBarWorldUI();
	explicit HpBarWorldUI(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit HpBarWorldUI(const HpBarWorldUI& rhs);
	virtual ~HpBarWorldUI() override = default;

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;

public:
	void Late_Update(Float timeDelta) override;
	void Priority_Update(Float timeDelta) override;
	void Update(Float timeDelta) override;

	ObjectGuid Get_TargetObjectGuid() const { return m_TargetGuid; }
	HRESULT Set_TargetObjectGuid(ObjectGuid targetGuid);
	Vector3 Get_WorldOffset() const { return m_WorldOffset; }
	void Set_WorldOffset(const Vector3& worldOffset) { m_WorldOffset = worldOffset; }
	// Inspector: the name over the bar (none drawn when empty) and the level under it (none at 0).
	wstring Get_DisplayName() const { return m_DisplayName; }
	void Set_DisplayName(const wstring& displayName) { m_DisplayName = displayName; }
	int32 Get_Level() const { return m_Level; }
	void Set_Level(int32 level) { m_Level = level; }
	// Where the fill's middle falls on the 1920x1080 canvas now; false while the target is gone or behind the camera.
	Bool Get_CanvasPosition(Vector2& outPosition);
	Vector2 Get_BarSize() const { return m_BarSize; }
	// How much of the bar shows at the target's distance from the camera (1 near, 0 at HideDistance and past it).
	Float Get_DistanceAlpha();

private:
	HRESULT Ready_Components();
	HRESULT Ready_Layout();
	Shared<Entity> Resolve_Target();

private:
	Weak<Entity> m_Target{};
	ObjectGuid m_TargetGuid{};
	Vector3 m_WorldOffset = { 0.f, 0.f, 0.f };
	wstring m_DisplayName{};
	int32 m_Level{ 0 };
	// The fill's size the owner asked for in its desc; the layout puts it back over whatever rect a scene saved.
	Vector2 m_BarSize{ 200.f, 10.f };

	Weak<UIPanel> m_Layout{};
	Bool m_LayoutFailed{ false };	// logged once, not rebuilt every frame
	Weak<UIImage> m_Fill{};
	Weak<TextUI> m_NameText{};
	Weak<TextUI> m_LevelText{};
	// Every piece with its own colour, scaled by the distance fade.
	vector<pair<Weak<UIImage>, Color>> m_Images{};
	vector<pair<Weak<TextUI>, Color>> m_Texts{};
	Float m_Alpha{ 1.f };

	Shared<Shader> m_Shader{ nullptr };
	Shared<Texture> m_Texture{ nullptr };
	Shared<VIBuffer_Rect> m_BufferRect{ nullptr };

public:
	static Shared<HpBarWorldUI> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;

};

NS_END
