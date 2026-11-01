#include "pch.h"
#include "LaserEffect.h"

#include <Game.h>
#include <Shader.h>
#include <Texture.h>

namespace
{
	// Effect_PodLaserImpact (impact_%d.dds): the 017 smoke burst (64 frames at 50 fps), then single images.
	constexpr uint32 BurstFrames = 64;
	enum IMAGE : uint32 { FLASH_STAR = BurstFrames, FLASH, GLYPH, GLYPH_STRIP = GLYPH + 4, NEEDLE, SPIKE, SPARKLE,
		DEBRIS, EMBER, COLUMN, EVADE_STREAK, IMAGE_COUNT };

	constexpr Float BurstLifetime = 2.f;
	constexpr uint32 MaxBursts = 8;
	constexpr uint32 MaxQuads = 6144;
	constexpr Float TwoPi = 6.2831853f;

	const Vector3 Gold{ 1.6f, 1.15f, 0.48f };
	const Vector3 Wind = [] { Vector3 wind{ 1.f, 0.25f, 0.35f }; wind.Normalize(); return wind; }();
	const Vector3 WindSide = [] { Vector3 side = Wind.Cross(Vector3::Up); side.Normalize(); return side; }();

	Float Lifetime(uint8 kind)
	{
		// GLYPH, GLYPH_STRIP, NEEDLE, RIBBON, RISE, DEBRIS, EMBER
		constexpr Float lifetimes[] = { 1.25f, 1.f, 1.25f, 1.6f, 1.4f, 1.6f, 1.f };
		return lifetimes[kind];
	}

	// Holds full strength for 40% of the life, then fades out.
	Float FadeOut(Float age, Float lifetime)
	{
		return age < lifetime * 0.4f ? 1.f : std::max(0.f, 1.f - (age - lifetime * 0.4f) / (lifetime * 0.6f));
	}

	struct QUAD_BUILDER
	{
		Vector3 cameraPosition{};
		Vector3 cameraRight{};
		Vector3 cameraUp{};
		array<vector<VTXMESH>, IMAGE_COUNT * 2> batches{};
		uint32 quads{};

		void Emit(uint32 image, Bool alphaBlend, const array<Vector3, 4>& corners, const array<Vector2, 4>& uvs,
			const Vector3& tint, Float opacity)
		{
			if (opacity <= 0.f || quads >= MaxQuads) return;
			++quads;
			auto& batch = batches[image * 2 + (alphaBlend ? 0 : 1)];
			for (const uint32 corner : { 0u, 1u, 2u, 2u, 1u, 3u })
			{
				VTXMESH vertex{};
				vertex.position = corners[corner];
				vertex.normal = tint;
				vertex.texcoord = uvs[corner];
				vertex.texcoord1 = Vector2{ opacity, 0.f };
				batch.push_back(vertex);
			}
		}

		// Faces the camera; rotation turns it in the screen plane.
		void Billboard(uint32 image, Bool alphaBlend, const Vector3& center, Float width, Float height, Float rotation,
			const Vector3& tint, Float opacity, Float u0 = 0.f, Float u1 = 1.f)
		{
			const Float c = std::cos(rotation), s = std::sin(rotation);
			const Vector3 right = (cameraRight * c + cameraUp * s) * (width * 0.5f);
			const Vector3 up = (cameraUp * c - cameraRight * s) * (height * 0.5f);
			Emit(image, alphaBlend, { center - right + up, center + right + up, center - right - up, center + right - up },
				{ Vector2{ u0, 0.f }, Vector2{ u1, 0.f }, Vector2{ u0, 1.f }, Vector2{ u1, 1.f } }, tint, opacity);
		}

