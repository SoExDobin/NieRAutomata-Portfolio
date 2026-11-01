#include "pch.h"
#include "EmTelegraph.h"

#include <Game.h>
#include <Model.h>
#include <PartObject.h>
#include <Shader.h>
#include <SpdLogger.h>

#include "VIBuffer_Particle_Point.h"

EmTelegraph::EmTelegraph(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: ParticleEffect{ device, context } {}
EmTelegraph::EmTelegraph(const EmTelegraph& rhs)
	: ParticleEffect{ rhs } {}

void EmTelegraph::Play(const Shared<PartObject>& body, const Shared<Model>& model, Float size, const wstring& soundTag,
	Float eyeReach)
{
	GAME_INSTANCE->StopSound(SOUNDCHANNEL::CHANNEL_19);
	GAME_INSTANCE->PlaySoundFXOnce(soundTag, SOUNDCHANNEL::CHANNEL_19, 0.5f);

	EM_TELEGRAPH_DESC desc{};
	desc.body = body;
	desc.model = model;
	desc.eyeBone = model->Get_BoneIndexByName("bone16");
	desc.headBone = model->Get_BoneIndexByName("bone3");
	desc.eyeReach = eyeReach;
	desc.size = size;
	if (desc.eyeBone >= 0)
		GAME_INSTANCE->Instantiate<EmTelegraph>(L"EmTelegraph", body->Get_LevelIndex(), &desc);
}

Vector4 EmTelegraph::Eye_Color(Float tellAge)
{
	// The flare pops up over the first fifth of the tell and fades over the rest.
	Float flare = 0.f;
	if (tellAge >= 0.f && tellAge < TellTime)
	{
		const Float t = tellAge / TellTime;
		flare = t < 0.2f ? t / 0.2f : 1.f - (t - 0.2f) / 0.8f;
	}
	const Float scale = 1.f + EyeFlare * flare;
	return Vector4{ EyeGlow.x * scale, EyeGlow.y * scale, EyeGlow.z * scale, 1.f };
}

HRESULT EmTelegraph::Render_Eyes(const Shared<Model>& model, const Shared<Shader>& shader, Float tellAge)
{
	const Vector4 color = Eye_Color(tellAge);
	if (FAILED(shader->Bind_RawValue("g_EyeColor", &color, sizeof(Vector4))))
		return E_FAIL;
	// Every mesh with its own material bound; EM_Eye draws only where a material has the eye mask (g_EmissiveMap).
	const uint32 numMeshes = model->Get_NumMeshes();
	for (uint32 i = 0; i < numMeshes; ++i)
	{
		if (FAILED(model->Bind_BoneMatrices(shader, BoneMatrices, i)) ||
			FAILED(model->BindAndBeginMaterial(shader, i)) ||
			FAILED(shader->Begin("EM_Eye")) ||
			FAILED(model->Render(i)))
			return E_FAIL;
	}
	return S_OK;
}

void EmTelegraph::Advance_Tell(Float& tellAge, Float timeDelta)
{
	if (tellAge < 0.f)
		return;
	tellAge += timeDelta;
	if (tellAge >= TellTime)
		tellAge = -1.f;
}

HRESULT EmTelegraph::Initialize_Prototype()
{
	return ParticleEffect::Initialize_Prototype();
}

HRESULT EmTelegraph::Initialize(void* arg)
{
	if (FAILED(ParticleEffect::Initialize(arg)))
		return E_FAIL;
	// The Editor's Spawn clones it with no desc: it has no eye to ride and goes at once.
	if (arg)
		m_Desc = *static_cast<EM_TELEGRAPH_DESC*>(arg);

	if (FAILED(Ready_Components()))
	{
		LOG_ERROR(L"Failed to Ready_Components EmTelegraph");
		return E_FAIL;
	}
	return S_OK;
}

void EmTelegraph::Update(Float timeDelta)
{
	m_Age += timeDelta;
	if (m_Age >= TellTime || m_Desc.body.expired() || m_Desc.model.expired() || m_Desc.eyeBone < 0)
		GAME_INSTANCE->Destroy(Get_ObjectGuid());
}

void EmTelegraph::Submit_RenderGroup()
{
	if (m_Age < TellTime && !m_Desc.body.expired())
		GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::EMISSIVE, shared_from_this());
}

