#include "pch.h"
#include "FireFlashEffect.h"
#include "VIBuffer_Particle_Point.h"
#include <Game.h>
#include "Random_Helper.h"
#include "SpdLogger.h"

FireFlashEffect::FireFlashEffect() : ParticleEffect{} {}
FireFlashEffect::FireFlashEffect(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: ParticleEffect{device, context} {}
FireFlashEffect::FireFlashEffect(const FireFlashEffect& rhs)
	: ParticleEffect{rhs} {}

HRESULT FireFlashEffect::Initialize_Prototype()
{
	if (FAILED(ParticleEffect::Initialize_Prototype()))
	{
		LOG_ERROR(L"Failed to Initialize Proto ParticleEffect");
		return E_FAIL;
	}

	return S_OK;
}

HRESULT FireFlashEffect::Initialize(void* arg)
{
	// No desc field is read, so the Editor's Spawn (arg nullptr) clones it like Pl0000's Add_Child does.
	if (FAILED(ParticleEffect::Initialize(arg)))
	{
		LOG_ERROR(L"Failed to Initialize ParticleEffect");
		return E_FAIL;
	}

	if (FAILED(Ready_Components()))
	{
		LOG_ERROR(L"Failed to Ready_Components ParticleEffect");
		return E_FAIL;
	}

	return S_OK;
}

void FireFlashEffect::Priority_Update(Float timeDelta)
{
	
}

void FireFlashEffect::Update(Float timeDelta)
{
	m_FlashRemaining = std::max(m_FlashRemaining - timeDelta, 0.f);
	const Float sigilStep = timeDelta / SigilFadeTime;
	m_SigilAlpha = std::clamp(m_SigilAlpha + (m_SigilShown ? sigilStep : -sigilStep), 0.f, 1.f);
}

void FireFlashEffect::Late_Update(Float timeDelta)
{
	
}

void FireFlashEffect::Fixed_Update(Float fixedDelta)
{
	
}

void FireFlashEffect::Flash()
{
	m_FlashRemaining = FlashTime;
	// core.eff/034 sc(1.0, 2.2) read as the size range.
	m_FlashSize = static_cast<Float>(Helper::Random_Double(1.0, 2.2));
	m_FlashRoll = static_cast<Float>(Helper::Random_Double(0.0, XM_2PI));
}

void FireFlashEffect::Set_Muzzle(const Vector3& position, const Quaternion& facing, Bool sigilShown)
{
	m_MuzzlePosition = position;
	m_MuzzleFacing = facing;
	m_SigilShown = sigilShown;
}

HRESULT FireFlashEffect::Render()
{
	if (FAILED(Bind_ShaderResources()))
		return E_FAIL;

	const Matrix facing = Matrix::CreateFromQuaternion(m_MuzzleFacing);
	const Matrix muzzle = Matrix::CreateTranslation(m_MuzzlePosition);
	// .est MOVE colours of core.eff/028 #0 and core.eff/034 #2.
	if (m_SigilAlpha > 0.f &&
		FAILED(Render_Quad(0, Matrix::CreateScale(0.9f) * facing * muzzle, Vector4{ 0.8f, 0.79f, 0.75f, 3.f }, m_SigilAlpha)))
		return E_FAIL;
	if (m_FlashRemaining > 0.f &&
		FAILED(Render_Quad(1, Matrix::CreateScale(m_FlashSize) * Matrix::CreateRotationZ(m_FlashRoll) * facing * muzzle,
			Vector4{ 1.4f, 1.f, 0.8f, 2.f }, m_FlashRemaining / FlashTime)))
		return E_FAIL;

	return S_OK;
}

HRESULT FireFlashEffect::Render_Quad(uint32 frame, const Matrix& world, const Vector4& tint, Float fade)
{
	// Fire_Flash holds the sigil (0) and the flash (1); a missing-file fallback has a single frame.
	frame = std::min(frame, m_Texture->Get_TextureNumSRVs() - 1);
	if (FAILED(m_Shader->Bind_Matrix(WorldMatrix, &world)))
		return E_FAIL;
	if (FAILED(m_Shader->Bind_RawValue("g_QuadTint", &tint, sizeof(Vector4))))
		return E_FAIL;
	const uint32 glow = 0;
	if (FAILED(m_Shader->Bind_RawValue("g_QuadGlow", &glow, sizeof(uint32))))
		return E_FAIL;
	if (FAILED(m_Shader->Bind_RawValue("g_FadeAlpha", &fade, sizeof(Float))))
		return E_FAIL;
	if (FAILED(m_Texture->Bind_ShaderResourceView(m_Shader, DiffuseMap, frame)))
		return E_FAIL;
	if (FAILED(m_Shader->Begin("Quad")))
		return E_FAIL;
	if (FAILED(m_Buffer->Bind_Resources()))
		return E_FAIL;
	return m_Buffer->Render();
}

HRESULT FireFlashEffect::Ready_Components()
{
	Shader::SHADER_DESC shaderDesc{ VTXPARTICLE_POINT_DESC::Tag, VTXPARTICLE_POINT_DESC::Elements, VTXPARTICLE_POINT_DESC::numElements };
	m_Shader = Add_Component<Shader>(ETOI(LEVEL::GAMEPLAY), &shaderDesc);
	if (nullptr == m_Shader)
		return E_FAIL;

	// One unit point at the origin; each quad's size, facing and place come from the world matrix.
	VIBuffer_Particle_Point::VIBUFFER_INSTANCE_POINT_DESC instanceDesc{};
	instanceDesc.numInstances = 1;
	instanceDesc.isLoop = false;
	instanceDesc.center = Vector3::Zero;
	instanceDesc.pivot = Vector3::Zero;
	instanceDesc.range = Vector3::Zero;
	instanceDesc.scale = Vector2{ 1.f, 1.f };
	instanceDesc.speed = Vector2::Zero;
	instanceDesc.lifeTime = Vector2{ 1.f, 1.f };

	m_Buffer = Add_Component<VIBuffer_Particle_Point>(ETOI(LEVEL::GAMEPLAY), &instanceDesc);
	if (nullptr == m_Buffer)
		return E_FAIL;

	auto textureDesc = Texture::TEXTURE_DESC{ Get_LevelIndex(), L"Fire_Flash" };
	m_Texture = Add_Component<Texture>(ETOI(LEVEL::GAMEPLAY), &textureDesc);
	if (nullptr == m_Texture)
		return E_FAIL;

	return S_OK;
}

HRESULT FireFlashEffect::Bind_ShaderResources()
{
	if (FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ViewMatrix, D3DTS::VIEW)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ProjMatrix, D3DTS::PROJ)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Bind_CameraPosition(m_Shader, CameraPosition)))
		return E_FAIL;

	return S_OK;
}

void FireFlashEffect::Submit_RenderGroup()
{
	if (m_SigilAlpha > 0.f || m_FlashRemaining > 0.f)
		GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::BLEND, shared_from_this());
}

Shared<FireFlashEffect> FireFlashEffect::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<FireFlashEffect>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : FireFlashEffect");
		return nullptr;
	}

	return prototype;
}

Shared<GameObject> FireFlashEffect::Clone(void* arg)
{
	auto instance = make_shared<FireFlashEffect>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : FireFlashEffect");
		return nullptr;
	}

	return instance;
}


