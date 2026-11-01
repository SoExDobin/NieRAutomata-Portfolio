#include "VIBuffer_Instance.h"

VIBuffer_Instance::VIBuffer_Instance() : VIBuffer{} {}
VIBuffer_Instance::VIBuffer_Instance(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: VIBuffer{ device, context } {
}
VIBuffer_Instance::VIBuffer_Instance(const VIBuffer_Instance& rhs)
	: VIBuffer{ rhs }
	, m_InstanceStride{ rhs.m_InstanceStride }
	, m_NumInstances{ rhs.m_NumInstances }
	, m_IndexCountPerInstance{ rhs.m_IndexCountPerInstance }
{}

HRESULT VIBuffer_Instance::Initialize_Prototype() { return VIBuffer::Initialize_Prototype(); }
HRESULT VIBuffer_Instance::Initialize(void* arg) { return VIBuffer::Initialize(arg); }

HRESULT VIBuffer_Instance::Bind_Resources()
{
	ID3D11Buffer* buffers[] = { m_VB.Get(), m_VBInstance.Get() };
	uint32 strides[] = { m_VtxStride, m_InstanceStride };
	uint32 offsets[] = { 0, 0 };

	m_Context->IASetVertexBuffers(0, m_NumVtxBuffers, buffers, strides, offsets);

	if (m_IB)
	{
		DXGI_FORMAT fmt = m_IndexStride == 2 ?
			DXGI_FORMAT_R16_UINT :
			DXGI_FORMAT_R32_UINT;
		m_Context->IASetIndexBuffer(m_IB.Get(), fmt, 0);
	}

	m_Context->IASetPrimitiveTopology(m_PrimitiveType);
	return S_OK;
}

HRESULT VIBuffer_Instance::Render()
{
	if (m_IB)
		m_Context->DrawIndexedInstanced(m_IndexCountPerInstance, m_NumInstances, 0, 0, 0);
	else
		m_Context->DrawInstanced(m_NumVertices, m_NumInstances, 0, 0);
	Count_Draw(m_IB ? m_IndexCountPerInstance : m_NumVertices, m_NumInstances);

	return S_OK;
}

void VIBuffer_Instance::On_Destroy()
{
	m_VBInstance.Reset();
	VIBuffer::On_Destroy();
}
