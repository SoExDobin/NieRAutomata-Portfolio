#include "VIBuffer_Rect.h"
#include "Game.h"

VIBuffer_Rect::VIBuffer_Rect()
	: VIBuffer{} {}

VIBuffer_Rect::VIBuffer_Rect(const ComPtr<ID3D11Device> &device, const ComPtr<ID3D11DeviceContext> &context)
    : VIBuffer{device, context} {}

VIBuffer_Rect::VIBuffer_Rect(const VIBuffer_Rect& rhs) : VIBuffer{rhs} {}

HRESULT VIBuffer_Rect::Initialize_Prototype() {
  m_NumVtxBuffers = 1;
  m_NumVertices = 4;
  m_VtxStride = sizeof(VTXTEX);
  m_NumIndices = 6;
  m_IndexStride = 2;
  m_PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

  D3D11_BUFFER_DESC vtxBufferDesc = {};
  vtxBufferDesc.ByteWidth = m_VtxStride * m_NumVertices;
  vtxBufferDesc.Usage = D3D11_USAGE_DEFAULT;
  vtxBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  vtxBufferDesc.StructureByteStride = m_VtxStride;
  vtxBufferDesc.CPUAccessFlags = 0;
  vtxBufferDesc.MiscFlags = 0;

  VTXTEX *vertices = new VTXTEX[m_NumVertices];
  vertices[0].position = Float3(-0.5f, 0.5f, 0.f);
  vertices[0].texcoord = Float2(0.f, 0.f);

  vertices[1].position = Float3(0.5f, 0.5f, 0.f);
  vertices[1].texcoord = Float2(1.f, 0.f);

  vertices[2].position = Float3(0.5f, -0.5f, 0.f);
  vertices[2].texcoord = Float2(1.f, 1.f);

  vertices[3].position = Float3(-0.5f, -0.5f, 0.f);
  vertices[3].texcoord = Float2(0.f, 1.f);

  D3D11_SUBRESOURCE_DATA vtxInitialData = {};
  vtxInitialData.pSysMem = vertices;

  if (FAILED(m_Device->CreateBuffer(&vtxBufferDesc, &vtxInitialData,
                                    m_VB.GetAddressOf())))
    return E_FAIL;

  D3D11_BUFFER_DESC indexBufferDesc = {};
  indexBufferDesc.ByteWidth = m_IndexStride * m_NumIndices;
  indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
  indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
  indexBufferDesc.StructureByteStride = m_IndexStride;
  indexBufferDesc.CPUAccessFlags = 0;
  indexBufferDesc.MiscFlags = 0;

  uShort *indices = new uShort[m_NumIndices];
  indices[0] = 0;
  indices[1] = 1;
  indices[2] = 2;

  indices[3] = 0;
  indices[4] = 2;
  indices[5] = 3;

  D3D11_SUBRESOURCE_DATA indexInitialData = {};
  indexInitialData.pSysMem = indices;

  if (FAILED(m_Device->CreateBuffer(&indexBufferDesc, &indexInitialData,
                                    m_IB.GetAddressOf())))
    return E_FAIL;

  delete[] vertices;
  delete[] indices;

  return VIBuffer::Initialize_Prototype();
}

HRESULT VIBuffer_Rect::Initialize(void *arg) {
  return VIBuffer::Initialize(arg);
}

Shared<VIBuffer_Rect> VIBuffer_Rect::CreatePrototype()
{
    auto bufferRect = make_shared<VIBuffer_Rect>(
        GAME_INSTANCE->Get_Device(), GAME_INSTANCE->Get_Context());

    if (FAILED(bufferRect->Initialize_Prototype())) {
        MSG_BOX("Failed to Created : CVIBuffer_Rect");
    }
    return bufferRect;
}

Shared<VIBuffer_Rect>
VIBuffer_Rect::Create(const ComPtr<ID3D11Device> &device, const ComPtr<ID3D11DeviceContext> &context) {
    auto bufferRect = make_shared<VIBuffer_Rect>(device, context);

    if (FAILED(bufferRect->Initialize_Prototype())) {
        MSG_BOX("Failed to Created : CVIBuffer_Rect");
    }
    return bufferRect;
}

Shared<Component> VIBuffer_Rect::Clone(void *arg) {
  auto bufferRect = make_shared<VIBuffer_Rect>(*this);

  if (FAILED(bufferRect->Initialize(arg))) {
    MSG_BOX("Failed to Cloned : CVIBuffer_Rect");
  }

  return bufferRect;
}
