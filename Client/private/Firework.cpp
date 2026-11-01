#include "pch.h"
#include "Firework.h"
#include "Game.h"
#include "Transform.h"
#include <SoundEmitter.h>
#include "VIBuffer_Particle_Point.h"
#include <Shader.h>
#include <SpdLogger.h>

namespace
{
	// Burst k's n-th random number in [0, 1).
	Float Burst_Random(int32 k, uint32 n)
	{
		uint32 x = static_cast<uint32>(k) * 0x9E3779B1u ^ (n + 1u) * 0x85EBCA77u;
		x ^= x >> 15; x *= 0x2C1B3C6Du;
		x ^= x >> 12; x *= 0x297A2D39u;
		x ^= x >> 15;
		return static_cast<Float>(x & 0xFFFFFFu) / 16777216.f;
	}

	// Chrysanthemum colours from the park footage: white-gold, warm white, pink, violet, green, pale blue.
	const Vector3 StreakColors[] = {
		{ 1.f, 0.78f, 0.4f }, { 1.f, 0.95f, 0.85f }, { 1.f, 0.45f, 0.7f },
		{ 0.7f, 0.45f, 1.f }, { 0.5f, 1.f, 0.55f }, { 0.7f, 0.85f, 1.f },
	};
	constexpr Float StreakLife = 2.6f;
	constexpr Float BallLife = 3.2f;
	constexpr Float BallChance = 0.35f;

	// A shell's climb, the Spray pass's motion (Shader_VtxParticlePoint Spray_Position) for one spark straight along way.
	constexpr Float ShellSpeed = 28.f;
	constexpr Float ShellDrag = 1.2f;
	constexpr Float ShellGravity = 9.f;
	Vector3 Shell_Position(const Vector3& muzzle, const Vector3& way, Float age)
	{
		return muzzle + way * ShellSpeed * (1.f - expf(-ShellDrag * age)) / ShellDrag
			- Vector3{ 0.f, 0.5f * ShellGravity * age * age, 0.f };
	}
}

Firework::Firework() : GameObject{} {}
Firework::Firework(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: GameObject{ device, context } {}
Firework::Firework(const Firework& rhs)
	: GameObject{ rhs }, m_Interval{ rhs.m_Interval }, m_Radius{ rhs.m_Radius }, m_Area{ rhs.m_Area } {}

HRESULT Firework::Initialize_Prototype()
{
	return GameObject::Initialize_Prototype();
}

HRESULT Firework::Initialize(void* arg)
{
	if (FAILED(GameObject::Initialize(arg)))
	{
		LOG_ERROR(L"Failed to Initialize Firework {}", m_ObjectName);
		return E_FAIL;
	}

	Shader::SHADER_DESC shaderDesc{ VTXPARTICLE_POINT_DESC::Tag, VTXPARTICLE_POINT_DESC::Elements, VTXPARTICLE_POINT_DESC::numElements };
	m_Shader = Add_Component<Shader>(ETOI(LEVEL::STATIC), &shaderDesc);
	// One point per star; the shader places each from its instance index.
	VIBuffer_Particle_Point::VIBUFFER_INSTANCE_POINT_DESC instanceDesc{};
	instanceDesc.numInstances = MaxStars;
	instanceDesc.scale = Vector2{ 1.f, 1.f };
	instanceDesc.lifeTime = Vector2{ 1.f, 1.f };
	m_Buffer = Add_Component<VIBuffer_Particle_Point>(ETOI(LEVEL::STATIC), &instanceDesc);
	SoundEmitter::SOUND_EMITTER_DESC emitterDesc{};
	emitterDesc.soundTag = L"r130_Amb_Firework";
	emitterDesc.volume = 0.5f;
	emitterDesc.nearDistance = 40.f;
	emitterDesc.farDistance = 250.f;
	if (nullptr == Add_Component<SoundEmitter>(ETOI(LEVEL::STATIC), &emitterDesc))
		return E_FAIL;
	if (!m_Shader || !m_Buffer)
	{
		LOG_ERROR(L"Failed to ready Firework components {}", m_ObjectName);
		return E_FAIL;
	}
	m_Start = std::chrono::steady_clock::now();
	return S_OK;
}

void Firework::Submit_RenderGroup()
{
	if (Is_Active())
		GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::WORLDUI, shared_from_this());
}

