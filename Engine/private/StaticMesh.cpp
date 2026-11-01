#include "StaticMesh.h"

#include "SpdLogger.h"

StaticMesh::StaticMesh() : Mesh{} {}
StaticMesh::StaticMesh(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: Mesh{ device, context } {}
StaticMesh::StaticMesh(const StaticMesh& rhs)
	: Mesh{ rhs } {}

HRESULT StaticMesh::Ready_VertexBuffer(const MODEL_MESH& meshData, const Matrix& preTransformMatrix)
{
	m_VtxStride = sizeof(VTXMESH);
	m_NumVertices = meshData.vertices.size();

	D3D11_BUFFER_DESC vertexBufferDesc{};
	vertexBufferDesc.ByteWidth = m_VtxStride * m_NumVertices;
	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.StructureByteStride = m_VtxStride;
	vertexBufferDesc.CPUAccessFlags = 0;
	vertexBufferDesc.MiscFlags = 0;

	VTXMESH* vertices = new VTXMESH[m_NumVertices];
	for (size_t i = 0; i < m_NumVertices; i++)
	{
		Vector3 position = meshData.vertices[i].position;
		position = XMVector3TransformCoord(position, preTransformMatrix);
		vertices[i].position = position;

		Vector3 normal = meshData.vertices[i].normal;
		normal = XMVector3TransformNormal(normal, preTransformMatrix);
		vertices[i].normal = XMVector3Normalize(normal);

		Vector3 tangent = meshData.vertices[i].tangent;
		tangent = XMVector3TransformNormal(tangent, preTransformMatrix);
		vertices[i].tangent = XMVector3Normalize(tangent);

		vertices[i].texcoord = meshData.vertices[i].texcoord;
		vertices[i].texcoord1 = meshData.vertices[i].texcoord1;
		vertices[i].color = meshData.vertices[i].color;
	}

	m_RawPosition.reserve(m_NumVertices * 3);
	for (uint32 i = 0; i < m_NumVertices; ++i)
	{
		m_RawPosition.push_back(vertices[i].position.x);
		m_RawPosition.push_back(vertices[i].position.y);
		m_RawPosition.push_back(vertices[i].position.z);
	}

	D3D11_SUBRESOURCE_DATA vertexInitialData{};
	vertexInitialData.pSysMem = vertices;

	if (FAILED(m_Device->CreateBuffer(&vertexBufferDesc, &vertexInitialData, &m_VB)))
	{
		LOG_ERROR(L"Failed to Create Mesh vertices");
		MSG_BOX("Failed to Create Mesh vertices");
		delete[] vertices;
		return E_FAIL;
	}

	delete[] vertices;
	return S_OK;
}
