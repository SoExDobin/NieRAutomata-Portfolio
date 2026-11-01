#include "FontManager.h"
#include "Font.h"

FontManager::FontManager(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: EngineManager{}, m_Device{device}, m_Context{context} {}

HRESULT FontManager::Initialize_Prototype()
{
	return EngineManager::Initialize_Prototype();
}

HRESULT FontManager::Initialize(void* arg)
{
	return EngineManager::Initialize(arg);
}

void FontManager::On_Destroy()
{
	m_Fonts.clear();
	EngineManager::On_Destroy();
}

HRESULT FontManager::Clear_Fonts()
{
	m_Fonts.clear();

	return S_OK;
}

HRESULT FontManager::Add_Font(const wstring& fontTag, const tChar* fontFilePath)
{
	if (nullptr == Find_Font(fontTag))
	{
		m_Fonts.emplace(fontTag, Font::Create(m_Device, m_Context, fontFilePath));
		return S_OK;
	}

	return E_FAIL;
}

void FontManager::Draw(const wstring& fontTag, const tChar* text, const Vector2& position, const Color& color, Float scale, Float spacing)
{
	if (auto font = Find_Font(fontTag))
	{
		font->Draw(text, position, color, scale, spacing);
	}
}

Vector2 FontManager::Measure(const wstring& fontTag, const tChar* text, Float spacing)
{
	if (auto font = Find_Font(fontTag))
		return font->Measure(text, spacing);
	return Vector2::Zero;
}

Shared<Font> FontManager::Find_Font(const wstring& fontTag)
{
	if (m_Fonts.contains(fontTag))
		return m_Fonts.at(fontTag);

	return nullptr;
}

Unique<FontManager> FontManager::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto instance = make_unique<FontManager>(device, context);

	if (FAILED(instance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : FontManager");
		return nullptr;
	}

	return instance;
}