void Firework::Launch(const Vector3& muzzle, const Vector3& way)
{
	Vector3 unit = way;
	if (unit.LengthSquared() < 0.0001f)
		unit = Vector3::UnitY;
	unit.Normalize();
	const Float now = std::chrono::duration<Float>(std::chrono::steady_clock::now() - m_Start).count();
	m_Shells.push_back({ muzzle, unit, now, static_cast<int32>(m_Shells.size()) + static_cast<int32>(now * 1000.f) });
}

void Firework::Set_Muted(Bool muted)
{
	const Shared<SoundEmitter> emitter = Get_Component<SoundEmitter>();
	if (!emitter || muted == (m_UnmutedVolume >= 0.f))
		return;
	if (muted)
	{
		m_UnmutedVolume = emitter->Get_Volume();
		emitter->Set_Volume(0.f);
	}
	else
	{
		emitter->Set_Volume(m_UnmutedVolume);
		m_UnmutedVolume = -1.f;
	}
}

/* Burst k goes off at (k + 0.6 x random) x Interval; the ones still alive now are drawn, one draw each. */
HRESULT Firework::Render()
{
	if (FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ViewMatrix, D3DTS::VIEW)) ||
		FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ProjMatrix, D3DTS::PROJ)) ||
		FAILED(GAME_INSTANCE->Bind_CameraPosition(m_Shader, CameraPosition)))
		return E_FAIL;

	const Float now = std::chrono::duration<Float>(std::chrono::steady_clock::now() - m_Start).count();
	std::erase_if(m_Shells, [now](const SHELL& shell) { return now - shell.launched > ShellRise + BallLife * 1.1f; });
	for (const SHELL& shell : m_Shells)
		if (FAILED(Render_Shell(shell, now)))
			return E_FAIL;

	if (m_Interval <= 0.01f || m_Radius <= 0.f)
		return S_OK;
	m_Transform->Update_WorldMatrix();
	const Vector3 origin = m_Transform->Get_WorldMatrix().Translation();
	const int32 last = static_cast<int32>(now / m_Interval);
	const int32 first = last - static_cast<int32>(BallLife * 1.1f / m_Interval) - 1;
	for (int32 k = (std::max)(first, 0); k <= last; ++k)
	{
		const Bool balls = Burst_Random(k, 1) < BallChance;
		const Float life = balls ? BallLife : StreakLife;
		const Float age = now - (static_cast<Float>(k) + 0.6f * Burst_Random(k, 0)) * m_Interval;
		if (age < 0.f || age > life * 1.1f)
			continue;

		const Float angle = Burst_Random(k, 2) * XM_2PI;
		const Float reach = sqrtf(Burst_Random(k, 3)) * m_Area;
		const Vector4 burst{ origin.x + cosf(angle) * reach, origin.y + (Burst_Random(k, 4) - 0.3f) * m_Area * 0.35f,
			origin.z + sinf(angle) * reach, age };
		const Vector4 shape{ m_Radius * (0.75f + 0.5f * Burst_Random(k, 5)) * (balls ? 0.7f : 1.f), life,
			Burst_Random(k, 7), balls ? 1.f : 0.f };
		const Vector3& color = StreakColors[static_cast<uint32>(Burst_Random(k, 6) * size(StreakColors)) % size(StreakColors)];
		if (FAILED(Render_Burst(burst, shape, Vector4{ color.x, color.y, color.z, 1.5f }, balls ? BallStars : MaxStars)))
			return E_FAIL;
	}
	return S_OK;
}

