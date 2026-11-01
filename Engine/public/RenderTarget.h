#pragma once
#include "Object.h"

NS_BEGIN(Engine)

class Shader;
class VIBuffer_Rect;

class ENGINE_DLL RenderTarget final : public Object
{
public:
	explicit RenderTarget(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	~RenderTarget() override = default;

public:
	HRESULT Initialize(uint32 sizeX, uint32 sizeY, DXGI_FORMAT pixelFormat, const Color& clearColor);
	HRESULT Begin() override;

	ComPtr<ID3D11RenderTargetView> Get_RenderTargetView() const { return m_RenderTargetView; }
	ComPtr<ID3D11ShaderResourceView> Get_ShaderResourceView() const { return m_ShaderResourceView; }
	HRESULT Bind_ShaderResource(const Shared<Shader>& shader, const Char* constantName) const;
	void Clear_RenderTarget() const;
	// Copies a texture of the same size and format (the screen the renderer is drawing to) into this target.
	HRESULT Copy_From(ID3D11Resource* source) const;
	Shared<RenderTarget> Create_Resized(uint32 width, uint32 height) const;
	Bool Has_Size(uint32 width, uint32 height) const;
	// Names the texture and views for graphics debuggers (RenderDoc, PIX, D3D debug layer).
	void Set_DebugName(const wstring& name) const;

private:
	Color m_ClearColor{};
	ComPtr<ID3D11Device> m_Device{ nullptr };
	ComPtr<ID3D11DeviceContext> m_Context{ nullptr };
	ComPtr<ID3D11Texture2D> m_Texture2D{ nullptr };
	ComPtr<ID3D11RenderTargetView> m_RenderTargetView{ nullptr };
	ComPtr<ID3D11ShaderResourceView> m_ShaderResourceView{ nullptr };

public:
	static Shared<RenderTarget> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context, 
		uint32 sizeX, uint32 sizeY, DXGI_FORMAT pixelFormat, const Color& clearColor);
	
#ifdef _DEBUG
private:
	Matrix		m_WorldMatrix = {};
public:
	HRESULT Ready_Debug(Float x, Float y, Float sizeX, Float sizeY);
	HRESULT Render_Debug(const Shared<VIBuffer_Rect>& buffer, const Shared<Shader>& shader) const;
#endif
};

NS_END
