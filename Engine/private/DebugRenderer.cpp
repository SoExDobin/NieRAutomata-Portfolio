#include "DebugRenderer.h"

#ifdef _DEBUG
#include "SpdLogger.h"

HRESULT DebugRenderer::Initialize(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	m_Context = context;
	m_Batch = make_shared<PrimitiveBatch<VertexPositionColor>>(context.Get());
	m_Effect = make_shared<BasicEffect>(device.Get());
	m_Effect->SetVertexColorEnabled(true);

	const void* shaderByteCode = nullptr;
	size_t shaderByteCodeLength = 0;
	m_Effect->GetVertexShaderBytecode(&shaderByteCode, &shaderByteCodeLength);
	if (FAILED(device->CreateInputLayout(VertexPositionColor::InputElements, VertexPositionColor::InputElementCount,
		shaderByteCode, shaderByteCodeLength, m_InputLayout.GetAddressOf())))
		return E_FAIL;

	D3D11_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	if (FAILED(device->CreateBlendState(&blendDesc, m_FaceBlendState.GetAddressOf())))
		return E_FAIL;

	D3D11_DEPTH_STENCIL_DESC depthDesc{};
	depthDesc.DepthEnable = TRUE;
	depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	if (FAILED(device->CreateDepthStencilState(&depthDesc, m_FaceDepthState.GetAddressOf())))
		return E_FAIL;

	// 충돌 면은 보이는 메시와 거의 같은 자리라 카메라 쪽으로 당겨 깜빡이지 않게 한다.
	D3D11_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_NONE;
	rasterizerDesc.DepthBias = -1000;
	rasterizerDesc.SlopeScaledDepthBias = -1.f;
	rasterizerDesc.DepthClipEnable = TRUE;
	if (FAILED(device->CreateRasterizerState(&rasterizerDesc, m_FaceRasterizerState.GetAddressOf())))
		return E_FAIL;

	return S_OK;
}

void DebugRenderer::Apply_Effect(const Matrix& world, const Matrix& view, const Matrix& proj)
{
	m_Effect->SetWorld(world);
	m_Effect->SetView(view);
	m_Effect->SetProjection(proj);
	m_Effect->Apply(m_Context.Get());
	m_Context->GSSetShader(nullptr, nullptr, 0);
	m_Context->IASetInputLayout(m_InputLayout.Get());
}

const Shared<PrimitiveBatch<VertexPositionColor>>& DebugRenderer::Begin_Lines(const Matrix& world, const Matrix& view, const Matrix& proj)
{
	Apply_Effect(world, view, proj);
	m_Batch->Begin();
	return m_Batch;
}

void DebugRenderer::End_Lines()
{
	m_Batch->End();
}

void DebugRenderer::Begin_Faces(const Matrix& view, const Matrix& proj)
{
	m_Context->OMGetBlendState(m_SavedBlendState.ReleaseAndGetAddressOf(), m_SavedBlendFactor, &m_SavedSampleMask);
	m_Context->OMGetDepthStencilState(m_SavedDepthState.ReleaseAndGetAddressOf(), &m_SavedStencilRef);
	m_Context->RSGetState(m_SavedRasterizerState.ReleaseAndGetAddressOf());

	Apply_Effect(Matrix::Identity, view, proj);
	m_Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_Context->OMSetBlendState(m_FaceBlendState.Get(), nullptr, 0xffffffff);
	m_Context->OMSetDepthStencilState(m_FaceDepthState.Get(), 0);
	m_Context->RSSetState(m_FaceRasterizerState.Get());
}

void DebugRenderer::Draw_Faces(ID3D11Buffer* vertexBuffer, uint32 startVertex, uint32 vertexCount)
{
	const UINT stride = sizeof(VertexPositionColor);
	const UINT offset = 0;
	m_Context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
	m_Context->Draw(vertexCount, startVertex);
}

void DebugRenderer::End_Faces()
{
	m_Context->OMSetBlendState(m_SavedBlendState.Get(), m_SavedBlendFactor, m_SavedSampleMask);
	m_Context->OMSetDepthStencilState(m_SavedDepthState.Get(), m_SavedStencilRef);
	m_Context->RSSetState(m_SavedRasterizerState.Get());
	m_SavedBlendState.Reset();
	m_SavedDepthState.Reset();
	m_SavedRasterizerState.Reset();
}

Unique<DebugRenderer> DebugRenderer::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto debugRenderer = make_unique<DebugRenderer>();
	if (FAILED(debugRenderer->Initialize(device, context)))
	{
		LOG_ERROR(L"Failed to Create DebugRenderer");
		return nullptr;
	}
	return debugRenderer;
}
#endif
