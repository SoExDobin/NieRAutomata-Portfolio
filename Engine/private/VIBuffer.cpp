#include "VIBuffer.h"
#include "Game.h"

VIBuffer::VIBuffer()
	: Component{}
{
}

VIBuffer::VIBuffer(ComPtr<ID3D11Device> device,
                   ComPtr<ID3D11DeviceContext> context)
    : Component(device, context) {}

VIBuffer::VIBuffer(const VIBuffer& rhs)
    : Component{rhs}, m_VB{rhs.m_VB}, m_IB{rhs.m_IB},
      m_NumVtxBuffers{rhs.m_NumVtxBuffers}, m_NumVertices{rhs.m_NumVertices},
      m_VtxStride{rhs.m_VtxStride}, m_NumIndices{rhs.m_NumIndices},
      m_IndexStride{rhs.m_IndexStride}, m_PrimitiveType{rhs.m_PrimitiveType} {
}

HRESULT VIBuffer::Initialize_Prototype() {
	return Component::Initialize_Prototype();
}

HRESULT VIBuffer::Initialize(void *arg) { return Component::Initialize(arg); }


HRESULT VIBuffer::Bind_Resources() {
    ID3D11Buffer* pVBs[] = { m_VB.Get() };
    uint32 vtxStrides[] = { m_VtxStride, };
    uint32 offsets[] = { 0, };

    m_Context->IASetVertexBuffers(0, m_NumVtxBuffers, pVBs, vtxStrides, offsets);
    m_Context->IASetIndexBuffer(m_IB.Get(), 2 == m_IndexStride ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0);
    m_Context->IASetPrimitiveTopology(m_PrimitiveType);

    ComPtr<ID3D11InputLayout> inputLayout = {nullptr};

    return S_OK;
}

HRESULT VIBuffer::Render() {
    m_Context->DrawIndexed(m_NumIndices, 0, 0);
    Count_Draw(m_NumIndices);

    return S_OK;
}

void VIBuffer::Count_Draw(uint32 indexCount, uint32 instanceCount) const {
    uint32 triangles = 0;
    if (D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST == m_PrimitiveType)
        triangles = indexCount / 3;
    else if (D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP == m_PrimitiveType && indexCount >= 3)
        triangles = indexCount - 2;
    GAME_INSTANCE->Add_DrawStats(triangles * instanceCount);
}