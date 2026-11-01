#include "pch.h"
#include "EmLaser.h"

#include <Game.h>
#include <Shader.h>
#include <Texture.h>

#include "Entity.h"
#include "Monster.h"

EmLaser::EmLaser(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: GameObject{ device, context } {}
EmLaser::EmLaser(const EmLaser& rhs) : GameObject{ rhs } {}
EmLaser::~EmLaser() { Stop_ShotSound(); }

HRESULT EmLaser::Initialize_Prototype()
{
	// Pl0000::TryEvade refuses a dash evasion when the attacker is on the Bullet layer, so this attack hands itself in as
	// the attacker: it has to be avoided by moving or jumping, not dodged.
	m_LayerMask.Set_Layer(L"Bullet");
	return GameObject::Initialize_Prototype();
}

HRESULT EmLaser::Initialize(void* arg)
{
	if (FAILED(GameObject::Initialize(arg))) return E_FAIL;

	Shader::SHADER_DESC shaderDesc{ L"VTX_Mesh_Effect", VTXMESH::Elements, VTXMESH::numElements };
	m_Shader = Add_Component<Shader>(ETOI(LEVEL::STATIC), &shaderDesc);
	Texture::TEXTURE_DESC textureDesc{ Get_LevelIndex(), L"Effect_Em3000" };
	m_Texture = Add_Component<Texture>(Get_LevelIndex(), &textureDesc);
	if (!m_Shader || !m_Texture) return E_FAIL;

	D3D11_BUFFER_DESC bufferDesc{};
	bufferDesc.ByteWidth = sizeof(VTXMESH) * VertexCapacity;
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	return m_Device->CreateBuffer(&bufferDesc, nullptr, m_VertexBuffer.GetAddressOf());
}

void EmLaser::Fire()
{
	const Shared<GameObject> owner = Get_Parent();
	if (!owner) return;
	const Vector3 look = owner->Get_Transform()->Get_Look();
	m_StartYaw = atan2f(look.x, look.z);
	m_Center = owner->Get_Transform()->Get_Position();
	m_Aimed = false;
	m_Age = 0.f;
	m_HitCooldown = 0.f;
	Stop_ShotSound();
	GAME_INSTANCE->PlaySound3D(L"em3000_laser_tele", SoundVolume, m_Center, SoundNear, SoundFar, false);
}

void EmLaser::Fire_Aimed(const Vector3& origin, const Vector3& direction, Float floorY, Float width, Float length)
{
	m_Aimed = true;
	m_FloorY = floorY;
	m_AimedWidth = width;
	m_AimedLength = length;
	Set_Aim(origin, direction);
	m_Age = 0.f;
	m_HitCooldown = 0.f;
	Stop_ShotSound();
	GAME_INSTANCE->PlaySound3D(L"em3000_laser_tele", SoundVolume, m_Center, SoundNear, SoundFar, false);
}

void EmLaser::Set_Aim(const Vector3& origin, const Vector3& direction)
{
	m_Center = origin;
	m_AimDirection = direction;
	m_AimDirection.Normalize();
}

void EmLaser::Stop()
{
	m_Age = -1.f;
	Stop_ShotSound();
}

void EmLaser::Stop_ShotSound()
{
	if (m_ShotSound == 0) return;
	GAME_INSTANCE->StopLoopSound3D(m_ShotSound);
	m_ShotSound = 0;
}

Float EmLaser::Get_Sweep() const
{
	return m_Aimed ? 0.f : XM_2PI * std::clamp((m_Age - WarnTime) / SweepTime, 0.f, 1.f);
}

Float EmLaser::Get_BeamLength() const
{
	if (m_Aimed)
	{
		// Nothing shows while the muzzle charges; then the beam shoots out to the floor it points at and on along it.
		const Float t = std::clamp((m_Age - AimedChargeTime) / FireGrowTime, 0.f, 1.f);
		return m_AimedLength * t * (2.f - t);
	}
	// The guide lines grow out from the skirt, and the beams shoot out over them when they fire.
	const Bool warning = m_Age < WarnTime;
	const Float grow = warning ? m_Age / GuideGrowTime : (m_Age - WarnTime) / FireGrowTime;
	const Float t = std::clamp(grow, 0.f, 1.f);
	return BeamLength * t * (2.f - t);
}

Float EmLaser::Get_FloorReach() const
{
	return m_AimDirection.y < -0.01f ? (std::max)((m_Center.y - m_FloorY) / -m_AimDirection.y, 0.f) : m_AimedLength;
}

void EmLaser::Trace_GroundPath(Float length)
{
	m_GroundPath.clear();
	const Float reach = Get_FloorReach();
	if (length <= reach) return;

	Vector3 flat{ m_AimDirection.x, 0.f, m_AimDirection.z };
	flat.Normalize();
	Vector3 point = m_Center + m_AimDirection * reach;
	point.y = m_FloorY;
	m_GroundPath.push_back(point);
	const uint32 steps = std::clamp(static_cast<uint32>(ceilf((length - reach) / GroundStep)), 1u, MaxGroundSteps);
	const Float step = (length - reach) / steps;
	for (uint32 k = 0; k < steps; ++k)
	{
		Vector3 next = point + flat * step, hit{};
		if (GAME_INSTANCE->Raycast_Physics(Vector3{ next.x, point.y + GroundProbe, next.z }, -Vector3::UnitY, GroundProbe * 2.f, hit))
		{
			if (hit.y - point.y > step * WallSlope) return;
			next.y = hit.y;
		}
		m_GroundPath.push_back(next);
		point = next;
	}
}

void EmLaser::Update(Float timeDelta)
{
	if (m_Age < 0.f) return;
	const Float warnTime = Get_WarnTime();
	const Bool firing = m_Age >= warnTime;
	m_Age += timeDelta;
	// The aimed beam burns until its owner stops it; the sweep ends on its own.
	if (!m_Aimed && m_Age >= WarnTime + SweepTime + FadeTime)
	{
		Stop();
		return;
	}
	if (const Shared<GameObject> owner = Get_Parent(); owner && !m_Aimed)
		m_Center = owner->Get_Transform()->Get_Position();
	// The shot plays as a loop only so it can be cut off when the beams go out; it is longer than the sweep.
	if (!firing && m_Age >= warnTime)
		m_ShotSound = GAME_INSTANCE->PlaySound3D(L"em3000_laser_shot", SoundVolume, m_Center, SoundNear, SoundFar, true);
	// Glitch once as the beams fire; let it fade while the beams keep burning.
	if (!firing && m_Age >= warnTime)
		GAME_INSTANCE->Start_ScreenGlitch(FireGlitch, FireGlitchTime);

	m_HitCooldown -= timeDelta;
	if (m_HitCooldown > 0.f || m_Age < warnTime) return;
	if (m_Aimed)
		Hit_PlayerAimed();
	else if (m_Age < WarnTime + SweepTime)
		Hit_Player();
}

void EmLaser::Hit_Player()
{
	const Shared<Monster> owner = Cast<Monster>(Get_Parent());
	const Shared<Entity> player = owner ? Cast<Entity>(owner->Get_Target()) : nullptr;
	if (!player) return;

	const Vector3 feet = player->Get_Transform()->Get_Position();
	if (feet.y - m_Center.y > JumpClearance) return;
	const Vector2 offset{ feet.x - m_Center.x, feet.z - m_Center.z };
	const Float distance = offset.Length();
	if (distance < InnerRadius - PlayerRadius || distance > InnerRadius + Get_BeamLength() + PlayerRadius) return;

	const Float playerYaw = atan2f(offset.x, offset.y);
	const Float sweep = Get_Sweep();
	for (uint32 i = 0; i < NumBeams; ++i)
	{
		const Float beamYaw = m_StartYaw + sweep + XM_2PI * i / NumBeams;
		const Float apart = remainderf(playerYaw - beamYaw, XM_2PI);
		if (cosf(apart) <= 0.f || distance * fabsf(sinf(apart)) > BeamWidth * 0.5f + PlayerRadius) continue;

		Entity::DAMAGE_INFO damage{};
		damage.attacker = shared_from_this();
		damage.attackType = ATK_TYPE::LIGHT;
		damage.damage = Damage;
		damage.hitPosition = feet + Vector3{ 0.f, BeamHeight, 0.f };
		player->TakeDamage(damage);
		m_HitCooldown = HitInterval;
		return;
	}
}

void EmLaser::Hit_PlayerAimed()
{
	const Shared<Monster> owner = Cast<Monster>(Get_Parent());
	const Shared<Entity> player = owner ? Cast<Entity>(owner->Get_Target()) : nullptr;
	if (!player) return;

	const Vector3 feet = player->Get_Transform()->Get_Position();
	const Float length = Get_BeamLength(), reach = (std::min)(Get_FloorReach(), length);
	const Float hitRadius = m_AimedWidth * 0.5f + PlayerRadius;
	Entity::DAMAGE_INFO damage{};
	damage.attacker = shared_from_this();
	damage.attackType = ATK_TYPE::LIGHT;
	damage.damage = Damage;

	// Over the ground past where it lands, a grounded player within its width is hit; a jump clears it.
	for (size_t k = 1; k < m_GroundPath.size(); ++k)
	{
		const Vector3 from = m_GroundPath[k - 1], piece = m_GroundPath[k] - from;
		const Vector2 flatPiece{ piece.x, piece.z }, toFeet{ feet.x - from.x, feet.z - from.z };
		const Float s = std::clamp(toFeet.Dot(flatPiece) / (std::max)(flatPiece.LengthSquared(), 1e-6f), 0.f, 1.f);
		const Vector3 nearest = from + piece * s;
		if (feet.y - nearest.y <= JumpClearance && Vector2{ feet.x - nearest.x, feet.z - nearest.z }.Length() <= hitRadius)
		{
			damage.hitPosition = feet + Vector3{ 0.f, BeamHeight, 0.f };
			player->TakeDamage(damage);
			m_HitCooldown = HitInterval;
			return;
		}
	}

	// Down from the eye: the closest points of the beam and of the player's body (a vertical segment up from the feet),
	// clamped to both.
	const Vector3 beam = m_AimDirection * reach;
	const Vector3 body{ 0.f, PlayerHeight, 0.f };
	const Vector3 gap = m_Center - feet;
	const Float a = beam.Dot(beam), e = body.Dot(body), b = beam.Dot(body);
	const Float c = beam.Dot(gap), f = body.Dot(gap);
	const Float denominator = a * e - b * b;
	Float s = denominator > 1e-6f ? std::clamp((b * f - c * e) / denominator, 0.f, 1.f) : 0.f;
	const Float t = std::clamp((b * s + f) / e, 0.f, 1.f);
	if (a > 1e-6f)
		s = std::clamp((b * t - c) / a, 0.f, 1.f);
	const Vector3 onBody = feet + body * t;
	if (Vector3::Distance(m_Center + beam * s, onBody) > hitRadius) return;

	damage.hitPosition = onBody;
	player->TakeDamage(damage);
	m_HitCooldown = HitInterval;
}

void EmLaser::Add_Draw(const Char* pass, uint32 image, uint32 first, uint32 end)
{
	if (end > first && m_NumDraws < m_Draws.size())
		m_Draws[m_NumDraws++] = { pass, image, first, end - first };
}

void EmLaser::Late_Update(Float timeDelta)
{
	m_NumDraws = 0;
	if (m_Age < 0.f) return;

	const Float warnTime = Get_WarnTime();
	const Bool warning = m_Age < warnTime;
	Float opacity = warning ? std::min(m_Age / 0.15f, 1.f) : 1.f;
	if (!m_Aimed && m_Age > WarnTime + SweepTime)
		opacity = 1.f - (m_Age - WarnTime - SweepTime) / FadeTime;

	const Vector3 camera = GAME_INSTANCE->Get_Transform(D3DTS::VIEW).Invert().Translation();
	const Float sweep = Get_Sweep();
	const Float length = Get_BeamLength();
	const Float widthScale = Get_WidthScale();

	// Each beam's start and direction; side lies flat for the guide lines on the floor, facing turns about the beam to
	// face the camera (the beams, the bolts and the haze).
	struct BEAM { Vector3 start, axis, side; };
	const auto facing = [&camera](BEAM beam, Float along)
	{
		beam.side = beam.axis.Cross(camera - (beam.start + beam.axis * along));
		if (beam.side.LengthSquared() < 1e-6f) beam.side = beam.axis.Cross(Vector3::Up);
		beam.side.Normalize();
		return beam;
	};
	// Each drawn piece runs `lengths` metres from its start, `alongs` metres down the whole beam, so the aimed beam's
	// fade and noise run on from its slant down to the floor into its run along it.
	array<BEAM, NumBeams> guides{}, beams{};
	array<Float, NumBeams> lengths{}, alongs{};
	lengths.fill(length);
	uint32 beamCount = m_Aimed ? 1u : NumBeams;
	if (m_Aimed)
	{
		const Float reach = (std::min)(Get_FloorReach(), length);
		lengths[0] = reach;
		beams[0] = facing({ m_Center, m_AimDirection, Vector3::Zero }, reach * 0.5f);
		Trace_GroundPath(length);
		for (size_t k = 1; k < m_GroundPath.size(); ++k)
		{
			const Vector3 lift{ 0.f, BeamHeight, 0.f };
			Vector3 axis = m_GroundPath[k] - m_GroundPath[k - 1];
			lengths[beamCount] = axis.Length();
			axis.Normalize();
			alongs[beamCount] = alongs[beamCount - 1] + lengths[beamCount - 1];
			beams[beamCount] = facing({ m_GroundPath[k - 1] + lift, axis, Vector3::Zero }, lengths[beamCount] * 0.5f);
			++beamCount;
		}
	}
	else
		for (uint32 i = 0; i < NumBeams; ++i)
		{
			const Float yaw = m_StartYaw + sweep + XM_2PI * i / NumBeams;
			const Vector3 axis{ sinf(yaw), 0.f, cosf(yaw) };
			const Vector3 start = m_Center + axis * InnerRadius;
			Vector3 flat = axis.Cross(Vector3::Up);
			flat.Normalize();
			guides[i] = { start + Vector3{ 0.f, GuideHeight, 0.f }, axis, flat };
			beams[i] = facing({ start + Vector3{ 0.f, BeamHeight, 0.f }, axis, flat }, length * 0.5f);
		}

	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (FAILED(m_Context->Map(m_VertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;
	auto* vertices = static_cast<VTXMESH*>(mapped.pData);
	uint32 used = 0;
	// A strip along a beam from `from` to `to` metres out: u runs from uFrom to uTo along it, v from 0 to 1 across it.
	const auto strip = [&](const BEAM& beam, Float from, Float to, Float width, Float uFrom, Float uTo,
		const Vector3& tintFrom, const Vector3& tintTo)
	{
		const Vector3 a = beam.start + beam.axis * from, b = beam.start + beam.axis * to;
		const Float half = width * 0.5f;
		const Vector3 corners[4] = { a + beam.side * half, a - beam.side * half, b + beam.side * half, b - beam.side * half };
		const Vector2 uvs[4] = { { uFrom, 0.f }, { uFrom, 1.f }, { uTo, 0.f }, { uTo, 1.f } };
		constexpr uint32 order[6] = { 0, 2, 1, 1, 2, 3 };
		for (uint32 v = 0; v < 6; ++v)
		{
			VTXMESH& vertex = vertices[used++];
			vertex = {};
			vertex.position = corners[order[v]];
			vertex.normal = order[v] < 2 ? tintFrom : tintTo;
			vertex.texcoord = uvs[order[v]];
			vertex.texcoord1 = Vector2{ opacity, 0.f };
		}
	};

	// Lightning: one bolt image a frame, laid in squares that shift and flip at every swap so the bolts jump about.
	const uint32 frame = static_cast<uint32>(m_Age * LightningRate);
	const Float shift = fmodf(frame * 0.618f, 1.f);
	const auto bolts = [&](const BEAM& beam, uint32 seed, Float size, uint32 count, Float boltLength, const auto& tintAt)
	{
		for (uint32 k = 0; k < count; ++k)
		{
			const Float from = std::max(size * (shift + static_cast<Float>(k) - 1.f), 0.f);
			const Float to = std::min(size * (shift + static_cast<Float>(k)), boltLength);
			if (to <= from) continue;
			const Bool flip = (seed * 5 + k * 7 + frame) % 3 == 0;
			strip(beam, from, to, size, flip ? 1.f : 0.f, flip ? 0.f : 1.f, tintAt(from), tintAt(to));
		}
	};
	// The muzzles crackle where the beams leave the skirt, charging up through the wind-up and flickering at full strength
	// while the beams fire: two camera-facing bolts over each beam's start, one large and one smaller.
	const Float charge = warning ? MuzzleChargeFrom + (1.f - MuzzleChargeFrom) * m_Age / warnTime : 1.f;
	const uint32 muzzleCount = m_Aimed ? 1u : NumBeams;
	const auto muzzles = [&]()
	{
		for (uint32 i = 0; i < muzzleCount; ++i)
		{
			const BEAM muzzle = facing(beams[i], 0.f);
			const Float flicker = 0.6f + 0.4f * static_cast<Float>((i * 3 + frame) % 4) / 3.f;
			for (const auto& [size, flip] : { std::pair{ MuzzleBoltSize * widthScale, (i + frame) % 2 == 0 },
				std::pair{ MuzzleBoltSize * widthScale * 0.6f, (i + frame) % 2 != 0 } })
			{
				const Vector3 tint = MuzzleTint * (charge * flicker);
				strip(muzzle, -size * 0.5f, size * 0.5f, size, flip ? 1.f : 0.f, flip ? 0.f : 1.f, tint, tint);
			}
		}
	};

	if (warning)
	{
		// Heat already shimmers over the charging muzzles.
		const Vector3 haze{ HazeStrength * charge * opacity, 0.f, 0.f };
		const Float hazeSize = MuzzleHazeSize * widthScale;
		for (uint32 i = 0; i < muzzleCount; ++i)
		{
			const BEAM muzzle = facing(beams[i], 0.f);
			strip(muzzle, -hazeSize * 0.5f, hazeSize * 0.5f, hazeSize, 0.f, hazeSize * HazeTiling, haze, haze);
		}
		Add_Draw("RingHaze", HazeImage, 0, used);

		// The guide lines: thin glowing red lines on the floor, growing out from the skirt. The aimed beam has none, only
		// its charging muzzle.
		const uint32 guidesFirst = used;
		if (!m_Aimed)
			for (const BEAM& guide : guides)
				strip(guide, 0.f, length, GuideWidth, 0.f, 1.f, GuideTint, GuideTint);
		Add_Draw("SoftBeam", HazeImage, guidesFirst, used);

		// Sparks crackling along them, standing up to face the camera, growing stronger as the laser charges, then the
		// muzzles.
		const uint32 boltsFirst = used;
		if (!m_Aimed)
			for (uint32 i = 0; i < NumBeams; ++i)
				bolts(facing(guides[i], length * 0.5f), i, GuideBoltSize, MaxGuideBoltsPerBeam, length,
					[charge](Float) { return GuideBoltTint * charge; });
		muzzles();
		Add_Draw("Lightning", LightningFirst + frame % LightningImages, boltsFirst, used);
	}
	else
	{
		// Heat haze over each beam, weakening toward its far end.
		const Vector3 haze{ HazeStrength * opacity, 0.f, 0.f };
		const auto weaken = [length](Float along) { return length > 0.f ? 1.f - along / length : 0.f; };
		for (uint32 i = 0; i < beamCount; ++i)
			strip(beams[i], 0.f, lengths[i], HazeWidth * widthScale, alongs[i] * HazeTiling, (alongs[i] + lengths[i]) * HazeTiling,
				haze * weaken(alongs[i]), haze * weaken(alongs[i] + lengths[i]));
		Add_Draw("RingHaze", HazeImage, 0, used);
		// A narrow deep-red glow blended over the floor (added red would turn orange on the grass), then a brighter red
		// band and the hot core added over it. u runs over the whole beam, so its far end fades only once.
		const auto u = [length](Float along) { return length > 0.f ? along / length : 0.f; };
		const uint32 glowFirst = used;
		for (uint32 i = 0; i < beamCount; ++i)
			strip(beams[i], 0.f, lengths[i], GlowWidth * widthScale, u(alongs[i]), u(alongs[i] + lengths[i]), GlowTint, GlowTint);
		Add_Draw("SoftBeamBlend", HazeImage, glowFirst, used);
		const uint32 beamsFirst = used;
		for (const auto& [width, tint] : { std::pair{ BandWidth, BandTint }, std::pair{ CoreWidth, CoreTint } })
			for (uint32 i = 0; i < beamCount; ++i)
				strip(beams[i], 0.f, lengths[i], width * widthScale, u(alongs[i]), u(alongs[i] + lengths[i]), tint, tint);
		Add_Draw("SoftBeam", HazeImage, beamsFirst, used);

		// Bolts along each beam, fading out with its far end (SoftBeam fades over its last 45%), then the muzzles.
		const uint32 boltsFirst = used;
		for (uint32 i = 0; i < beamCount; ++i)
		{
			const Float along0 = alongs[i];
			const auto fade = [length, along0](Float along) {
				return LightningTint * std::clamp((1.f - (along0 + along) / length) / 0.45f, 0.f, 1.f); };
			bolts(beams[i], i, LightningSize * widthScale, MaxBoltsPerBeam, lengths[i], fade);
		}
		muzzles();
		Add_Draw("Lightning", LightningFirst + frame % LightningImages, boltsFirst, used);
	}
	m_Context->Unmap(m_VertexBuffer.Get(), 0);
}

HRESULT EmLaser::Render()
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
	for (uint32 i = 0; i < m_NumDraws; ++i)
	{
		const DRAW& draw = m_Draws[i];
		if (FAILED(m_Texture->Bind_ShaderResourceView(m_Shader, "g_EffectMap", draw.image)) ||
			FAILED(m_Shader->Begin(draw.pass))) return E_FAIL;
		m_Context->Draw(draw.count, draw.first);
	}
	return S_OK;
}

void EmLaser::Submit_RenderGroup()
{
	// The haze reads the lit screen copied before this group; depth-tested against the opaque scene.
	if (m_NumDraws > 0) GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::DISTORTION, shared_from_this());
}

Shared<EmLaser> EmLaser::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<EmLaser>(device, context);
	return SUCCEEDED(prototype->Initialize_Prototype()) ? prototype : nullptr;
}

Shared<GameObject> EmLaser::Clone(void* arg)
{
	auto instance = make_shared<EmLaser>(*this);
	return SUCCEEDED(instance->Initialize(arg)) ? instance : nullptr;
}
