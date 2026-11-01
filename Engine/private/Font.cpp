#include "Font.h"

Font::Font() {}
Font::Font(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context) 
	: m_Device{ device }, m_Context{context} {}
Font::Font(const Font& rhs) {}

HRESULT Font::Initialize_Prototype(const tChar* fontFilePath)
{
	m_Batch = make_shared<SpriteBatch>(m_Context.Get());
	m_Font = make_shared<SpriteFont>(m_Device.Get(), fontFilePath);

	return S_OK;
}

HRESULT Font::Initialize(void* arg) { return Object::Initialize(arg); }

namespace
{
	// How far SpriteFont moves on after a character: its offset, its cell and the gap it leaves.
	Float Get_Advance(const SpriteFont& font, tChar character)
	{
		const SpriteFont::Glyph* glyph = font.FindGlyph(character);
		return glyph->XOffset + static_cast<Float>(glyph->Subrect.right - glyph->Subrect.left) + glyph->XAdvance;
	}
}

void Font::Draw(const tChar* text, const Vector2& position, const Color& color, Float scale, Float spacing)
{
	m_Context->GSSetShader(nullptr, nullptr, 0);

	// SpriteBatch blends premultiplied colour, so a fading colour scales its rgb too.
	const Color premultiplied{ color.x * color.w, color.y * color.w, color.z * color.w, color.w };
	m_Batch->Begin();

	if (spacing == 0.f)
		m_Font->DrawString(m_Batch.get(), text, position, premultiplied, 0.f, Vector2::Zero, scale);
	else
	{
		Float x = 0.f;
		for (const tChar* character = text; *character; ++character)
		{
			const tChar single[2]{ *character, 0 };
			m_Font->DrawString(m_Batch.get(), single, Vector2{ position.x + x * scale, position.y }, premultiplied,
				0.f, Vector2::Zero, scale);
			x += Get_Advance(*m_Font, *character) + spacing;
		}
	}

	m_Batch->End();
}

Vector2 Font::Measure(const tChar* text, Float spacing) const
{
	Float width = 0.f;
	if (spacing == 0.f)
		width = XMVectorGetX(m_Font->MeasureString(text, false));
	else if (*text)
	{
		for (const tChar* character = text; *character; ++character)
			width += Get_Advance(*m_Font, *character) + spacing;
		width -= spacing;
	}
	const SpriteFont::Glyph* capital = m_Font->FindGlyph(L'H');
	const Float middle = capital->YOffset + static_cast<Float>(capital->Subrect.bottom - capital->Subrect.top) * 0.5f;
	return Vector2{ width, middle };
}

Shared<Font> Font::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context, const tChar* fontFilePath)
{
	auto instance = make_shared<Font>(device, context);

	if (FAILED(instance->Initialize_Prototype(fontFilePath)))
	{
		MSG_BOX("Failed to Created : Font");
		return nullptr;
	}

	return instance;
}
