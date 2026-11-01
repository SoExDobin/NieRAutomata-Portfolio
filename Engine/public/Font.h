#pragma once
#include "Object.h"

NS_BEGIN(Engine)

class Font final : public Object
{
public:
	explicit Font();
	explicit Font(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Font(const Font& rhs);
	~Font() override = default;

public:
	HRESULT Initialize_Prototype(const tChar* fontFilePath);
	HRESULT Initialize(void* arg) override;

public:
	// position is the top-left of the text; scale multiplies the sheet's size and spacing (in sheet pixels) widens the
	// gap after every character.
	void Draw(const tChar* text, const Vector2& position, const Color& color, Float scale = 1.f, Float spacing = 0.f);
	// x: the drawn width at scale 1; y: the middle of a capital letter below the top-left, for centring a line.
	Vector2 Measure(const tChar* text, Float spacing = 0.f) const;

private:
	ComPtr<ID3D11Device> m_Device{ nullptr };
	ComPtr<ID3D11DeviceContext> m_Context{ nullptr };

	Shared<SpriteBatch> m_Batch{ nullptr };
	Shared<SpriteFont> m_Font{ nullptr };

public:
	static Shared<Font> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context, const tChar* fontFilePath);
	
};

NS_END
