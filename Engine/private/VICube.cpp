#include "VICube.h"

#include "Game.h"

VICube::VICube() : VIBuffer{} {}
VICube::VICube(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: VIBuffer{device, context} {}
VICube::VICube(const VICube& rhs)
	: VIBuffer{ rhs } {}

HRESULT VICube::Initialize_Prototype()
{
	m_NumVtxBuffers = 1;
	m_NumVertices = 8;
	m_VtxStride = sizeof(VTXCUBE);
	m_NumIndices = 36;
	m_IndexStride = 2;
	m_PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	D3D11_BUFFER_DESC	VertexBufferDesc{};
	VertexBufferDesc.ByteWidth = m_VtxStride * m_NumVertices;
	VertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	VertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	VertexBufferDesc.StructureByteStride = m_VtxStride;
	VertexBufferDesc.CPUAccessFlags = 0;
	VertexBufferDesc.MiscFlags = 0;

	VTXCUBE* vertices = new VTXCUBE[m_NumVertices];
	vertices[0].position = Float3(-0.5f, 0.5f, -0.5f);
	vertices[0].texcoord = vertices[0].position;

	vertices[1].position = Float3(0.5f, 0.5f, -0.5f);
	vertices[1].texcoord = vertices[1].position;

	vertices[2].position = Float3(0.5f, -0.5f, -0.5f);
	vertices[2].texcoord = vertices[2].position;

	vertices[3].position = Float3(-0.5f, -0.5f, -0.5f);
	vertices[3].texcoord = vertices[3].position;


	vertices[4].position = Float3(-0.5f, 0.5f, 0.5f);
	vertices[4].texcoord = vertices[4].position;

	vertices[5].position = Float3(0.5f, 0.5f, 0.5f);
	vertices[5].texcoord = vertices[5].position;

	vertices[6].position = Float3(0.5f, -0.5f, 0.5f);
	vertices[6].texcoord = vertices[6].position;

	vertices[7].position = Float3(-0.5f, -0.5f, 0.5f);
	vertices[7].texcoord = vertices[7].position;

	D3D11_SUBRESOURCE_DATA	VertexInitialData{};
	VertexInitialData.pSysMem = vertices;

	if (FAILED(m_Device->CreateBuffer(&VertexBufferDesc, &VertexInitialData, m_VB.GetAddressOf())))
		return E_FAIL;

	D3D11_BUFFER_DESC	IndexBufferDesc{};
	IndexBufferDesc.ByteWidth = m_IndexStride * m_NumIndices;
	IndexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	IndexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	IndexBufferDesc.StructureByteStride = m_IndexStride;
	IndexBufferDesc.CPUAccessFlags = 0;
	IndexBufferDesc.MiscFlags = 0;

	uShort* indices = new uShort[m_NumIndices];
	indices[0] = 1; indices[1] = 5; indices[2] = 6;
	indices[3] = 1; indices[4] = 6; indices[5] = 2;

	indices[6] = 4; indices[7] = 0; indices[8] = 3;
	indices[9] = 4; indices[10] = 3; indices[11] = 7;

	indices[12] = 4; indices[13] = 5; indices[14] = 1;
	indices[15] = 4; indices[16] = 1; indices[17] = 0;

	indices[18] = 3; indices[19] = 2; indices[20] = 6;
	indices[21] = 3; indices[22] = 6; indices[23] = 7;

	indices[24] = 7; indices[25] = 6; indices[26] = 5;
	indices[27] = 7; indices[28] = 5; indices[29] = 4;

	indices[30] = 0; indices[31] = 1; indices[32] = 2;
	indices[33] = 0; indices[34] = 2; indices[35] = 3;


	D3D11_SUBRESOURCE_DATA IndexInitialData{};
	IndexInitialData.pSysMem = indices;

	if (FAILED(m_Device->CreateBuffer(&IndexBufferDesc, &IndexInitialData, m_IB.GetAddressOf())))
		return E_FAIL;

	delete[] vertices;
	delete[] indices;

	return VIBuffer::Initialize_Prototype();
}

HRESULT VICube::Initialize(void* arg)
{
	return VIBuffer::Initialize(arg);
}

Shared<VICube> VICube::CreatePrototype()
{
	auto prototype = make_shared<VICube>(GAME_INSTANCE->Get_Device(), GAME_INSTANCE->Get_Context());

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : VICube");
		return nullptr;
	}

	return prototype;
}

Shared<VICube> VICube::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<VICube>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : VICube");
		return nullptr;
	}

	return prototype;
}

Shared<Component> VICube::Clone(void* arg)
{
	auto instance = make_shared<VICube>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : VICube");
		return nullptr;
	}

	return instance;
}


