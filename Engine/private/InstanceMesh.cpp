#include "InstanceMesh.h"

#include "Game.h"
#include "SpdLogger.h"

InstanceMesh::InstanceMesh() : StaticMesh{} {}
InstanceMesh::InstanceMesh(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: StaticMesh{ device, context } {}
InstanceMesh::InstanceMesh(const InstanceMesh& rhs)
	: StaticMesh{ rhs }
	, m_InstanceStride{ rhs.m_InstanceStride }
{}
InstanceMesh::InstanceMesh(const StaticMesh& source)
	: StaticMesh{ source }
{}

HRESULT InstanceMesh::Set_Instances(const vector<INSTANCE_DATA>& instances)
{
	m_NumInstances = static_cast<uint32>(instances.size());
	m_Instances = instances;
	if (0 == m_NumInstances)
		return S_OK;

	if (m_NumInstances > m_InstanceCapacity)
	{
		D3D11_BUFFER_DESC instanceBufferDesc{};
		instanceBufferDesc.ByteWidth = m_InstanceStride * m_NumInstances;
		instanceBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		instanceBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		instanceBufferDesc.StructureByteStride = m_InstanceStride;
		instanceBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		instanceBufferDesc.MiscFlags = 0;

		D3D11_SUBRESOURCE_DATA instanceInitialData{};
		instanceInitialData.pSysMem = instances.data();

		m_VBInstance.Reset();
		m_InstanceCapacity = 0;
		if (FAILED(m_Device->CreateBuffer(&instanceBufferDesc, &instanceInitialData, &m_VBInstance)))
		{
			LOG_ERROR(L"Failed to Create InstanceMesh instances {}", Helper::To_wString(m_MeshName));
			m_NumInstances = 0;
			return E_FAIL;
		}

		m_InstanceCapacity = m_NumInstances;
		m_NumVtxBuffers = 2;
		return S_OK;
	}

	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (FAILED(m_Context->Map(m_VBInstance.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		LOG_ERROR(L"Failed to Map InstanceMesh instances {}", Helper::To_wString(m_MeshName));
		m_NumInstances = 0;
		return E_FAIL;
	}

	memcpy(mapped.pData, instances.data(), static_cast<size_t>(m_InstanceStride) * m_NumInstances);
	m_Context->Unmap(m_VBInstance.Get(), 0);

	return S_OK;
}

HRESULT InstanceMesh::Bind_Resources()
{
	if (!m_VBInstance)
		return StaticMesh::Bind_Resources();

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

HRESULT InstanceMesh::Render()
{
	const uint32 numInstances = m_NumInstances;
	if (0 == numInstances || !m_VBInstance)
		return S_OK;

	if (GAME_INSTANCE->Get_Instancing())
	{
		m_Context->DrawIndexedInstanced(m_NumIndices, numInstances, 0, 0, 0);
		Count_Draw(m_NumIndices, numInstances);
		GAME_INSTANCE->Add_InstancedProps(numInstances);
		return S_OK;
	}

	// F7 끔: 인스턴싱이 없는 렌더러가 오브젝트마다 world를 올리듯, 인스턴스마다 한 개짜리 버퍼에 그 world를 쓰고
	// 바인딩한 뒤 그린다. 이 업로드·바인딩 비용까지 내야 인스턴싱과 공정하게 비교된다.
	if (!m_VBSingle)
	{
		D3D11_BUFFER_DESC singleDesc{};
		singleDesc.ByteWidth = m_InstanceStride;
		singleDesc.Usage = D3D11_USAGE_DYNAMIC;
		singleDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		singleDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		if (FAILED(m_Device->CreateBuffer(&singleDesc, nullptr, &m_VBSingle)))
		{
			LOG_ERROR(L"Failed to Create InstanceMesh single instance buffer {}", Helper::To_wString(m_MeshName));
			return E_FAIL;
		}
	}

	const uint32 offset = 0;
	for (uint32 i = 0; i < numInstances; ++i)
	{
		D3D11_MAPPED_SUBRESOURCE mapped{};
		if (FAILED(m_Context->Map(m_VBSingle.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
			return E_FAIL;
		memcpy(mapped.pData, &m_Instances[i], m_InstanceStride);
		m_Context->Unmap(m_VBSingle.Get(), 0);

		m_Context->IASetVertexBuffers(1, 1, m_VBSingle.GetAddressOf(), &m_InstanceStride, &offset);
		m_Context->DrawIndexedInstanced(m_NumIndices, 1, 0, 0, 0);
		Count_Draw(m_NumIndices);
	}
	return S_OK;
}

void InstanceMesh::On_Destroy()
{
	m_VBInstance.Reset();
	m_VBSingle.Reset();
	m_Instances.clear();
	m_InstanceCapacity = 0;
	m_NumInstances = 0;
	StaticMesh::On_Destroy();
}
