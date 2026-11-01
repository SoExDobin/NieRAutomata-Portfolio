#include "VIBuffer_Particle_Point.h"

#include "Game.h"
#include "Random_Helper.h"

VIBuffer_Particle_Point::VIBuffer_Particle_Point() : VIBuffer_Instance{} {}
VIBuffer_Particle_Point::VIBuffer_Particle_Point(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: VIBuffer_Instance{device, context} {}
VIBuffer_Particle_Point::VIBuffer_Particle_Point(const VIBuffer_Particle_Point& rhs)
	: VIBuffer_Instance{ rhs } {}

HRESULT VIBuffer_Particle_Point::Initialize_Prototype()
{
	if (FAILED(VIBuffer_Instance::Initialize_Prototype()))
		return E_FAIL;
	
	m_NumVertices = 1;
	m_NumVtxBuffers = 2;
	m_VtxStride = sizeof(VTXPOS);
	m_PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
	
	D3D11_BUFFER_DESC vtxBufferDesc{};
	vtxBufferDesc.ByteWidth = m_VtxStride * m_NumVertices;
	vtxBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vtxBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vtxBufferDesc.StructureByteStride = m_VtxStride;
	vtxBufferDesc.CPUAccessFlags = 0;
	vtxBufferDesc.MiscFlags = 0;

	VTXPOS* vertices = new VTXPOS[m_NumVertices];
	ZeroMemory(vertices, sizeof(VTXPOS) * m_NumVertices);

	D3D11_SUBRESOURCE_DATA vtxInitialData{};
	vtxInitialData.pSysMem = vertices;

	HRESULT hr = m_Device->CreateBuffer(&vtxBufferDesc, &vtxInitialData, m_VB.GetAddressOf());
	delete[] vertices;

	return hr;
}

HRESULT VIBuffer_Particle_Point::Initialize(void* arg)
{
	if (nullptr == arg) return E_FAIL;

	auto desc = static_cast<VIBUFFER_INSTANCE_POINT_DESC*>(arg);
	m_IsLoop = desc->isLoop;
	m_Pivot = desc->pivot;
	m_NumInstances = desc->numInstances;
	m_InstanceStride = sizeof(VTXPARTICLE_INSTANCE);

	D3D11_BUFFER_DESC instanceBufferDesc{};
	instanceBufferDesc.ByteWidth = m_InstanceStride * m_NumInstances;
	instanceBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	instanceBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	instanceBufferDesc.StructureByteStride = m_InstanceStride;
	instanceBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	instanceBufferDesc.MiscFlags = 0;

	m_InitialVertices.resize(m_NumInstances);
	m_CurrentVertices.resize(m_NumInstances);
	m_SpreadDirections.resize(m_NumInstances);
	m_Speeds.resize(m_NumInstances);

	for (uint32 i = 0; i < m_NumInstances; ++i)
	{
		Float scale = static_cast<Float>(Helper::Random_Double(desc->scale.x, desc->scale.y));
		m_Speeds[i] = static_cast<Float>(Helper::Random_Double(desc->speed.x, desc->speed.y));
		Vector3 direction{
			static_cast<Float>(Helper::Random_Double(-1.0, 1.0)),
			static_cast<Float>(Helper::Random_Double(-0.3, 1.0)),
			static_cast<Float>(Helper::Random_Double(-1.0, 1.0)) };
		if (direction.LengthSquared() < 0.0001f) direction = Vector3::UnitY;
		direction.Normalize();
		m_SpreadDirections[i] = direction;

		m_InitialVertices[i].right = Vector4{scale, 0.f, 0.f, 0.f};
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
	m_CurrentVertices = m_InitialVertices;

	D3D11_SUBRESOURCE_DATA	instanceInitialData{};
	instanceInitialData.pSysMem = m_InitialVertices.data();

	if (FAILED(m_Device->CreateBuffer(&instanceBufferDesc, &instanceInitialData, m_VBInstance.GetAddressOf())))
		return E_FAIL;

	return VIBuffer_Instance::Initialize(arg);
}

HRESULT VIBuffer_Particle_Point::Bind_Resources()
{
	ID3D11Buffer* buffer[] = { m_VB.Get(), m_VBInstance.Get() };
	uint32 strides[] = { m_VtxStride, m_InstanceStride };
	uint32 offsets[] = { 0, 0 }; 

	m_Context->IASetVertexBuffers(0, m_NumVtxBuffers, buffer, strides, offsets);
	m_Context->IASetPrimitiveTopology(m_PrimitiveType);

	return S_OK;
}

HRESULT VIBuffer_Particle_Point::Render()
{
	return Render(m_NumInstances);
}

HRESULT VIBuffer_Particle_Point::Render(uint32 numInstances)
{
	numInstances = std::min(numInstances, m_NumInstances);
	m_Context->DrawInstanced(1, numInstances, 0, 0);
	Count_Draw(1, numInstances);
	return S_OK;
}

void VIBuffer_Particle_Point::On_Destroy()
{
	m_VBInstance.Reset();
	m_InitialVertices.clear();
	m_CurrentVertices.clear();
	m_SpreadDirections.clear();
	m_Speeds.clear();
	VIBuffer_Instance::On_Destroy();
}

void VIBuffer_Particle_Point::Update_Drop(Float timeDelta)
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

void VIBuffer_Particle_Point::Update_Spread(Float timeDelta)
{
	for (uint32 i = 0; i < m_NumInstances; ++i)
	{
		auto& particle = m_CurrentVertices[i];
		particle.lifeTime.y += timeDelta;
		if (m_IsLoop && particle.lifeTime.y >= particle.lifeTime.x)
		{
			particle = m_InitialVertices[i];
		}
		if (particle.lifeTime.y < particle.lifeTime.x)
		{
			const Vector3 displacement = m_SpreadDirections[i] * m_Speeds[i] * timeDelta;
			particle.translation.x += displacement.x;
			particle.translation.y += displacement.y;
			particle.translation.z += displacement.z;
		}
	}
	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (FAILED(m_Context->Map(m_VBInstance.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;
	memcpy(mapped.pData, m_CurrentVertices.data(), m_InstanceStride * m_NumInstances);
	m_Context->Unmap(m_VBInstance.Get(), 0);
}

Shared<VIBuffer_Particle_Point> VIBuffer_Particle_Point::CreatePrototype()
{
	auto prototype = make_shared<VIBuffer_Particle_Point>(GAME_INSTANCE->Get_Device(), GAME_INSTANCE->Get_Context());

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : VIBuffer_Particle_Point");
		return nullptr;
	}

	return prototype;
}

Shared<Component> VIBuffer_Particle_Point::Clone(void* arg)
{
	auto instance = make_shared<VIBuffer_Particle_Point>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : VIBuffer_Particle_Point");
		return nullptr;
	}

	return instance;
}


