#include "pch.h"
#include "EmRing.h"

#include <Game.h>
#include <Shader.h>
#include <Texture.h>

#include "Entity.h"
#include "Random_Helper.h"

EmRing::EmRing(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: GameObject{ device, context } {}
EmRing::EmRing(const EmRing& rhs) : GameObject{ rhs } {}

HRESULT EmRing::Initialize_Prototype()
{
	// Pl0000::TryEvade refuses a dash evasion when the attacker is on the Bullet layer, so this attack hands itself in as
	// the attacker: it has to be avoided by moving or jumping, not dodged.
	m_LayerMask.Set_Layer(L"Bullet");
	return GameObject::Initialize_Prototype();
}

HRESULT EmRing::Initialize(void* arg)
{
	if (!arg || FAILED(GameObject::Initialize(arg))) return E_FAIL;
	m_Desc = *static_cast<EM_RING_DESC*>(arg);
	m_Center = m_Desc.start;

	// BURST: the radii and flight times step evenly from the first ring to the last. GROUND: one ring.
	const uint32 count = m_Desc.mode == MODE::BURST ? std::max(m_Desc.count, 1u) : 1u;
	m_Rings.resize(count);
	for (uint32 i = 0; i < count; ++i)
	{
		const Float t = count > 1 ? static_cast<Float>(i) / (count - 1) : 0.f;
		RING& ring = m_Rings[i];
		ring.spreadTime = m_Desc.firstFall + (m_Desc.lastFall - m_Desc.firstFall) * t;
		ring.endRadius = m_Desc.mode == MODE::BURST ? m_Desc.firstRadius + (m_Desc.lastRadius - m_Desc.firstRadius) * t
			: m_Desc.endRadius;
	}

	if (m_Desc.mode == MODE::AREA && m_Desc.flame > 0.f)
		m_FlameImages = Find_Images(L"Effect_BlueFlame");
	if (Is_Burst())
		for (uint32 i = 0; i < PuffCount; ++i)
		{
			const Float around = static_cast<Float>(Helper::Random_Double(0.0, XM_2PI));
			// The first puff at the very centre, the rest anywhere out to PuffSpread.
			const Float out = i == 0 ? 0.f : static_cast<Float>(Helper::Random_Double(0.2, 1.0)) * PuffSpread * m_Desc.flame;
			m_Puffs.push_back({ Vector3{ cosf(around) * out, 0.f, sinf(around) * out },
				i == 0 ? 0.f : static_cast<Float>(Helper::Random_Double(0.0, PuffDelay)),
				static_cast<Float>(Helper::Random_Double(PuffMinRise, PuffMaxRise)),
				static_cast<Float>(Helper::Random_Double(PuffMinSize, PuffMaxSize)) * m_Desc.flame,
				static_cast<Float>(Helper::Random_Double(0.0, XM_2PI)) });
		}
	if (m_Desc.mode == MODE::AREA && m_Desc.flash > 0.f)
		m_FlareImages = Find_Images(L"Effect_Flare");
	if ((m_Desc.mode == MODE::GROUND || m_Desc.mode == MODE::AREA) && m_Desc.glitch > 0.f)
		GAME_INSTANCE->Start_ScreenGlitch(m_Desc.glitch, GlitchTime);
	m_FlashRoll = static_cast<Float>(Helper::Random_Double(0.0, XM_2PI));

	Shader::SHADER_DESC shaderDesc{ L"VTX_Mesh_Effect", VTXMESH::Elements, VTXMESH::numElements };
	m_Shader = Add_Component<Shader>(ETOI(LEVEL::STATIC), &shaderDesc);
	Texture::TEXTURE_DESC textureDesc{ Get_LevelIndex(), L"Effect_Em3000" };
	m_Texture = Add_Component<Texture>(Get_LevelIndex(), &textureDesc);
	if (!m_Shader || !m_Texture) return E_FAIL;

	D3D11_BUFFER_DESC bufferDesc{};
	bufferDesc.ByteWidth = sizeof(VTXMESH) * VerticesPerRing * count;
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	return m_Device->CreateBuffer(&bufferDesc, nullptr, m_VertexBuffer.GetAddressOf());
}

void EmRing::Update(Float timeDelta)
{
	m_Age += timeDelta;
	Bool running = false;
	for (uint32 i = 0; i < m_Rings.size(); ++i)
	{
		RING& ring = m_Rings[i];
		if (ring.done) continue;
		running = true;
		ring.age = m_Age - m_Desc.interval * i;
		if (ring.age >= 0.f) Update_Ring(ring, timeDelta);
	}
	if (!running)
		GAME_INSTANCE->Destroy(Get_ObjectGuid());
}

void EmRing::Update_Ring(RING& ring, Float timeDelta)
{
	if (m_Desc.mode == MODE::ROAR)
	{
		// Fast at first, slowing as it opens (as ShockRing's crest).
		const Float t = std::min(ring.age * m_Desc.spreadSpeed / std::max(ring.endRadius, 1e-3f), 1.f);
		const Float left = 1.f - t;
		ring.radius = ring.endRadius * (1.f - left * left * left);
		ring.opacity = std::min(ring.age / RoarFadeInTime, 1.f) * left * left;
		ring.done = t >= 1.f;
		return;
	}
	if (m_Desc.mode == MODE::AREA)
	{
		ring.radius = (std::min)(GroundStartRadius + m_Desc.spreadSpeed * ring.age, ring.endRadius);
		const Float spreadTime = (ring.endRadius - GroundStartRadius) / (std::max)(m_Desc.spreadSpeed, 1e-3f);
		ring.opacity = ring.age < spreadTime ? 1.f : 1.f - (ring.age - spreadTime) / AreaFadeTime;
		ring.wallHeight = GroundWallHeight;
		// The flame and the flash can outlast the lit disc.
		const Float hold = std::max(m_FlameImages.empty() ? 0.f : FlameTime + PuffDelay, m_FlareImages.empty() ? 0.f : FlashTime);
		if (ring.opacity <= 0.f)
		{
			ring.opacity = 0.f;
			ring.done = ring.age >= hold;
		}
		else if (ring.age <= AreaHitTime)
			Hit_Player(ring, 0.f, true);
		return;
	}
	if (m_Desc.mode == MODE::GROUND)
	{
		ring.radius = GroundStartRadius + m_Desc.spreadSpeed * ring.age;
		const Float fadeStart = ring.endRadius - m_Desc.spreadSpeed * GroundFadeTime;
		ring.opacity = ring.radius < fadeStart ? 1.f : 1.f - (ring.radius - fadeStart) / (ring.endRadius - fadeStart);
		ring.wallHeight = GroundWallHeight;
		if (ring.opacity <= 0.f)
			ring.done = true;
		else if (ring.opacity > 0.5f)
			Hit_Player(ring, Get_BandWidth() * 0.5f);
		return;
	}

	if (ring.burstAge < 0.f)
	{
		// The ring eases out to where it will burst while it first floats up over the head, easing to a stop, then
		// drops, faster and faster, onto the floor.
		const Float t = std::min(ring.age / std::max(ring.spreadTime, 1e-3f), 1.f);
		ring.radius = m_Desc.startRadius + (ring.endRadius - m_Desc.startRadius) * t * (2.f - t);
		if (t < RisePortion)
		{
			const Float u = t / RisePortion;
			ring.height = BurstStartHeight + RiseHeight * u * (2.f - u);
		}
		else
		{
			const Float u = (t - RisePortion) / (1.f - RisePortion);
			ring.height = (BurstStartHeight + RiseHeight) * (1.f - u * u);
		}
		ring.opacity = std::min(ring.age / FadeInTime, 1.f);
		if (t >= 1.f)
		{
			ring.burstAge = 0.f;
			Play_BurstSound(ring);
			if (m_Desc.glitch > 0.f)
				GAME_INSTANCE->Start_ScreenGlitch(m_Desc.glitch, GlitchTime);
		}
		return;
	}

	// Burst: the fire wall shoots up along the band and dies down.
	ring.burstAge += timeDelta;
	const Float burst = ring.burstAge / BurstTime;
	ring.wallHeight = BurstWallHeight * std::min(burst * 4.f, 1.f);
	ring.opacity = burst < 0.5f ? 1.f : 1.f - (burst - 0.5f) * 2.f;
	if (ring.burstAge <= BurstHitWindow) Hit_Player(ring, BandWidth * 0.5f + 0.3f);
	if (ring.burstAge >= BurstTime)
		ring.done = true;
}

void EmRing::Play_BurstSound(const RING& ring) const
{
	if (m_Desc.burstSound.empty()) return;
	// The volley spans the whole stage; the burst is heard from the part of the ring next to the player.
	Vector3 toward = Vector3::UnitX;
	if (const Shared<GameObject> target = m_Desc.target.lock())
	{
		const Vector3 feet = target->Get_Transform()->Get_Position();
		const Vector3 flat{ feet.x - m_Center.x, 0.f, feet.z - m_Center.z };
		if (flat.LengthSquared() > 1e-4f)
			flat.Normalize(toward);
	}
	GAME_INSTANCE->PlaySound3D(m_Desc.burstSound, BurstVolume, m_Center + toward * ring.radius, BurstSoundNear, BurstSoundFar, false);
}

void EmRing::Hit_Player(RING& ring, Float bandHalfWidth, Bool filled)
{
	if (ring.hasHit) return;
	const Shared<Entity> player = Cast<Entity>(m_Desc.target.lock());
	if (!player) return;

	const Vector3 feet = player->Get_Transform()->Get_Position();
	if (feet.y - m_Center.y > JumpClearance) return;
	const Float distance = Vector2{ feet.x - m_Center.x, feet.z - m_Center.z }.Length();
	if (filled ? distance > ring.endRadius + PlayerRadius : fabsf(distance - ring.radius) > bandHalfWidth + PlayerRadius) return;

	ring.hasHit = true;
	Entity::DAMAGE_INFO damage{};
	damage.attacker = shared_from_this();
	damage.attackType = ATK_TYPE::HEAVY;
	damage.damage = m_Desc.damage;
	damage.knockbackForce = 1.f;
	damage.hitPosition = feet;
	player->TakeDamage(damage);
}

void EmRing::Add_Draw(const Char* pass, uint32 image, uint32 first, uint32 end, Bool light,
	const ComPtr<ID3D11ShaderResourceView>& srv)
{
	if (end > first && m_NumDraws < m_Draws.size())
		m_Draws[m_NumDraws++] = { pass, image, first, end - first, light, srv };
}

vector<ComPtr<ID3D11ShaderResourceView>> EmRing::Find_Images(const wstring& tag) const
{
	vector<ComPtr<ID3D11ShaderResourceView>> images{};
	const Texture::TEXTURE_DESC* desc = GAME_INSTANCE->Get_TextureDesc(Get_LevelIndex(), tag);
	if (!desc) return images;
	for (uint32 i = 0; i < desc->m_NumSRVs; ++i)
	{
		tChar path[MAX_PATH] = TEXT("");
		swprintf_s(path, desc->m_FilePath.c_str(), i);
		const ComPtr<ID3D11ShaderResourceView>& srv = GAME_INSTANCE->Get_Texture(desc->m_levIndex, path);
		if (!srv) return {};
		images.push_back(srv);
	}
	return images;
}

void EmRing::Late_Update(Float timeDelta)
{
	m_NumDraws = 0;

	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (FAILED(m_Context->Map(m_VertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;
	auto* vertices = static_cast<VTXMESH*>(mapped.pData);
	uint32 used = 0;
	const auto put = [&](const Vector3 (&corners)[4], const Vector2 (&uvs)[4], const Vector3& lowTint, const Vector3& highTint,
		Float opacity)
	{
		constexpr uint32 order[6] = { 0, 2, 1, 1, 2, 3 };
		for (uint32 v = 0; v < 6; ++v)
		{
			VTXMESH& vertex = vertices[used++];
			vertex = {};
			vertex.position = corners[order[v]];
			vertex.normal = order[v] % 2 == 0 ? lowTint : highTint;
			vertex.texcoord = uvs[order[v]];
			vertex.texcoord1 = Vector2{ opacity, 0.f };
		}
	};

	// A point on a ring in direction d: upright layers stand on it (across = height over the floor), flat ones lie at its
	// height (across = distance from the centre).
	const auto at = [this](const RING& ring, Bool upright, const Vector3& d, Float across)
	{
		return upright ? m_Center + d * ring.radius + Vector3{ 0.f, across, 0.f }
			: m_Center + d * across + Vector3{ 0.f, ring.height + FloorLift, 0.f };
	};
	// One layer around a ring, across it from lo to hi with v from vLo to vHi. u runs along it, uScale per metre of arc, or
	// stays 0.5 (the middle column of a cross-section image) when uScale is 0.
	const auto strip = [&](const RING& ring, Bool upright, Float lo, Float hi, Float vLo, Float vHi, Float uScale,
		const Vector3& lowTint, const Vector3& highTint)
	{
		if (!upright) lo = std::max(lo, 0.f);
		for (uint32 i = 0; i < Segments; ++i)
		{
			const Float a0 = XM_2PI * i / Segments, a1 = XM_2PI * (i + 1) / Segments;
			const Vector3 d0{ sinf(a0), 0.f, cosf(a0) }, d1{ sinf(a1), 0.f, cosf(a1) };
			const Float u0 = uScale > 0.f ? ring.radius * a0 * uScale : 0.5f;
			const Float u1 = uScale > 0.f ? ring.radius * a1 * uScale : 0.5f;
			const Vector3 corners[4] = { at(ring, upright, d0, lo), at(ring, upright, d0, hi), at(ring, upright, d1, lo),
				at(ring, upright, d1, hi) };
			const Vector2 uvs[4] = { { u0, vLo }, { u0, vHi }, { u1, vLo }, { u1, vHi } };
			put(corners, uvs, lowTint, highTint, ring.opacity);
		}
	};
	// A layer as a line across the ring, upright and flat, `width` wide around its height and its radius.
	const auto line = [&](const RING& ring, Float width, const Vector3& tint)
	{
		const Float y = ring.height + FloorLift, half = width * 0.5f;
		strip(ring, true, y - half, y + half, 0.f, 1.f, 0.f, tint, tint);
		strip(ring, false, ring.radius - half, ring.radius + half, 0.f, 1.f, 0.f, tint, tint);
	};

	if (m_Desc.mode == MODE::ROAR)
	{
		// An annulus facing the camera round the centre: u along it, v across it (0 inside, 1 outside), widening as it opens.
		const Matrix camera = GAME_INSTANCE->Get_Transform(D3DTS::VIEW).Invert();
		const Vector3 cameraRight = camera.Right(), cameraUp = camera.Up();
		const uint32 first = used;
		for (const RING& ring : m_Rings)
		{
			if (!ring.Is_Shown()) continue;
			const Float open = ring.radius / std::max(ring.endRadius, 1e-3f);
			const Float half = Get_BandWidth() * (RoarStartWidth + (1.f - RoarStartWidth) * open) * 0.5f;
			const Float lo = std::max(ring.radius - half, 0.f), hi = ring.radius + half;
			const Vector3 strength{ m_Desc.haze * ring.opacity, 0.f, 0.f };
			for (uint32 i = 0; i < Segments; ++i)
			{
				const Float a0 = XM_2PI * i / Segments, a1 = XM_2PI * (i + 1) / Segments;
				const Vector3 d0 = cameraRight * sinf(a0) + cameraUp * cosf(a0), d1 = cameraRight * sinf(a1) + cameraUp * cosf(a1);
				const Vector3 corners[4] = { m_Center + d0 * lo, m_Center + d0 * hi, m_Center + d1 * lo, m_Center + d1 * hi };
				const Float u0 = static_cast<Float>(i) / Segments, u1 = static_cast<Float>(i + 1) / Segments;
				const Vector2 uvs[4] = { { u0, 0.f }, { u0, 1.f }, { u1, 0.f }, { u1, 1.f } };
				put(corners, uvs, strength, strength, 1.f);
			}
		}
		Add_Draw("RoarRing", RippleImage, first, used, false);
	}
	else if (m_Desc.mode == MODE::BURST)
	{
		// Every ring's heat haze (around it, and over its fire wall once it bursts) before any ring's light.
		uint32 first = used;
		for (const RING& ring : m_Rings)
		{
			if (!ring.Is_Shown()) continue;
			const Vector3 haze{ HazeStrength * ring.opacity, 0.f, 0.f };
			const Float y = ring.height + FloorLift, half = HazeWidth * 0.5f;
			strip(ring, true, y - half, y + half, 0.f, 1.f, HazeTiling, haze, haze);
			strip(ring, false, ring.radius - half, ring.radius + half, 0.f, 1.f, HazeTiling, haze, haze);
			if (ring.wallHeight > 0.f)
				strip(ring, true, 0.f, ring.wallHeight * 1.3f, 0.5f, 1.f, HazeTiling, haze, haze);
		}
		Add_Draw("RingHaze", HazeImage, first, used, false);

		// The ripples running out over the floor from where the rings burst, easing out as they go and fading.
		first = used;
		for (const RING& ring : m_Rings)
		{
			if (!ring.Is_Shown() || ring.burstAge < 0.f) continue;
			const Float left = 1.f - std::min(ring.burstAge / BurstTime, 1.f);
			const Float radius = ring.radius + BurstRippleSpread * (1.f - left * left * left), half = BurstRippleWidth * 0.5f;
			const Vector3 strength{ BurstRippleStrength * left * left, 0.f, 0.f };
			strip(ring, false, radius - half, radius + half, 0.f, 1.f, 0.f, strength, strength);
		}
		Add_Draw("RoarRing", RippleImage, first, used, false);

		// The rings: the deep-red glow blended over the scene, then the brighter band and the hot core added over it.
		first = used;
		for (const RING& ring : m_Rings)
			if (ring.Is_Shown()) line(ring, RingGlowWidth, RingGlowTint);
		Add_Draw("SoftBeamBlend", HazeImage, first, used);
		first = used;
		for (const RING& ring : m_Rings)
		{
			if (!ring.Is_Shown()) continue;
			line(ring, RingBandWidth, RingBandTint);
			line(ring, RingCoreWidth, RingCoreTint);
		}
		Add_Draw("SoftBeam", HazeImage, first, used);

		// Lightning crackling along them: one bolt image a frame, laid in squares around each ring, upright and flat. Each
		// swap shifts the squares and flips some, so the bolts jump about instead of repeating; a ring flashes as it is born.
		first = used;
		const uint32 frame = static_cast<uint32>(m_Age * LightningRate);
		for (uint32 i = 0; i < m_Rings.size(); ++i)
		{
			const RING& ring = m_Rings[i];
			if (!ring.Is_Shown()) continue;
			const uint32 tiles = std::clamp(static_cast<uint32>(XM_2PI * ring.radius / LightningSize), 6u, MaxLightningTiles);
			const Float phase = fmodf(frame * 0.618f + i * 0.37f, 1.f);
			const Float half = LightningSize * 0.5f, y = ring.height + FloorLift;
			const Vector3 bolt = LightningTint * (1.f + BirthFlash * expf(-ring.age * 6.f));
			for (uint32 k = 0; k < tiles; ++k)
			{
				const Float a0 = XM_2PI * (k + phase) / tiles, a1 = XM_2PI * (k + 1 + phase) / tiles;
				const Vector3 d0{ sinf(a0), 0.f, cosf(a0) }, d1{ sinf(a1), 0.f, cosf(a1) };
				const Float u0 = (k * 7 + frame) % 3 == 0 ? 1.f : 0.f, v0 = (k + frame) % 2 == 0 ? 1.f : 0.f;
				const Vector2 uvs[4] = { { u0, v0 }, { u0, 1.f - v0 }, { 1.f - u0, v0 }, { 1.f - u0, 1.f - v0 } };
				for (const Bool upright : { true, false })
				{
					const Float lo = upright ? y - half : ring.radius - half, hi = upright ? y + half : ring.radius + half;
					const Vector3 corners[4] = { at(ring, upright, d0, lo), at(ring, upright, d0, hi), at(ring, upright, d1, lo),
						at(ring, upright, d1, hi) };
					put(corners, uvs, bolt, bolt, ring.opacity);
				}
			}
		}
		Add_Draw("Lightning", LightningFirst + frame % LightningImages, first, used);
	}
	else
	{
		// The haze first (it repaints the screen copy): over the floor round the band and up off it, as on BURST's walls.
		if (m_Desc.haze > 0.f)
		{
			const uint32 hazeFirst = used;
			const Float half = Get_GlowWidth() * GroundHazeSpread * 0.5f;
			for (const RING& ring : m_Rings)
			{
				if (!ring.Is_Shown()) continue;
				const Vector3 haze{ m_Desc.haze * ring.opacity, 0.f, 0.f };
				strip(ring, false, ring.radius - half, ring.radius + half, 0.f, 1.f, HazeTiling, haze, haze);
				strip(ring, true, 0.f, GroundHazeHeight, 0.5f, 1.f, HazeTiling, haze, haze);
			}
			Add_Draw("RingHaze", HazeImage, hazeFirst, used, false);
		}

		// Flat band and its glow: v runs across the ring, so the glow image's middle column shapes the cross-section. AREA
		// lights the floor inside it too, brightening out to the rim (v held on the glow image's bright middle). A burst
		// lights no floor.
		const uint32 first = used;
		for (const RING& ring : m_Rings)
		{
			if (!ring.Is_Shown() || Is_Burst()) continue;
			if (m_Desc.mode == MODE::AREA)
				strip(ring, false, 0.f, ring.radius, 0.5f, 0.5f, 0.f, m_Desc.tint * (DiscBrightness / 3.f), m_Desc.tint * DiscBrightness);
			for (const auto& [width, brightness] : { std::pair{ Get_BandWidth(), BandBrightness }, std::pair{ Get_GlowWidth(), GlowBrightness } })
			{
				const Vector3 tint = m_Desc.tint * brightness;
				strip(ring, false, ring.radius - width * 0.5f, ring.radius + width * 0.5f, 0.f, 1.f, 0.f, tint, tint);
			}
		}
		Add_Draw("AdditiveWrap", GlowImage, first, used);

		// AREA's burst: the puffs of 336 facing the camera, each on its own frame (its own draw) as it rises and grows.
		const Matrix camera = GAME_INSTANCE->Get_Transform(D3DTS::VIEW).Invert();
		const Vector3 cameraRight = camera.Right(), cameraUp = camera.Up(), cameraPosition = camera.Translation();
		// A square facing the camera, turned by roll.
		const auto facing = [&](const Vector3& center, Float size, Float roll, const Vector3& tint, Float opacity)
		{
			const Float h = size * 0.5f, c = cosf(roll) * h, s = sinf(roll) * h;
			const Vector3 right = cameraRight * c + cameraUp * s, up = cameraUp * c - cameraRight * s;
			const Vector3 corners[4] = { center - right - up, center - right + up, center + right - up, center + right + up };
			const Vector2 uvs[4] = { { 0.f, 1.f }, { 0.f, 0.f }, { 1.f, 1.f }, { 1.f, 0.f } };
			put(corners, uvs, tint, tint, opacity);
		};
		for (const RING& ring : m_Rings)
		{
			if (!ring.Is_Shown() || !Is_Burst()) continue;
			for (const PUFF& puff : m_Puffs)
			{
				const Float age = ring.age - puff.delay;
				if (age < 0.f || age >= FlameTime) continue;
				const Float t = age / FlameTime;
				const uint32 frame = std::min(static_cast<uint32>(t * m_FlameImages.size()), static_cast<uint32>(m_FlameImages.size()) - 1);
				const Float size = puff.size * (0.5f + 0.5f * std::min(t * 3.f, 1.f));
				const uint32 puffFirst = used;
				facing(m_Center + puff.offset + Vector3{ 0.f, size * 0.4f + puff.rise * age, 0.f }, size, puff.roll, FlameTint, 1.f);
				Add_Draw("Additive", 0, puffFirst, used, true, m_FlameImages[frame]);
			}
		}

		// AREA's flash at the centre: each layer of Effect_Flare its own draw.
		for (const RING& ring : m_Rings)
		{
			if (!ring.Is_Shown() || m_FlareImages.size() < 5 || ring.age >= FlashTime) continue;
			const Float t = ring.age / FlashTime;
			const Float pop = std::min(t / FlashPop, 1.f);
			const Float fade = t < FlashPop ? 1.f : (1.f - (t - FlashPop) / (1.f - FlashPop)) * (1.f - (t - FlashPop) / (1.f - FlashPop));
			const Float size = m_Desc.flash * (0.5f + 0.5f * pop);
			Vector3 center = m_Center + Vector3{ 0.f, FlashHeight, 0.f };
			Vector3 toCamera = cameraPosition - center;
			const Float distance = toCamera.Length();
			if (distance > 0.001f)
				center += toCamera / distance * std::min(size * 0.5f, distance * 0.5f);
			// image, size, turn, brightness: the halo opens out as it fades, the glint is the sharpest and goes first.
			const struct { uint32 image; Float size, roll, brightness; } layers[]{
				{ 0, size, m_FlashRoll, 0.6f },
				{ 1, size * 0.8f, m_FlashRoll * 2.f, 1.f },
				{ 2, size * (0.8f + 0.6f * t), m_FlashRoll * 3.f, 0.8f },
				{ 3, size * 1.2f, 0.f, 1.5f * (1.f - t) },
				{ 4, size * 0.3f, 0.f, 1.2f },
			};
			for (const auto& layer : layers)
			{
				const uint32 flashFirst = used;
				facing(center, layer.size, layer.roll, m_Desc.tint * (FlashBrightness * layer.brightness), fade);
				Add_Draw("Additive", 0, flashFirst, used, true, m_FlareImages[layer.image]);
			}
		}
	}

	// The fire walls standing on the bands, fading out toward their tops; the noise tiles around each ring and scrolls up.
	const uint32 wallsFirst = used;
	for (const RING& ring : m_Rings)
	{
		if (!ring.Is_Shown() || ring.wallHeight <= 0.f || Is_Burst()) continue;
		const Float repeats = std::max(1.f, roundf(ring.radius * 0.8f));
		const Float scroll = ring.age * 1.5f;
		strip(ring, true, 0.f, ring.wallHeight, 1.f + scroll, scroll, repeats / (XM_2PI * ring.radius),
			m_Desc.tint * FireBrightness, Vector3::Zero);
	}
	Add_Draw("AdditiveWrap", FireImage, wallsFirst, used);
	m_Context->Unmap(m_VertexBuffer.Get(), 0);
}

HRESULT EmRing::Render()
{
	if (m_NumDraws == 0) return S_OK;
	const Matrix identity = Matrix::Identity;
	if (FAILED(m_Shader->Bind_Matrix(WorldMatrix, &identity)) ||
		FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ViewMatrix, D3DTS::VIEW)) ||
		FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ProjMatrix, D3DTS::PROJ)) ||
		FAILED(GAME_INSTANCE->Bind_RenderTarget_ShaderResource(m_Shader, SceneMap, RT_SCENE)) ||
		FAILED(m_Shader->Bind_RawValue("g_EffectTime", &m_Age, sizeof(Float)))) return E_FAIL;

	ID3D11Buffer* buffer = m_VertexBuffer.Get();
	const uint32 stride = sizeof(VTXMESH), offset = 0;
	m_Context->IASetVertexBuffers(0, 1, &buffer, &stride, &offset);
	m_Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	const Bool emissive = GAME_INSTANCE->Get_DrawingGroup() == RENDERGROUP::EMISSIVE;
	for (uint32 i = 0; i < m_NumDraws; ++i)
	{
		const DRAW& draw = m_Draws[i];
		if ((draw.light && m_Desc.glow) != emissive) continue;
		if (FAILED(draw.srv ? m_Shader->Bind_SRV("g_EffectMap", draw.srv) : m_Texture->Bind_ShaderResourceView(m_Shader, "g_EffectMap", draw.image)) ||
			FAILED(m_Shader->Begin(draw.pass))) return E_FAIL;
		m_Context->Draw(draw.count, draw.first);
	}
	return S_OK;
}

void EmRing::Submit_RenderGroup()
{
	// The haze reads the lit screen copied before this group; with glow the light goes to the emissive target.
	GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::DISTORTION, shared_from_this());
	if (m_Desc.glow)
		GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::EMISSIVE, shared_from_this());
}

Shared<EmRing> EmRing::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<EmRing>(device, context);
	return SUCCEEDED(prototype->Initialize_Prototype()) ? prototype : nullptr;
}

Shared<GameObject> EmRing::Clone(void* arg)
{
	auto instance = make_shared<EmRing>(*this);
	return SUCCEEDED(instance->Initialize(arg)) ? instance : nullptr;
}
