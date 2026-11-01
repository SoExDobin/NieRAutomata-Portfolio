#pragma once
#include "UIObject.h"

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

/* The player's HP in the top-left corner (the monster bar is HpBarWorldUI), as the reference draws it
   (1080p capture): a small square over each end, the HP fill over a dark backing, a faint line and
   a bright one under it ending in a ring on the left and a dot on the right, and "Lv : n" below. Its UI rect is the
   fill, put on the reference's spot when the pieces are built; the pieces are UIImage / TextUI children made on the
   first Late_Update, so they only exist while the game runs and the scene documents keep the bar childless. */
class CLIENT_DLL Pl0000HpBar final : public UIObject
{
public:
	typedef struct tagPl0000HpBarDesc : public UI_DESC
	{
		Shared<Entity> target{ nullptr };
	} PL0000_HP_BAR_DESC;

public:
	explicit Pl0000HpBar();
	explicit Pl0000HpBar(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Pl0000HpBar(const Pl0000HpBar& rhs);
	virtual ~Pl0000HpBar() override = default;

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;

public:
	void Priority_Update(Float timeDelta) override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;

public:
	// Inspector: the level under the bar (none drawn at 0).
	int32 Get_Level() const { return m_Level; }
	void Set_Level(int32 level) { m_Level = level; }

private:
	HRESULT Ready_Components();
	HRESULT Ready_Layout();

private:
	Weak<Entity> m_Target{};
	int32 m_Level{ 0 };

	Weak<UIPanel> m_Layout{};
	Bool m_LayoutFailed{ false };	// logged once, not rebuilt every frame
	Weak<UIImage> m_Fill{};
	Weak<TextUI> m_LevelLabel{};
	Weak<TextUI> m_LevelText{};

	Shared<Shader> m_Shader{ nullptr };
	Shared<Texture> m_Texture{ nullptr };
	Shared<VIBuffer_Rect> m_BufferRect{ nullptr };

public:
	static Shared<Pl0000HpBar> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;
};

NS_END