HRESULT EmTelegraph::Render()
{
	const Shared<PartObject> body = m_Desc.body.lock();
	const Shared<Model> model = m_Desc.model.lock();
	if (!body || !model)
		return S_OK;

	// The eye this frame: every update, the body's animation included, is done by the time anything renders.
	Vector3 eye = model->Get_BoneMatrix(m_Desc.eyeBone).Translation();
	if (m_Desc.eyeReach > 0.f && m_Desc.headBone >= 0)
	{
		Vector3 out = eye - model->Get_BoneMatrix(m_Desc.headBone).Translation();
		out.Normalize();
		eye += out * m_Desc.eyeReach;
	}
	eye = Vector3::Transform(eye, body->Get_CombinedWorldMatrix());
	const Matrix view = GAME_INSTANCE->Get_Transform(D3DTS::VIEW);
	const Matrix proj = GAME_INSTANCE->Get_Transform(D3DTS::PROJ);
	const Matrix cameraWorld = view.Invert();
	Vector3 toCamera = cameraWorld.Translation() - eye;
	toCamera.Normalize();
	const Vector3 center = eye + toCamera * (CameraLift * m_Desc.size);
	const Float depth = Vector3::Transform(center, view).z;
	if (depth <= 0.f)
		return S_OK;

	// Sized in screens at the streak's depth, facing the camera so it lies level across the screen; it pops to full
	// height in the first fifth and fades out.
	const Float t = m_Age / TellTime;
	const Float grow = t < 0.2f ? 0.4f + 3.f * t : 1.f;
	const Float fade = t < 0.2f ? 1.f : 1.f - (t - 0.2f) / 0.8f;
	const Float screenWidth = 2.f * depth / proj._11;
	const Float screenHeight = 2.f * depth / proj._22;
	Matrix world = Matrix::CreateScale(StreakWidth * screenWidth, StreakHeight * screenHeight * grow, 1.f) * cameraWorld;
	world.Translation(center);

	// Frame 0 of Eye_Flash is the streak; a missing-file fallback has a single frame too.
	const uint32 glow = 0;
	if (FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ViewMatrix, D3DTS::VIEW)) ||
		FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ProjMatrix, D3DTS::PROJ)) ||
		FAILED(GAME_INSTANCE->Bind_CameraPosition(m_Shader, CameraPosition)) ||
		FAILED(m_Shader->Bind_RawValue("g_QuadTint", &StreakTint, sizeof(Vector4))) ||
		FAILED(m_Shader->Bind_RawValue("g_QuadGlow", &glow, sizeof(uint32))) ||
		FAILED(m_Shader->Bind_Matrix(WorldMatrix, &world)) ||
		FAILED(m_Shader->Bind_RawValue("g_FadeAlpha", &fade, sizeof(Float))) ||
		FAILED(m_Texture->Bind_ShaderResourceView(m_Shader, DiffuseMap, 0)) ||
		FAILED(m_Shader->Begin("Quad")) ||
		FAILED(m_Buffer->Bind_Resources()))
		return E_FAIL;
	return m_Buffer->Render();
}

HRESULT EmTelegraph::Ready_Components()
{
	Shader::SHADER_DESC shaderDesc{ VTXPARTICLE_POINT_DESC::Tag, VTXPARTICLE_POINT_DESC::Elements, VTXPARTICLE_POINT_DESC::numElements };
	m_Shader = Add_Component<Shader>(ETOI(LEVEL::GAMEPLAY), &shaderDesc);
	if (nullptr == m_Shader)
		return E_FAIL;

	// One unit point at the origin; the quad's size, facing and place come from the world matrix.
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

	auto textureDesc = Texture::TEXTURE_DESC{ Get_LevelIndex(), L"Eye_Flash" };
	m_Texture = Add_Component<Texture>(ETOI(LEVEL::GAMEPLAY), &textureDesc);
	if (nullptr == m_Texture)
		return E_FAIL;

	return S_OK;
}

Shared<EmTelegraph> EmTelegraph::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<EmTelegraph>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : EmTelegraph");
		return nullptr;
	}

	return prototype;
}

Shared<GameObject> EmTelegraph::Clone(void* arg)
{
	auto instance = make_shared<EmTelegraph>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : EmTelegraph");
		return nullptr;
	}

	return instance;
}