		// Stretched from tail to head and turned about that axis to face the camera.
		// The images are vertical, so their top (v = 0) is the head.
		void Axial(uint32 image, const Vector3& tail, const Vector3& head, Float width, const Vector3& tint, Float opacity)
		{
			Vector3 side = (head - tail).Cross(cameraPosition - (head + tail) * 0.5f);
			if (side.LengthSquared() < 1e-8f) return;
			side.Normalize();
			side *= width * 0.5f;
			Emit(image, false, { head - side, head + side, tail - side, tail + side },
				{ Vector2{ 0.f, 0.f }, Vector2{ 1.f, 0.f }, Vector2{ 0.f, 1.f }, Vector2{ 1.f, 1.f } }, tint, opacity);
		}
	};

	// Wind: drifts away faster and faster and sways like grass once the burst speed has died down.
	Vector3 Sway(Float age, Float phase, Float frequency, Float amplitude)
	{
		const Float grow = std::min(1.f, age / 0.35f);
		return Wind * (0.9f * age * age * grow) +
			WindSide * (amplitude * grow * std::sin(TwoPi * frequency * age + phase)) +
			Vector3::Up * (amplitude * 0.5f * grow * std::sin(TwoPi * frequency * 0.7f * age + phase * 1.3f));
	}
}

