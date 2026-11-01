#pragma once
#include "UIObject.h"


NS_BEGIN(Engine)

// A line of text placed like UIImage on the 1920x1080 canvas: its UI position is the text's anchor point, the line is
// centred on it vertically and Pivot (0 left, 0.5 centre, 1 right) sets which part of the line sits on it.
class ENGINE_DLL TextUI : public UIObject
{
public:
	typedef struct tagTextUIDesc : public UI_DESC
	{
		wstring fontTag;
		const tChar* text{ nullptr };
		Color color{ 1.f, 1.f, 1.f, 1.f };
		Float fontScale{ 1.f };	// on the canvas; the screen scales it with the view's height
		Float spacing{ 0.f };	// extra gap after each character, in font pixels
		Float pivot{ 0.f };
	} UI_TEXT_DESC;
public:
	explicit TextUI();
	explicit TextUI(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit TextUI(const TextUI& rhs);
	virtual ~TextUI() override = default;

public:
	const wstring& Get_FontTag() const { return m_fontTag; }
	void Set_FontTag(const wstring& newFontTag) { m_fontTag = newFontTag; }
	const wstring& Get_Text() const { return m_Text; }
	void Set_Text(const wstring& text) { m_Text = text; }
	const Color& Get_Color() const { return m_Color; }
	void Set_Color(const Color& color) { m_Color = color; }
	Float Get_FontScale() const { return m_FontScale; }
	void Set_FontScale(Float fontScale) { m_FontScale = fontScale; }
	Float Get_Spacing() const { return m_Spacing; }
	void Set_Spacing(Float spacing) { m_Spacing = spacing; }
	Float Get_Pivot() const { return m_Pivot; }
	void Set_Pivot(Float pivot) { m_Pivot = pivot; }
	// The line's width on the canvas.
	Float Get_CanvasWidth() const;

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;

public:
	void Priority_Update(Float timeDelta) override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	void Fixed_Update(Float fixedDelta) override;
	HRESULT Render() override;
	void Submit_RenderGroup() override;

protected:
	wstring m_fontTag{};
	wstring m_Text{};
	Color m_Color{ 1.f, 1.f, 1.f, 1.f };
	Float m_FontScale{ 1.f };
	Float m_Spacing{ 0.f };
	Float m_Pivot{ 0.f };

public:
	static Shared<TextUI> CreatePrototype();
	static Shared<TextUI> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg = nullptr) override;
};

NS_END