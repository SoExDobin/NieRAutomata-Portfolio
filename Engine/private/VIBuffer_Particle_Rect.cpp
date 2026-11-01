#include "VIBuffer_Particle_Rect.h"

#include "Game.h"
#include "Random_Helper.h"
#include "SpdLogger.h"

VIBuffer_Particle_Rect::VIBuffer_Particle_Rect() : VIBuffer_Instance{} {}
VIBuffer_Particle_Rect::VIBuffer_Particle_Rect(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: VIBuffer_Instance{device, context} {}
VIBuffer_Particle_Rect::VIBuffer_Particle_Rect(const VIBuffer_Particle_Rect& rhs)
	: VIBuffer_Instance{ rhs } {}

HRESULT VIBuffer_Particle_Rect::Initialize_Prototype()
{
	if (FAILED(VIBuffer_Instance::Initialize_Prototype()))
	{
		LOG_ERROR(L"Failed to Initialize Prototype VIBuffer_Particle_Rect");
		return E_FAIL;
	}

	m_NumVtxBuffers = 2;
	m_NumVertices = 4;
	m_VtxStride = sizeof(VTXTEX);
	m_NumIndices = 6;
	m_IndexStride = 2;
	m_PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	VTXTEX* vertices = new VTXTEX[m_NumVertices];
	vertices[0].position = Float3(-0.5f, 0.5f, 0.f);
	vertices[0].texcoord = Float2(0.f, 0.f);
	vertices[1].position = Float3(0.5f, 0.5f, 0.f);
	vertices[1].texcoord = Float2(1.f, 0.f);
	vertices[2].position = Float3(0.5f, -0.5f, 0.f);
	vertices[2].texcoord = Float2(1.f, 1.f);
	vertices[3].position = Float3(-0.5f, -0.5f, 0.f);
	vertices[3].texcoord = Float2(0.f, 1.f);

	D3D11_BUFFER_DESC vtxDesc{};
	vtxDesc.ByteWidth = m_VtxStride * m_NumVertices;
	vtxDesc.Usage = D3D11_USAGE_DEFAULT;
	vtxDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vtxDesc.StructureByteStride = m_VtxStride;

	D3D11_SUBRESOURCE_DATA vtxData{ vertices };
	if (FAILED(m_Device->CreateBuffer(&vtxDesc, &vtxData, m_VB.GetAddressOf())))
		return E_FAIL;

	uShort* indices = new uShort[m_NumIndices];
	indices[0] = 0; indices[1] = 1; indices[2] = 2;
	indices[3] = 0; indices[4] = 2; indices[5] = 3;

	D3D11_BUFFER_DESC indexDesc{};
	indexDesc.ByteWidth = m_IndexStride * m_NumIndices;
	indexDesc.Usage = D3D11_USAGE_DEFAULT;
	indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexDesc.StructureByteStride = m_IndexStride;
	D3D11_SUBRESOURCE_DATA indexData{ indices };
	if (FAILED(m_Device->CreateBuffer(&indexDesc, &indexData, m_IB.GetAddressOf())))
		return E_FAIL;

	delete[] vertices;
	delete[] indices;

	return S_OK;
}

HRESULT VIBuffer_Particle_Rect::Initialize(void* arg)
{
	if (nullptr == arg) return E_FAIL;

	auto desc = static_cast<VIBUFFER_INSTANCE_RECT_DESC*>(arg);
	m_IsLoop = desc->isLoop;
	m_Pivot = desc->pivot;
	m_NumInstances = desc->numInstances;
	m_IndexCountPerInstance = 6;
	m_InstanceStride = sizeof(VTXPARTICLE_INSTANCE);

	// Instance
	D3D11_BUFFER_DESC instanceBufferDesc{};
	instanceBufferDesc.ByteWidth = m_InstanceStride * m_NumInstances;
	instanceBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	instanceBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	instanceBufferDesc.StructureByteStride = m_InstanceStride;
	instanceBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	instanceBufferDesc.MiscFlags = 0;

	m_InitialVertices.resize(m_NumInstances);
	m_Speeds.resize(m_NumInstances);

	for (uint32 i = 0; i < m_NumInstances; ++i)
	{
		Float scale = static_cast<Float>(Helper::Random_Double(desc->scale.x, desc->scale.y));
		m_Speeds[i] = static_cast<Float>(Helper::Random_Double(desc->speed.x, desc->speed.y));

		m_InitialVertices[i].right = Vector4{ scale, 0.f, 0.f, 0.f };
		m_InitialVertices[i].up = Vector4{ 0.f, scale, 0.f, 0.f };
		m_InitialVertices[i].look = Vector4{ 0.f, 0.f, scale, 0.f };
		m_InitialVertices[i].translation = Vector4{
			static_cast<Float>(Helper::Random_Double(desc->center.x - desc->range.x * 0.5f, desc->center.x + desc->range.x * 0.5f)),
			static_cast<Float>(Helper::Random_Double(desc->center.y - desc->range.y * 0.5f, desc->center.y + desc->range.y * 0.5f)),
			static_cast<Float>(Helper::Random_Double(desc->center.z - desc->range.z * 0.5f, desc->center.z + desc->range.z * 0.5f)),
			1.f,
		};

		m_InitialVertices[i].lifeTime = Vector2{ static_cast<Float>(Helper::Random_Double(desc->lifeTime.x, desc->lifeTime.y)), 0.f };
	}

	D3D11_SUBRESOURCE_DATA	instanceInitialData{};
	instanceInitialData.pSysMem = m_InitialVertices.data();

	if (FAILED(m_Device->CreateBuffer(&instanceBufferDesc, &instanceInitialData, m_VBInstance.GetAddressOf())))
		return E_FAIL;

	return VIBuffer_Instance::Initialize(arg);
}

HRESULT VIBuffer_Particle_Rect::Bind_Resources()
{
	ID3D11Buffer* buffer[] = { m_VB.Get(), m_VBInstance.Get() };
	uint32 strides[] = { m_VtxStride, m_InstanceStride };
	uint32 offsets[] = { 0, 0 };

	m_Context->IASetVertexBuffers(0, m_NumVtxBuffers, buffer, strides, offsets);
	m_Context->IASetIndexBuffer(m_IB.Get(), 2 == m_IndexStride ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0);
	m_Context->IASetPrimitiveTopology(m_PrimitiveType);

	return S_OK;
}

HRESULT VIBuffer_Particle_Rect::Render()
{
	m_Context->DrawIndexedInstanced(m_IndexCountPerInstance, m_NumInstances, 0, 0, 0);
	Count_Draw(m_IndexCountPerInstance, m_NumInstances);
	return S_OK;
}

void VIBuffer_Particle_Rect::On_Destroy()
{
	m_VBInstance.Reset();
	m_InitialVertices.clear();
	m_Speeds.clear();
	VIBuffer_Instance::On_Destroy();
}

void VIBuffer_Particle_Rect::Update_Drop(Float timeDelta)
{
	D3D11_MAPPED_SUBRESOURCE mapped{};
	m_Context->Map(m_VBInstance.Get(), 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mapped);
	auto* vtxMapped = static_cast<VTXPARTICLE_INSTANCE*>(mapped.pData);

	for (uint32 i = 0; i < m_NumInstances; ++i)
	{
		vtxMapped[i].translation.y -= m_Speeds[i] * timeDelta;
		vtxMapped[i].lifeTime.y += timeDelta;

		if (m_IsLoop && vtxMapped[i].lifeTime.y >= vtxMapped[i].lifeTime.x)
		{
			vtxMapped[i].lifeTime.y = 0.f;
			vtxMapped[i].translation = m_InitialVertices[i].translation;
		}
	}

	m_Context->Unmap(m_VBInstance.Get(), 0);
}

void VIBuffer_Particle_Rect::Update_Spread(Float timeDelta)
{
	D3D11_MAPPED_SUBRESOURCE mapped{};
	m_Context->Map(m_VBInstance.Get(), 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mapped);
	auto* vtxMapped = static_cast<VTXPARTICLE_INSTANCE*>(mapped.pData);

	for (uint32 i = 0; i < m_NumInstances; ++i)
	{
		Vector4 dir = XMVectorSetW(vtxMapped[i].translation - m_Pivot, 0.f);

		vtxMapped[i].translation += dir * m_Speeds[i] * timeDelta;
		if (m_IsLoop && vtxMapped[i].lifeTime.y >= vtxMapped[i].lifeTime.x)
		{
			vtxMapped[i].lifeTime.y = 0.f;
			vtxMapped[i].translation = m_InitialVertices[i].translation;
		}
	}

	m_Context->Unmap(m_VBInstance.Get(), 0);
}

Shared<VIBuffer_Particle_Rect> VIBuffer_Particle_Rect::CreatePrototype()
{
	auto prototype = make_shared<VIBuffer_Particle_Rect>(GAME_INSTANCE->Get_Device(), GAME_INSTANCE->Get_Context());

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : VIBuffer_Particle_Rect");
		return nullptr;
	}

	return prototype;
}

Shared<Component> VIBuffer_Particle_Rect::Clone(void* arg)
{
	auto instance = make_shared<VIBuffer_Particle_Rect>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : VIBuffer_Particle_Rect");
		return nullptr;
	}

	return instance;
}