LaserEffect::LaserEffect(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: GameObject{ device, context } {}
LaserEffect::LaserEffect(const LaserEffect& rhs) : GameObject{ rhs } {}

HRESULT LaserEffect::Initialize_Prototype()
{
	m_LayerMask.Set_Layer(L"Default");
	return GameObject::Initialize_Prototype();
}

HRESULT LaserEffect::Initialize(void* arg)
{
	if (FAILED(GameObject::Initialize(arg))) return E_FAIL;
	Shader::SHADER_DESC shaderDesc{ L"VTX_Mesh_Effect", VTXMESH::Elements, VTXMESH::numElements };
	m_Shader = Add_Component<Shader>(ETOI(LEVEL::STATIC), &shaderDesc);
	Texture::TEXTURE_DESC textureDesc{ Get_LevelIndex(), L"Effect_PodLaserImpact" };
	m_Texture = Add_Component<Texture>(Get_LevelIndex(), &textureDesc);
	if (!m_Shader || !m_Texture || m_Texture->Get_TextureNumSRVs() < IMAGE_COUNT) return E_FAIL;

	D3D11_BUFFER_DESC bufferDesc{};
	bufferDesc.ByteWidth = sizeof(VTXMESH) * MaxQuads * 6;
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	return m_Device->CreateBuffer(&bufferDesc, nullptr, m_VertexBuffer.GetAddressOf());
}

Float LaserEffect::Random(Float minValue, Float maxValue)
{
	return std::uniform_real_distribution<Float>{ minValue, maxValue }(m_Random);
}

Vector3 LaserEffect::Random_Direction()
{
	std::normal_distribution<Float> normal{};
	Vector3 direction{ normal(m_Random), normal(m_Random), normal(m_Random) };
	if (direction.LengthSquared() < 1e-6f) return Vector3::Up;
	direction.Normalize();
	return direction;
}

void LaserEffect::Play(const Vector3& position)
{
	if (m_Bursts.size() >= MaxBursts) m_Bursts.erase(m_Bursts.begin());
	BURST& burst = m_Bursts.emplace_back();
	burst.origin = position;
	burst.spin = Random(0.f, TwoPi);
	auto& particles = burst.particles;
	const auto sway = [this](PARTICLE& particle)
	{
		particle.phase = Random(0.f, TwoPi);
		particle.frequency = Random(1.2f, 2.2f);
		particle.amplitude = Random(0.08f, 0.2f);
	};

	for (uint32 i = 0; i < 40; ++i)
	{
		PARTICLE& glyph = particles.emplace_back();
		glyph.kind = KIND::GLYPH;
		glyph.image = GLYPH + i % 4;
		glyph.position = position + Random_Direction() * 0.3f;
		glyph.velocity = Random_Direction() * Random(4.f, 9.f);
		glyph.drag = 0.93f;
		glyph.size = Random(0.15f, 0.28f);
		sway(glyph);
	}
	for (uint32 i = 0; i < 6; ++i)
	{
		PARTICLE& strip = particles.emplace_back();
		strip.kind = KIND::GLYPH_STRIP;
		strip.image = GLYPH_STRIP;
		strip.position = position;
		strip.velocity = Random_Direction() * Random(6.f, 10.f);
		strip.drag = 0.9f;
		strip.size = Random(0.5f, 0.7f);
		sway(strip);
	}
	// Many small needles packed around the impact.
	for (uint32 i = 0; i < 130; ++i)
	{
		PARTICLE& needle = particles.emplace_back();
		needle.kind = KIND::NEEDLE;
		needle.image = Random(0.f, 1.f) < 0.3f ? SPIKE : NEEDLE;
		needle.position = position + Random_Direction() * Random(0.2f, 1.2f);
		needle.velocity = Random_Direction() * Random(3.f, 9.f);
		needle.drag = 0.93f;
		needle.length = Random(0.25f, 0.7f);
		needle.size = 0.05f;
		if (Random(0.f, 1.f) < 0.25f)
			needle.variant = GLYPH + static_cast<uint32>(Random(0.f, 3.99f));
		sway(needle);
	}
	// Three ribbons of glitter that swirl around the impact and climb as they widen.
	for (uint32 ribbon = 0; ribbon < 3; ++ribbon)
	{
		const Float angle = Random(0.f, TwoPi);
		const Float spin = Random(2.5f, 4.f) * (Random(0.f, 1.f) < 0.5f ? -1.f : 1.f);
		const Float delay = Random(0.f, 0.1f);
		for (uint32 i = 0; i < 110; ++i)
		{
			PARTICLE& glitter = particles.emplace_back();
			glitter.kind = KIND::RIBBON;
			glitter.image = SPARKLE;
			glitter.position = Random_Direction() * Random(0.f, 0.1f);
			glitter.delay = delay;
			glitter.size = Random(0.06f, 0.11f);
			glitter.ribbonAngle = angle;
			glitter.ribbonSpin = spin;
			glitter.ribbonT = Random(0.f, 1.f);
			sway(glitter);
		}
	}
	for (uint32 i = 0; i < 60; ++i)
	{
		PARTICLE& rise = particles.emplace_back();
		rise.kind = KIND::RISE;
		rise.image = SPARKLE;
		rise.position = position + Vector3{ Random(-0.25f, 0.25f), Random(-0.3f, 0.3f), Random(-0.25f, 0.25f) };
		rise.velocity = Vector3{ 0.f, Random(2.f, 9.f), 0.f };
		rise.drag = 0.97f;
		rise.delay = Random(0.f, 0.4f);
		rise.size = Random(0.05f, 0.09f);
		sway(rise);
	}
	for (uint32 i = 0; i < 24; ++i)
	{
		PARTICLE& debris = particles.emplace_back();
		debris.kind = KIND::DEBRIS;
		debris.image = DEBRIS;
		debris.position = position + Random_Direction() * 0.3f;
		Vector3 direction = Random_Direction();
		direction.y = std::abs(direction.y) + 0.3f;
		direction.Normalize();
		debris.velocity = direction * Random(4.f, 10.f);
		debris.gravity = 9.8f;
		debris.size = Random(0.12f, 0.3f);
		debris.spin = Random(-8.f, 8.f);
		debris.variant = static_cast<uint32>(Random(0.f, 3.99f));
	}
	// The ember fountain starts once the flash is over.
	for (uint32 i = 0; i < 70; ++i)
	{
		PARTICLE& ember = particles.emplace_back();
		ember.kind = KIND::EMBER;
		ember.image = EMBER;
		ember.position = position;
		const Float heading = Random(0.f, TwoPi);
		const Float spread = Random(1.f, 2.5f);
		ember.velocity = Vector3{ std::cos(heading) * spread, Random(6.f, 12.f), std::sin(heading) * spread };
		ember.gravity = 6.f;
		ember.delay = Random(0.35f, 0.6f);
		ember.length = 0.3f;
		ember.size = 0.07f;
	}
}

void LaserEffect::PlayEvadeBurst(const Vector3& position)
{
	if (m_Bursts.size() >= MaxBursts) m_Bursts.erase(m_Bursts.begin());
	BURST& burst = m_Bursts.emplace_back();
	burst.origin = position;
	burst.evade = true;
	// Reuse the laser impact's circular, wind-swaying gold glitter during the texture flash.
	for (uint32 ribbon = 0; ribbon < 3; ++ribbon)
	{
		const Float angle = Random(0.f, TwoPi);
		const Float spin = Random(2.5f, 4.f) * (Random(0.f, 1.f) < 0.5f ? -1.f : 1.f);
		const Float delay = Random(0.02f, 0.05f);
		for (uint32 i = 0; i < 110; ++i)
		{
			PARTICLE& glitter = burst.particles.emplace_back();
			glitter.kind = KIND::RIBBON;
			glitter.image = SPARKLE;
			glitter.position = Random_Direction() * Random(0.f, 0.1f);
			glitter.delay = delay;
			glitter.size = Random(0.06f, 0.11f);
			glitter.ribbonAngle = angle;
			glitter.ribbonSpin = spin;
			glitter.ribbonT = Random(0.f, 1.f);
			glitter.phase = Random(0.f, TwoPi);
			glitter.frequency = Random(1.2f, 2.2f);
			glitter.amplitude = Random(0.08f, 0.2f);
		}
	}
}

void LaserEffect::Update(Float timeDelta)
{
	for (BURST& burst : m_Bursts)
	{
		burst.age += timeDelta;
		for (PARTICLE& particle : burst.particles)
		{
			if (particle.kind == KIND::RIBBON || burst.age < particle.delay) continue;
			particle.velocity *= std::pow(particle.drag, timeDelta * 60.f);
			particle.velocity.y -= particle.gravity * timeDelta;
			particle.position += particle.velocity * timeDelta;
		}
	}
	std::erase_if(m_Bursts, [](const BURST& burst) { return burst.age >= (burst.evade ? 0.8f : BurstLifetime); });
}

void LaserEffect::Late_Update(Float)
{
	m_Draws.clear();
	if (m_Bursts.empty()) return;

	QUAD_BUILDER builder{};
	const Matrix cameraWorld = GAME_INSTANCE->Get_Transform(D3DTS::VIEW).Invert();
	builder.cameraPosition = cameraWorld.Translation();
	builder.cameraRight = cameraWorld.Right();
	builder.cameraUp = cameraWorld.Up();
	builder.cameraRight.Normalize();
	builder.cameraUp.Normalize();

	for (const BURST& burst : m_Bursts)
	{
		const Float age = burst.age;
		if (burst.evade && age < 0.2f)
		{
			const auto parent = Get_Parent();
			const Vector3 origin = parent ? parent->Get_Transform()->Get_Position() + Vector3::Up * 0.85f : burst.origin;
			const Float growth = std::min(1.f, age / 0.08f);
			const Float fade = age < 0.055f ? 1.f : 1.f - (age - 0.055f) / 0.145f;
			for (uint32 strip = 0; strip < 5; ++strip)
			{
				const Float spread = static_cast<Float>(strip) - 2.f;
				const Float offset = spread * 0.13f + std::sin(age * 13.f + spread * 1.7f) * 0.015f;
				const Float lengthScale = 1.f - 0.08f * std::abs(spread);
				const Float height = (0.32f + 2.85f * growth) * lengthScale;
				// Keep each strip's lower edge near the player while its upper edge grows outward.
				const Vector3 center = origin + builder.cameraRight * offset + builder.cameraUp * (height * 0.5f - 0.16f);
				const Float width = strip == 2 ? 0.36f : 0.16f + 0.03f * (2.f - std::abs(spread));
				builder.Billboard(EVADE_STREAK, false, center, width, height, 0.f,
					Vector3{ 1.f, 1.f, 1.f }, fade * (strip == 2 ? 1.15f : 0.7f));
			}
		}
		// The 017 burst: grey smoke (alpha) and its white-hot glow (additive) for the first moments.
		const uint32 frame = static_cast<uint32>(age * 50.f);
		if (!burst.evade && frame < BurstFrames)
		{
			const Float smoke = age < 1.f ? 0.9f : 0.9f * (1.f - (age - 1.f) / 0.28f);
			builder.Billboard(frame, true, burst.origin, 5.f, 5.f, burst.spin, Vector3{ 0.62f, 0.6f, 0.58f }, smoke);
			builder.Billboard(frame, false, burst.origin, 5.f, 5.f, burst.spin, Vector3{ 1.5f, 1.4f, 1.25f },
				std::max(0.f, 1.f - age / 0.35f));
		}
		if (!burst.evade && age < 0.15f)
			builder.Billboard(FLASH_STAR, false, burst.origin, 4.5f * (0.6f + age / 0.25f), 4.5f * (0.6f + age / 0.25f),
				burst.spin, Vector3{ 1.6f, 1.5f, 1.3f }, 1.f - age / 0.15f);
		if (!burst.evade && age < 0.25f)
			builder.Billboard(FLASH, false, burst.origin, 3.f, 3.f, 0.f, Vector3{ 1.4f, 1.1f, 0.9f }, 1.f - age / 0.25f);
		// The glitter column shoots up from the impact.
		if (!burst.evade && age < 1.2f)
		{
			const Float opacity = age < 0.15f ? age / 0.15f : 1.f - (age - 0.15f) / 1.05f;
			builder.Axial(COLUMN, burst.origin - Vector3::Up * 0.3f, burst.origin + Vector3::Up * (6.f * std::min(1.f, age / 0.3f)),
				0.45f, Gold * 1.3f, 0.8f * opacity);
		}

		for (const PARTICLE& particle : burst.particles)
		{
			const Float particleAge = age - particle.delay;
			const Float lifetime = burst.evade ? 0.55f : Lifetime(static_cast<uint8>(particle.kind));
			if (particleAge < 0.f || particleAge >= lifetime) continue;
			const Float fade = FadeOut(particleAge, lifetime);
			const Vector3 sway = Sway(particleAge, particle.phase, particle.frequency, particle.amplitude);

			switch (particle.kind)
			{
			case KIND::GLYPH:
				builder.Billboard(particle.image, false, particle.position + sway, particle.size, particle.size, 0.f, Gold, fade);
				break;
			case KIND::GLYPH_STRIP:
				builder.Billboard(particle.image, false, particle.position + sway, particle.size, particle.size * 0.25f, 0.f, Gold, fade);
				break;
			case KIND::NEEDLE:
			{
				// The needle leans back and forth in the wind.
				Vector3 direction = particle.velocity;
				direction.Normalize();
				direction += WindSide * (0.35f * std::min(1.f, particleAge / 0.35f) *
					std::sin(TwoPi * particle.frequency * particleAge + particle.phase));
				direction.Normalize();
				const Vector3 tail = particle.position + sway;
				const Vector3 head = tail + direction * particle.length;
				builder.Axial(particle.image, tail, head, particle.size, Gold * 1.2f, fade);
				if (particle.variant != 0)
					builder.Billboard(particle.variant, false, head, 0.16f, 0.16f, 0.f, Gold, fade);
				break;
			}
			case KIND::RIBBON:
			{
				const Float angle = particle.ribbonAngle + particle.ribbonSpin * particleAge + particle.ribbonT * 2.4f;
				const Float radius = 0.4f + 2.6f * (1.f - std::exp(-2.2f * particleAge));
				const Float height = (particle.ribbonT - 0.5f) * 1.4f + 1.2f * particleAge;
				const Vector3 center = burst.origin + Vector3{ std::cos(angle) * radius, height, std::sin(angle) * radius } +
					particle.position + sway;
				const Float twinkle = 0.55f + 0.45f * std::sin(18.f * particleAge + particle.phase);
				builder.Billboard(particle.image, false, center, particle.size, particle.size, 0.f, Gold * 1.1f, fade * twinkle);
				break;
			}
			case KIND::RISE:
				builder.Billboard(particle.image, false, particle.position + sway, particle.size, particle.size, 0.f,
					Gold * 1.1f,
					fade * (0.6f + 0.4f * std::sin(14.f * particleAge + particle.phase)));
				break;
			case KIND::DEBRIS:
			{
				// debris.dds holds four 64 px pieces side by side.
				const Float u0 = static_cast<Float>(particle.variant) * 0.25f;
				builder.Billboard(particle.image, true, particle.position, particle.size, particle.size,
					particle.spin * particleAge, Vector3{ 0.12f, 0.11f, 0.1f }, fade, u0, u0 + 0.25f);
				break;
			}
			case KIND::EMBER:
			{
				Vector3 direction = particle.velocity;
				if (direction.LengthSquared() < 1e-6f) break;
				direction.Normalize();
				builder.Axial(particle.image, particle.position - direction * particle.length, particle.position, particle.size,
					Vector3{ 1.8f, 0.75f, 0.28f }, fade);
				break;
			}
			}
		}
	}

	if (builder.quads == 0) return;
	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (FAILED(m_Context->Map(m_VertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;
	auto* vertices = static_cast<VTXMESH*>(mapped.pData);
	uint32 written = 0;
	// Alpha-blended smoke and debris first, then the additive light on top.
	for (const Bool alphaBlend : { true, false })
	{
		for (uint32 image = 0; image < IMAGE_COUNT; ++image)
		{
			const auto& batch = builder.batches[image * 2 + (alphaBlend ? 0 : 1)];
			if (batch.empty()) continue;
			std::copy(batch.begin(), batch.end(), vertices + written);
			m_Draws.push_back({ image, alphaBlend, written, static_cast<uint32>(batch.size()) });
			written += static_cast<uint32>(batch.size());
		}
	}
	m_Context->Unmap(m_VertexBuffer.Get(), 0);
}

HRESULT LaserEffect::Render()
{
	if (m_Draws.empty()) return S_OK;
	const Matrix identity = Matrix::Identity;
	if (FAILED(m_Shader->Bind_Matrix(WorldMatrix, &identity)) ||
		FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ViewMatrix, D3DTS::VIEW)) ||
		FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ProjMatrix, D3DTS::PROJ))) return E_FAIL;
	ID3D11Buffer* buffer = m_VertexBuffer.Get();
	const uint32 stride = sizeof(VTXMESH), offset = 0;
	m_Context->IASetVertexBuffers(0, 1, &buffer, &stride, &offset);
	m_Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	for (const DRAW& draw : m_Draws)
	{
		const char* pass = draw.image == EVADE_STREAK ? "EvadeStreak" : (draw.alphaBlend ? "AlphaBlend" : "Additive");
		if (FAILED(m_Texture->Bind_ShaderResourceView(m_Shader, "g_EffectMap", draw.image)) ||
			FAILED(m_Shader->Begin(pass))) return E_FAIL;
		m_Context->Draw(draw.vertexCount, draw.firstVertex);
	}
	return S_OK;
}

void LaserEffect::Submit_RenderGroup()
{
	// After the map's transparent pass, like WeaponHalo; depth-tested against the opaque scene.
	if (!m_Draws.empty()) GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::WORLDUI, shared_from_this());
}

Shared<LaserEffect> LaserEffect::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<LaserEffect>(device, context);
	return SUCCEEDED(prototype->Initialize_Prototype()) ? prototype : nullptr;
}

Shared<GameObject> LaserEffect::Clone(void* arg)
{
	auto instance = make_shared<LaserEffect>(*this);
	return SUCCEEDED(instance->Initialize(arg)) ? instance : nullptr;
}
