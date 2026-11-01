#include "pch.h"
#include "ConfettiEffect.h"
#include "VIBuffer_Particle_Point.h"
#include <Game.h>
#include "Random_Helper.h"
#include "SpdLogger.h"

ConfettiEffect::ConfettiEffect() : ParticleEffect{} {}
ConfettiEffect::ConfettiEffect(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: ParticleEffect{ device, context } {}
ConfettiEffect::ConfettiEffect(const ConfettiEffect& rhs)
	: ParticleEffect{ rhs } {}

HRESULT ConfettiEffect::Initialize_Prototype()
{
	return ParticleEffect::Initialize_Prototype();
}

HRESULT ConfettiEffect::Initialize(void* arg)
{
	if (nullptr == arg)
	{
		LOG_ERROR(L"There is no ConfettiEffect Desc");
		return E_FAIL;
	}

	if (FAILED(ParticleEffect::Initialize(arg)))
		return E_FAIL;

	const CONFETTI_EFFECT_DESC& desc = *static_cast<CONFETTI_EFFECT_DESC*>(arg);
	if (FAILED(Ready_Components(desc)))
	{
		LOG_ERROR(L"Failed to Ready_Components ConfettiEffect");
		return E_FAIL;
	}

	return S_OK;
}

void ConfettiEffect::Update(Float timeDelta)
{
	m_Burst.w += timeDelta;
	if (m_Burst.w >= Life)
		GAME_INSTANCE->Destroy(Get_ObjectGuid());
}

void ConfettiEffect::Submit_RenderGroup()
{
	GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::BLEND, shared_from_this());
}

HRESULT ConfettiEffect::Render()
{
	if (FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ViewMatrix, D3DTS::VIEW)) ||
		FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ProjMatrix, D3DTS::PROJ)) ||
		FAILED(GAME_INSTANCE->Bind_CameraPosition(m_Shader, CameraPosition)) ||
		FAILED(m_Shader->Bind_RawValue("g_ConfettiBurst", &m_Burst, sizeof(Vector4))) ||
		FAILED(m_Shader->Bind_RawValue("g_ConfettiAim", &m_Aim, sizeof(Vector4))) ||
		FAILED(m_Shader->Bind_RawValue("g_ConfettiFloor", &m_FloorHeight, sizeof(Float))) ||
		FAILED(m_Shader->Bind_RawValue("g_ConfettiCount", &m_Count, sizeof(uint32))) ||
		FAILED(m_Texture->Bind_ShaderResourceView(m_Shader, DiffuseMap, 0)) ||
		FAILED(m_Shader->Begin("Confetti")) ||
		FAILED(m_Buffer->Bind_Resources()))
		return E_FAIL;

	return m_Buffer->Render();
}

HRESULT ConfettiEffect::Ready_Components(const CONFETTI_EFFECT_DESC& desc)
{
	Shader::SHADER_DESC shaderDesc{ VTXPARTICLE_POINT_DESC::Tag, VTXPARTICLE_POINT_DESC::Elements, VTXPARTICLE_POINT_DESC::numElements };
	m_Shader = Add_Component<Shader>(ETOI(LEVEL::GAMEPLAY), &shaderDesc);
	if (nullptr == m_Shader)
		return E_FAIL;

	// One point per piece; the shader places each from its instance index.
	VIBuffer_Particle_Point::VIBUFFER_INSTANCE_POINT_DESC instanceDesc{};
	instanceDesc.numInstances = MaxPieces;
	instanceDesc.scale = Vector2{ 1.f, 1.f };
	instanceDesc.lifeTime = Vector2{ 1.f, 1.f };
	m_Buffer = Add_Component<VIBuffer_Particle_Point>(ETOI(LEVEL::GAMEPLAY), &instanceDesc);
	if (nullptr == m_Buffer)
		return E_FAIL;

	// Texture/Effect/Confetti/Confetti.png: data010 texture 125's 30 frames of 256x64 stacked into one 256x1920 atlas.
	auto textureDesc = Texture::TEXTURE_DESC{ Get_LevelIndex(), L"Effect_Confetti" };
	m_Texture = Add_Component<Texture>(ETOI(LEVEL::GAMEPLAY), &textureDesc);
	if (nullptr == m_Texture)
		return E_FAIL;

	Vector3 aim = desc.throwDirection;
	if (aim.LengthSquared() < 0.0001f)
		aim = Vector3::UnitY;
	aim.Normalize();
	m_Burst = Vector4{ desc.position.x, desc.position.y, desc.position.z, 0.f };
	m_Aim = Vector4{ aim.x, aim.y, aim.z, static_cast<Float>(Helper::Random_Double(0.0, 100.0)) };
	m_FloorHeight = desc.floorHeight;
	m_Count = (std::min)(desc.count, MaxPieces);
	m_Transform->Set_Position(desc.position);
	m_Transform->Update_WorldMatrix();

	return S_OK;
}

Shared<ConfettiEffect> ConfettiEffect::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<ConfettiEffect>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : ConfettiEffect");
		return nullptr;
	}

	return prototype;
}

Shared<GameObject> ConfettiEffect::Clone(void* arg)
{
	auto instance = make_shared<ConfettiEffect>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : ConfettiEffect");
		return nullptr;
	}

	return instance;
}
