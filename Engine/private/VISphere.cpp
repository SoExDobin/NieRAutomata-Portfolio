#include "VISphere.h"
#include "Game.h"

VISphere::VISphere() : VIBuffer{} {}
VISphere::VISphere(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: VIBuffer{ device, context } {}
VISphere::VISphere(const VISphere& rhs)
	: VIBuffer{ rhs } {}

HRESULT VISphere::Initialize_Prototype(uint32 numSlices, uint32 numStacks)
{
	m_NumVtxBuffers = 1;
	m_VtxStride = sizeof(VTXCUBE); // Reusing VTXCUBE (Pos + 3D Texcoord)
	m_IndexStride = sizeof(uShort);
	m_PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	vector<VTXCUBE> vertices;
	
	// Top vertex
	VTXCUBE top;
	top.position = Float3(0.0f, 0.5f, 0.0f);
	top.texcoord = top.position;
	vertices.push_back(top);

	Float phiStep = XM_PI / static_cast<Float>(numStacks);
	Float thetaStep = XM_2PI / static_cast<Float>(numSlices);

	

	for (uint32 i = 1; i <= numStacks - 1; ++i)
	{
		Float phi = i * phiStep;
		for (uint32 j = 0; j <= numSlices; ++j)
		{
			Float theta = j * thetaStep;
			VTXCUBE v;
			v.position.x = 0.5f * sinf(phi) * cosf(theta);
			v.position.y = 0.5f * cosf(phi);
			v.position.z = 0.5f * sinf(phi) * sinf(theta);
			v.texcoord.x = theta / XM_2PI;
			v.texcoord.y = phi / XM_PI;
			v.texcoord.z = 0.f;
			vertices.push_back(v);
		}
	}

	// Bottom vertex
	VTXCUBE bottom;
	bottom.position = Float3(0.0f, -0.5f, 0.0f);
	bottom.texcoord = bottom.position;
	vertices.push_back(bottom);

	m_NumVertices = static_cast<uint32>(vertices.size());

	D3D11_BUFFER_DESC VertexBufferDesc{};
	VertexBufferDesc.ByteWidth = m_VtxStride * m_NumVertices;
	VertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	VertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	VertexBufferDesc.StructureByteStride = m_VtxStride;

	D3D11_SUBRESOURCE_DATA VertexInitialData{};
	VertexInitialData.pSysMem = vertices.data();

	if (FAILED(m_Device->CreateBuffer(&VertexBufferDesc, &VertexInitialData, m_VB.GetAddressOf())))
		return E_FAIL;

	// Indices
	vector<uShort> indices;

	// Top cap
	for (uint32 i = 1; i <= numSlices; ++i)
	{
		indices.push_back(0);
		indices.push_back(i + 1);
		indices.push_back(i);
	}

	// Middle stacks
	uint32 baseIndex = 1;
	uint32 ringVertexCount = numSlices + 1;
	for (uint32 i = 0; i < numStacks - 2; ++i)
	{
		for (uint32 j = 0; j < numSlices; ++j)
		{
			indices.push_back(baseIndex + i * ringVertexCount + j);
			indices.push_back(baseIndex + i * ringVertexCount + j + 1);
			indices.push_back(baseIndex + (i + 1) * ringVertexCount + j);

			indices.push_back(baseIndex + (i + 1) * ringVertexCount + j);
			indices.push_back(baseIndex + i * ringVertexCount + j + 1);
			indices.push_back(baseIndex + (i + 1) * ringVertexCount + j + 1);
		}
	}

	// Bottom cap
	uint32 southPoleIndex = static_cast<uint32>(vertices.size()) - 1;
	baseIndex = southPoleIndex - ringVertexCount;
	for (uint32 i = 0; i < numSlices; ++i)
	{
		indices.push_back(southPoleIndex);
		indices.push_back(baseIndex + i);
		indices.push_back(baseIndex + i + 1);
	}

	m_NumIndices = static_cast<uint32>(indices.size());

	D3D11_BUFFER_DESC IndexBufferDesc{};
	IndexBufferDesc.ByteWidth = m_IndexStride * m_NumIndices;
	IndexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	IndexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	IndexBufferDesc.StructureByteStride = m_IndexStride;

	D3D11_SUBRESOURCE_DATA IndexInitialData{};
	IndexInitialData.pSysMem = indices.data();

	if (FAILED(m_Device->CreateBuffer(&IndexBufferDesc, &IndexInitialData, m_IB.GetAddressOf())))
		return E_FAIL;

	return VIBuffer::Initialize_Prototype();
}

HRESULT VISphere::Initialize(void* arg)
{
	return VIBuffer::Initialize(arg);
}

Shared<VISphere> VISphere::CreatePrototype()
{
	auto prototype = make_shared<VISphere>(GAME_INSTANCE->Get_Device(), GAME_INSTANCE->Get_Context());

	uint32 numSlices = 20; uint32 numStacks = 20;
	if (FAILED(prototype->Initialize_Prototype(numSlices, numStacks)))
	{
		MSG_BOX("Failed to Created : VISphere");
		return nullptr;
	}
	return prototype;
}

Shared<VISphere> VISphere::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context, uint32 numSlices, uint32 numStacks)
{
	auto prototype = make_shared<VISphere>(device, context);
	if (FAILED(prototype->Initialize_Prototype(numSlices, numStacks)))
	{
		MSG_BOX("Failed to Created : VISphere");
		return nullptr;
	}
	return prototype;
}

Shared<Component> VISphere::Clone(void* arg)
{
	auto instance = make_shared<VISphere>(*this);
	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : VISphere");
		return nullptr;
	}
	return instance;
}