HRESULT Firework::Render_Burst(const Vector4& burst, const Vector4& shape, const Vector4& tint, uint32 stars)
{
	if (FAILED(m_Shader->Bind_RawValue("g_FireworkBurst", &burst, sizeof(Vector4))) ||
		FAILED(m_Shader->Bind_RawValue("g_FireworkShape", &shape, sizeof(Vector4))) ||
		FAILED(m_Shader->Bind_RawValue("g_FireworkStars", &stars, sizeof(uint32))) ||
		FAILED(m_Shader->Bind_RawValue("g_QuadTint", &tint, sizeof(Vector4))) ||
		FAILED(m_Shader->Begin("Firework")) ||
		FAILED(m_Buffer->Bind_Resources()))
		return E_FAIL;
	return m_Buffer->Render();
}

/* The climb is one Spray spark (count 1, no spread) whose life outlasts the rise, so it stays bright; the burst takes
   the shell's seed for its style and colour the way a scheduled burst takes k. */
HRESULT Firework::Render_Shell(const SHELL& shell, Float now)
{
	const Float age = now - shell.launched;
	if (age < 0.f)
		return S_OK;
	const int32 k = shell.seed;
	if (age < ShellRise)
	{
		const Vector4 burst{ shell.muzzle.x, shell.muzzle.y, shell.muzzle.z, age };
		const Vector4 aim{ shell.way.x, shell.way.y, shell.way.z, 0.f };
		const Vector4 motion{ ShellSpeed, ShellSpeed, ShellGravity, ShellDrag };
		const Vector4 look{ ShellRise * 8.f, 0.12f, 0.12f, Burst_Random(k, 8) * 100.f };
		const Vector4 tint{ 1.f, 0.7f, 0.35f, 1.5f };
		const uint32 count = 1;
		if (FAILED(m_Shader->Bind_RawValue("g_SprayBurst", &burst, sizeof(Vector4))) ||
			FAILED(m_Shader->Bind_RawValue("g_SprayAim", &aim, sizeof(Vector4))) ||
			FAILED(m_Shader->Bind_RawValue("g_SprayMotion", &motion, sizeof(Vector4))) ||
			FAILED(m_Shader->Bind_RawValue("g_SprayLook", &look, sizeof(Vector4))) ||
			FAILED(m_Shader->Bind_RawValue("g_SprayCount", &count, sizeof(uint32))) ||
			FAILED(m_Shader->Bind_RawValue("g_QuadTint", &tint, sizeof(Vector4))) ||
			FAILED(m_Shader->Begin("Spray")) ||
			FAILED(m_Buffer->Bind_Resources()))
			return E_FAIL;
		return m_Buffer->Render();
	}

	const Bool balls = Burst_Random(k, 1) < BallChance;
	const Float life = balls ? BallLife : StreakLife;
	const Float burstAge = age - ShellRise;
	if (burstAge > life * 1.1f)
		return S_OK;
	const Vector3 center = Shell_Position(shell.muzzle, shell.way, ShellRise);
	const Vector4 shape{ ShellRadius * (0.75f + 0.5f * Burst_Random(k, 5)) * (balls ? 0.7f : 1.f), life,
		Burst_Random(k, 7), balls ? 1.f : 0.f };
	const Vector3& color = StreakColors[static_cast<uint32>(Burst_Random(k, 6) * size(StreakColors)) % size(StreakColors)];
	return Render_Burst(Vector4{ center.x, center.y, center.z, burstAge }, shape, Vector4{ color.x, color.y, color.z, 1.5f },
		balls ? BallStars : MaxStars);
}

Shared<Firework> Firework::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<Firework>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		LOG_ERROR(L"Failed to Created : Firework");
		MSG_BOX("Failed to Created : Firework");
	}
	return prototype;
}

Shared<GameObject> Firework::Clone(void* arg)
{
	auto instance = make_shared<Firework>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		LOG_ERROR(L"Failed to Cloned : Firework");
		MSG_BOX("Failed to Cloned : Firework");
	}
	return instance;
}
