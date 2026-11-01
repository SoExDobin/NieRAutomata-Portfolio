#pragma once
#include "EngineManager.h"

NS_BEGIN(Engine)

class Font;

class FontManager final : public EngineManager
{
public:
	explicit FontManager(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	~FontManager() override = default;
	
public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	void On_Destroy() override;

public:
	HRESULT Clear_Fonts();
	
public:
	HRESULT Add_Font(const wstring& fontTag, const tChar* fontFilePath);
	void Draw(const wstring& fontTag, const tChar* text, const Vector2& position, const Color& color, Float scale, Float spacing);
	Vector2 Measure(const wstring& fontTag, const tChar* text, Float spacing);

private:
	Shared<Font> Find_Font(const wstring& fontTag);

private:
	ComPtr<ID3D11Device> m_Device = { nullptr };
	ComPtr<ID3D11DeviceContext> m_Context = { nullptr };
	map<wstring, Shared<class Font>> m_Fonts;

public:
	static Unique<FontManager> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
};

NS_END