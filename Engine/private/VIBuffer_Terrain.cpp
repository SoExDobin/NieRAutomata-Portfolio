#include "VIBuffer_Terrain.h"
#include "Game.h"
#include "SpdLogger.h"

VIBuffer_Terrain::VIBuffer_Terrain()
	: VIBuffer{} {}

VIBuffer_Terrain::VIBuffer_Terrain(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: VIBuffer{ device, context }, m_NumVerticesX{ 0 }, m_NumVerticesZ{ 0 } {}

VIBuffer_Terrain::VIBuffer_Terrain(const VIBuffer_Terrain& rhs)
	: VIBuffer { rhs }, m_NumVerticesX{rhs.m_NumVerticesX}, m_NumVerticesZ{rhs.m_NumVerticesZ} {}

HRESULT VIBuffer_Terrain::Initialize_Prototype(const tChar* heightMapFilePath)
{
	uLong byte{};
	HANDLE fileHandle{ INVALID_HANDLE_VALUE };
	uint32_t* pixels{ nullptr };
	m_NumVertices = {1};

	if (heightMapFilePath != L"")
	{
		fileHandle = CreateFile(
			heightMapFilePath,
			GENERIC_READ,
			0,
			nullptr,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			nullptr
		);

		if (fileHandle == INVALID_HANDLE_VALUE || fileHandle == nullptr)
		{
			MSG_BOX("Failed To Open Height Map File");
			return E_FAIL;
		}

		Bool isRead{};
		BITMAPFILEHEADER fh{};
		isRead = ReadFile(fileHandle, &fh, sizeof(BITMAPFILEHEADER), &byte, nullptr);
		if (fileHandle != nullptr && fileHandle == INVALID_HANDLE_VALUE && isRead)
		{
			MSG_BOX("Failed To Read Header Height Map File");
			return E_FAIL;
		}

		BITMAPINFOHEADER ih{};
		isRead = ReadFile(fileHandle, &ih, sizeof(BITMAPINFOHEADER), &byte, nullptr);
		if (fileHandle != nullptr && fileHandle == INVALID_HANDLE_VALUE && isRead)
		{
			MSG_BOX("Failed To Read Info Height Map File");
			return E_FAIL;
		}

		m_NumVerticesX = ih.biWidth;
		m_NumVerticesZ = ih.biHeight;
		m_NumVertices = m_NumVerticesX * m_NumVerticesZ;

		pixels = new uint32_t[m_NumVertices];

		isRead = ReadFile(fileHandle, pixels, sizeof(uint32_t) * m_NumVertices, &byte, nullptr);
		if (fileHandle != nullptr && fileHandle == INVALID_HANDLE_VALUE && isRead)
		{
			delete[] pixels;
			MSG_BOX("Failed To Read Data Height Map File");
			return E_FAIL;
		}
	}
	else
	{
		m_NumVerticesX = 129;
		m_NumVerticesZ = 129;
		m_NumVertices = m_NumVerticesX * m_NumVerticesZ;
		pixels = new uint32_t[m_NumVertices];
		memset(pixels, 0, sizeof(uint32_t) * m_NumVertices);
	}
	

	m_NumVtxBuffers = 1;
	m_VtxStride = sizeof(VTXNORMTEX);
	m_NumIndices = (m_NumVerticesX - 1) * (m_NumVerticesZ - 1) * 2 * 3;
	m_IndexStride = 4;
	m_PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	D3D11_BUFFER_DESC vtxBufferDesc{};
	vtxBufferDesc.ByteWidth = m_VtxStride * m_NumVertices;
	vtxBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vtxBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vtxBufferDesc.StructureByteStride = m_VtxStride;
	vtxBufferDesc.CPUAccessFlags = 0;
	vtxBufferDesc.MiscFlags = 0;

	VTXNORMTEX* vertices = new VTXNORMTEX[m_NumVertices];

	for (uint32_t i = 0; i < m_NumVerticesZ; ++i)
	{
		for (uint32_t j = 0; j < m_NumVerticesX; ++j)
		{
			uint32_t index = i * m_NumVerticesX + j;

			vertices[index].position = Vector3{ 
				static_cast<Float>(j), 
				static_cast<Float>(pixels[index] & 0x000000ff) / 10.f, 
				static_cast<Float>(i) 
			};
			vertices[index].normal = Vector3::Zero;
			vertices[index].texcoord = Vector2{
				static_cast<Float>(j) / (m_NumVerticesX - 1.f), 
				static_cast<Float>(i) / (m_NumVerticesZ - 1.f) 
			};
		}
	}

	D3D11_BUFFER_DESC IdxBufferDesc{};
	IdxBufferDesc.ByteWidth = m_IndexStride * m_NumIndices;
	IdxBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	IdxBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	IdxBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	IdxBufferDesc.StructureByteStride = m_IndexStride;
	IdxBufferDesc.CPUAccessFlags = 0;
	IdxBufferDesc.MiscFlags = 0;

	uint32_t* indices = new uint32_t[m_NumIndices];
	uint32_t numIndices = { 0 };

	for (size_t i = 0; i < m_NumVerticesZ - 1; ++i)
	{
		for (size_t j = 0; j < m_NumVerticesX - 1; ++j)
		{
			uint32_t index = i * m_NumVerticesX + j;

			uint32_t indexPoint[4] = {
				index + m_NumVerticesX, 
				index + m_NumVerticesX + 1, 
				index + 1, 
				index
			};

			Vector3 sour{}, dest{}, normal{};

			indices[numIndices++] = indexPoint[0];
			indices[numIndices++] = indexPoint[1];
			indices[numIndices++] = indexPoint[2];
			
			sour = vertices[indexPoint[1]].position - vertices[indexPoint[0]].position;
			dest = vertices[indexPoint[2]].position - vertices[indexPoint[1]].position;
			dest.Cross(sour, dest); dest.Normalize(dest);
			normal = dest;
			
			vertices[indexPoint[0]].normal += normal;
			vertices[indexPoint[1]].normal += normal;
			vertices[indexPoint[2]].normal += normal;

			indices[numIndices++] = indexPoint[0];
			indices[numIndices++] = indexPoint[2];
			indices[numIndices++] = indexPoint[3];

			sour = vertices[indexPoint[2]].position - vertices[indexPoint[0]].position;
			dest = vertices[indexPoint[3]].position - vertices[indexPoint[2]].position;
			dest.Cross(sour, dest); dest.Normalize(dest);
			normal = dest;

			vertices[indexPoint[0]].normal += normal;
			vertices[indexPoint[2]].normal += normal;
			vertices[indexPoint[3]].normal += normal;
		}	
	}

	for (uint32_t i = 0; i < m_NumVertices; ++i)
	{
		vertices[i].normal.Normalize();
	}

	D3D11_SUBRESOURCE_DATA vtxInitialData{};
	vtxInitialData.pSysMem = vertices;
	if (FAILED(m_Device->CreateBuffer(&vtxBufferDesc, &vtxInitialData, m_VB.GetAddressOf())))
	{
		delete[] indices;
		delete[] vertices;
		delete[] pixels;

		LOG_ERROR(L"Failed To Create Vertex Buffer At TerrainBuffer");
		MSG_BOX("Failed To Create Vertex Buffer");
		return E_FAIL;
	}

	D3D11_SUBRESOURCE_DATA idxInitialData{};
	idxInitialData.pSysMem = indices;
	if (FAILED(m_Device->CreateBuffer(&IdxBufferDesc, &idxInitialData, m_IB.GetAddressOf())))
	{
		delete[] indices;
		delete[] vertices;
		delete[] pixels;

		LOG_ERROR(L"Failed To Create Index Buffer At TerrainBuffer");
		MSG_BOX("Failed To Create Index Buffer");
		return E_FAIL;
	}

	delete[] indices;
	delete[] vertices;
	delete[] pixels;

	if (fileHandle != nullptr && fileHandle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(fileHandle);
	}

	return VIBuffer::Initialize_Prototype();
}

HRESULT VIBuffer_Terrain::Initialize(void* arg)
{
	return VIBuffer::Initialize(arg);
}

Shared<VIBuffer_Terrain> VIBuffer_Terrain::CreatePrototype()
{
	auto bufferTerrain = make_shared<VIBuffer_Terrain>(
		GAME_INSTANCE->Get_Device(), GAME_INSTANCE->Get_Context());

	if (FAILED(bufferTerrain->Initialize_Prototype()))
	{
		MSG_BOX("Failed To Create VIBuffer_Terrain");
		return nullptr;
	}

	return bufferTerrain;
}

Shared<VIBuffer_Terrain> VIBuffer_Terrain::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context, const tChar* heightMapFilePath)
{
	auto bufferTerrain = make_shared<VIBuffer_Terrain>(device, context);

	if (FAILED(bufferTerrain->Initialize_Prototype(heightMapFilePath)))
	{
		MSG_BOX("Failed To Create VIBuffer_Terrain");
		return nullptr;
	}

	return bufferTerrain;
}

Shared<Component> VIBuffer_Terrain::Clone(void* arg)
{
	auto bufferTerrain = make_shared<VIBuffer_Terrain>(*this);

	if (FAILED(bufferTerrain->Initialize(arg)))
	{
		MSG_BOX("Failed To Clone VIBuffer_Terrain");
		return nullptr;
	}

	return bufferTerrain;
}
