#include "pch.h"
#include "ExplodeEffect.h"
#include "VIBuffer_Particle_Point.h"
#include <Game.h>
#include "Random_Helper.h"
#include "SpdLogger.h"

ExplodeEffect::ExplodeEffect() : ParticleEffect{} {}
ExplodeEffect::ExplodeEffect(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: ParticleEffect{ device, context } {}
ExplodeEffect::ExplodeEffect(const ExplodeEffect& rhs)
	: ParticleEffect{ rhs } {}

HRESULT ExplodeEffect::Initialize_Prototype()
{
	return ParticleEffect::Initialize_Prototype();
}

HRESULT ExplodeEffect::Initialize(void* arg)
{
	if (nullptr == arg)
	{
		LOG_ERROR(L"There is no ExplodeEffect Desc");
		return E_FAIL;
	}

	if (FAILED(ParticleEffect::Initialize(arg)))
		return E_FAIL;

	const EXPLODE_EFFECT_DESC& desc = *static_cast<EXPLODE_EFFECT_DESC*>(arg);
	if (FAILED(Ready_Components(desc)))
	{
		LOG_ERROR(L"Failed to Ready_Components ExplodeEffect");
		return E_FAIL;
	}

	return S_OK;
}

void ExplodeEffect::Update(Float timeDelta)
{
	m_Time += timeDelta;

	if (m_Time * m_FramesPerSecond >= static_cast<Float>(m_FrameCount))
	{
		GAME_INSTANCE->Destroy(Get_ObjectGuid());
		return;
	}

	m_Transform->Update_WorldMatrix();
}

HRESULT ExplodeEffect::Render()
{
	if (FAILED(Bind_ShaderResources()))
		return E_FAIL;

	if (FAILED(m_Shader->Begin("Explode")))
		return E_FAIL;

	if (FAILED(m_Buffer->Bind_Resources()))
		return E_FAIL;

	return m_Buffer->Render();
}

void ExplodeEffect::Submit_RenderGroup()
{
	GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::BLEND, shared_from_this());
}

HRESULT ExplodeEffect::Ready_Components(const EXPLODE_EFFECT_DESC& desc)
{
	Shader::SHADER_DESC shaderDesc{ VTXPARTICLE_POINT_DESC::Tag, VTXPARTICLE_POINT_DESC::Elements, VTXPARTICLE_POINT_DESC::numElements };
	m_Shader = Add_Component<Shader>(ETOI(LEVEL::GAMEPLAY), &shaderDesc);
	if (nullptr == m_Shader)
		return E_FAIL;

	// A single still billboard; its life never advances, so the shader's lifetime fade stays at 1.
	VIBuffer_Particle_Point::VIBUFFER_INSTANCE_POINT_DESC instanceDesc{};
	instanceDesc.numInstances = 1;
	instanceDesc.isLoop = false;
	instanceDesc.center = Vector3::Zero;
	instanceDesc.pivot = Vector3::Zero;
	instanceDesc.range = Vector3::Zero;
	instanceDesc.scale = Vector2{ desc.size, desc.size };
	instanceDesc.speed = Vector2::Zero;
	instanceDesc.lifeTime = Vector2{ 1.f, 1.f };
	m_Buffer = Add_Component<VIBuffer_Particle_Point>(ETOI(LEVEL::GAMEPLAY), &instanceDesc);
	if (nullptr == m_Buffer)
		return E_FAIL;

	// Texture/Effect/Explode: Explode_1 (fire only), Explode_2/3 (fire into smoke). Amusment and Land sets are not used.
	const wstring textureTag = desc.textureTag.empty() ? L"Effect_Explode_" + to_wstring(Helper::Random_Int(1, 3)) : desc.textureTag;
	auto textureDesc = Texture::TEXTURE_DESC{ Get_LevelIndex(), textureTag };
	m_Texture = Add_Component<Texture>(ETOI(LEVEL::GAMEPLAY), &textureDesc);
	if (nullptr == m_Texture)
		return E_FAIL;
	m_FrameCount = m_Texture->Get_TextureNumSRVs();
	if (0 == m_FrameCount)
		return E_FAIL;

	m_Tint = desc.tint;
	m_Upright = desc.upright;
	m_FramesPerSecond = desc.framesPerSecond;
	m_FadeStart = desc.fadeStart;
	m_Transform->Set_Position(desc.position);
	m_Transform->Update_WorldMatrix();

	return S_OK;
}

HRESULT ExplodeEffect::Bind_ShaderResources()
{
	const uint32 isLocked = m_Upright ? 1 : 0;
	const uint32 frame = std::min(static_cast<uint32>(m_Time * m_FramesPerSecond), m_FrameCount - 1);
	const Float progress = static_cast<Float>(frame) / static_cast<Float>(m_FrameCount);
	const Float fade = 1.f - std::clamp((progress - m_FadeStart) / (1.f - m_FadeStart), 0.f, 1.f);

	if (FAILED(m_Transform->Bind_ShaderResource(m_Shader, WorldMatrix)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ViewMatrix, D3DTS::VIEW)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ProjMatrix, D3DTS::PROJ)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Bind_CameraPosition(m_Shader, CameraPosition)))
		return E_FAIL;
	if (FAILED(m_Shader->Bind_RawValue("g_LockUpRight", &isLocked, sizeof(uint32))))
		return E_FAIL;
	if (FAILED(m_Shader->Bind_RawValue("g_FadeAlpha", &fade, sizeof(Float))))
		return E_FAIL;
	if (FAILED(m_Shader->Bind_RawValue("g_ExplodeTint", &m_Tint, sizeof(Vector4))))
		return E_FAIL;

	return m_Texture->Bind_ShaderResourceView(m_Shader, DiffuseMap, frame);
}

Shared<ExplodeEffect> ExplodeEffect::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<ExplodeEffect>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : ExplodeEffect");
		return nullptr;
	}

	return prototype;
}

Shared<GameObject> ExplodeEffect::Clone(void* arg)
{
	auto instance = make_shared<ExplodeEffect>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : ExplodeEffect");
		return nullptr;
	}

	return instance;
}
