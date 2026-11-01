#include "RenderTarget.h"

#include "Shader.h"
#include "SpdLogger.h"
#include "VIBuffer_Rect.h"

RenderTarget::RenderTarget(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: m_Device{device}, m_Context{context}
{
}

void RenderTarget::Set_DebugName(const wstring& name) const
{
	const string narrow(name.begin(), name.end());
	const auto apply = [&](ID3D11DeviceChild* child, const string& value) {
		if (child)
			child->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(value.size()), value.data());
	};
	apply(m_Texture2D.Get(), narrow);
	apply(m_RenderTargetView.Get(), narrow + " RTV");
	apply(m_ShaderResourceView.Get(), narrow + " SRV");
}

HRESULT RenderTarget::Initialize(uint32 sizeX, uint32 sizeY, DXGI_FORMAT pixelFormat, const Color& clearColor)
{
	D3D11_TEXTURE2D_DESC texDesc{};
	texDesc.Width = sizeX;
	texDesc.Height = sizeY;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = pixelFormat;
	texDesc.SampleDesc.Quality = 0;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = 0;

	if (FAILED(m_Device->CreateTexture2D(&texDesc, nullptr, m_Texture2D.GetAddressOf())))
		return E_FAIL;

	if (FAILED(m_Device->CreateRenderTargetView(m_Texture2D.Get(), nullptr, m_RenderTargetView.GetAddressOf())))
		return E_FAIL;

	if (FAILED(m_Device->CreateShaderResourceView(m_Texture2D.Get(), nullptr, m_ShaderResourceView.GetAddressOf())))
		return E_FAIL;

	m_ClearColor = clearColor;

	return S_OK;
}

HRESULT RenderTarget::Begin()
{
	return Object::Begin();
}

Shared<RenderTarget> RenderTarget::Create_Resized(uint32 width, uint32 height) const
{
	D3D11_TEXTURE2D_DESC desc{};
	m_Texture2D->GetDesc(&desc);
	return Create(m_Device, m_Context, width, height, desc.Format, m_ClearColor);
}

Bool RenderTarget::Has_Size(uint32 width, uint32 height) const
{
	D3D11_TEXTURE2D_DESC desc{};
	m_Texture2D->GetDesc(&desc);
	return desc.Width == width && desc.Height == height;
}

HRESULT RenderTarget::Bind_ShaderResource(const Shared<Shader>& shader, const Char* constantName) const
{
	return shader->Bind_SRV(constantName, m_ShaderResourceView);
}

void RenderTarget::Clear_RenderTarget() const
{
	m_Context->ClearRenderTargetView(m_RenderTargetView.Get(), m_ClearColor);
}

HRESULT RenderTarget::Copy_From(ID3D11Resource* source) const
{
	ComPtr<ID3D11Texture2D> sourceTexture;
	if (!source || FAILED(source->QueryInterface(IID_PPV_ARGS(sourceTexture.GetAddressOf()))))
		return E_INVALIDARG;
	D3D11_TEXTURE2D_DESC sourceDesc{}, desc{};
	sourceTexture->GetDesc(&sourceDesc);
	m_Texture2D->GetDesc(&desc);
	if (sourceDesc.Width != desc.Width || sourceDesc.Height != desc.Height || sourceDesc.Format != desc.Format ||
		sourceDesc.SampleDesc.Count != 1)
		return E_FAIL;
	m_Context->CopyResource(m_Texture2D.Get(), sourceTexture.Get());
	return S_OK;
}

Shared<RenderTarget> RenderTarget::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context, uint32 sizeX, uint32 sizeY, DXGI_FORMAT pixelFormat, const Color& clearColor)
{
	auto renderTarget = make_shared<RenderTarget>(device, context);

	if (FAILED(renderTarget->Initialize(sizeX, sizeY, pixelFormat, clearColor)))
	{
		LOG_CRITICAL(L"Failed to Create Render Target");
		return nullptr;
	}

	return renderTarget;
}

#ifdef _DEBUG
HRESULT RenderTarget::Ready_Debug(Float x, Float y, Float sizeX, Float sizeY)
{
	uint32			numViewports = { 1 };
	D3D11_VIEWPORT	viewportDesc{};
	m_Context->RSGetViewports(&numViewports, &viewportDesc);

	m_WorldMatrix = Matrix::Identity;

	m_WorldMatrix._11 = sizeX;
	m_WorldMatrix._22 = sizeY;

	m_WorldMatrix._41 = x - viewportDesc.Width * 0.5f;
	m_WorldMatrix._42 = -y + viewportDesc.Height * 0.5f;

	return S_OK;
}

HRESULT RenderTarget::Render_Debug(const Shared<VIBuffer_Rect>& buffer, const Shared<Shader>& shader) const
{
	shader->Bind_Matrix(WorldMatrix, &m_WorldMatrix);
	shader->Bind_SRV(DefaultMap, m_ShaderResourceView);
	shader->Begin(0);
	buffer->Bind_Resources();
	buffer->Render();

	return S_OK;
}
#endif
