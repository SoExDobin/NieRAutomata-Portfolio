#include "pch.h"
#include "SparkEffect.h"
#include "VIBuffer_Particle_Point.h"
#include <Game.h>
#include "Random_Helper.h"
#include "SpdLogger.h"

SparkEffect::SparkEffect() : ParticleEffect{} {}
SparkEffect::SparkEffect(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: ParticleEffect{device, context} {}
SparkEffect::SparkEffect(const SparkEffect& rhs)
	: ParticleEffect{rhs} {}

HRESULT SparkEffect::Initialize_Prototype()
{
	m_LayerMask.Set_Layer(L"Default");
	if (FAILED(ParticleEffect::Initialize_Prototype()))
	{
		LOG_ERROR(L"Failed to Initialize Proto ParticleEffect");
		return E_FAIL;
	}

	return S_OK;
}

HRESULT SparkEffect::Initialize(void* arg)
{
	if (nullptr == arg)
	{
		LOG_ERROR(L"There is no ParticleEffect Desc");
		return E_FAIL;
	}

	if (FAILED(ParticleEffect::Initialize(arg)))
	{
		LOG_ERROR(L"Failed to Initialize ParticleEffect");
		return E_FAIL;
	}

	SPARK_EFFECT_DESC& desc = *static_cast<SPARK_EFFECT_DESC*>(arg);

	// The hit rotation's +Y is the way the sparks fly: the sword's swing, a bullet's bounce.
	Vector3 axis = Vector3::Transform(Vector3::UnitY, desc.rotation);
	axis.Normalize();
	m_Impact = desc.impact;
	m_Electric = desc.electric;
	m_AnchorBottom = desc.atkType != ATK_TYPE::POD && m_Impact <= 0.f && m_Electric <= 0.f;
	// The spray is sized before the components: the point buffer holds one point per spray spark.
	if (m_Impact > 0.f)
	{
		const Float reach = sqrtf(m_Impact);
		m_Spray = ImpactSpray;
		m_Spray.count = std::min(MaxSpray, static_cast<uint32>(ImpactSpray.count * m_Impact));
		m_Spray.minSpeed *= reach;
		m_Spray.maxSpeed *= reach;
		m_SprayAxis = Vector3::UnitY;
	}
	else if (m_Electric <= 0.f)
	{
		m_Spray = m_AnchorBottom ? SwordSpray : PodSpray;
		m_SprayAxis = axis;
	}

	if (FAILED(Ready_Components()))
	{
		LOG_ERROR(L"Failed to Ready_Components ParticleEffect");
		return E_FAIL;
	}

	m_Refract = desc.refract || m_Impact >= ImpactRefractFrom;
	if (m_Impact >= ImpactRefractFrom)
		GAME_INSTANCE->Start_ScreenGlitch(ImpactGlitch, ImpactGlitchTime);
	m_HitPosition = desc.position;
	m_FlareRoll = static_cast<Float>(Helper::Random_Double(0.0, XM_2PI));

	const int32 lastFrame = static_cast<int32>(m_Texture->Get_TextureNumSRVs()) - 1;
	auto spread = [&axis](Float s)
	{
		Vector3 direction = axis + Vector3{
			static_cast<Float>(Helper::Random_Double(-s, s)),
			static_cast<Float>(Helper::Random_Double(-s, s)),
			static_cast<Float>(Helper::Random_Double(-s, s)) };
		if (direction.LengthSquared() < 0.0001f) direction = axis;
		direction.Normalize();
		return direction;
	};

	if (m_Electric > 0.f)
	{
		// Electricity: a few small sparks jump off the crackle every way and fall a little.
		m_NumSparks = ElectricSparks;
		for (uint32 i = 0; i < m_NumSparks; ++i)
		{
			Vector3 direction{ static_cast<Float>(Helper::Random_Double(-1.0, 1.0)), static_cast<Float>(Helper::Random_Double(-0.3, 1.0)),
				static_cast<Float>(Helper::Random_Double(-1.0, 1.0)) };
			if (direction.LengthSquared() < 0.0001f) direction = Vector3::UnitY;
			direction.Normalize();
			m_Sparks[i] = { static_cast<uint32>(Helper::Random_Int(0, lastFrame)), desc.position,
				direction * static_cast<Float>(Helper::Random_Double(3.0, 7.0)), direction,
				static_cast<Float>(Helper::Random_Double(0.1, 0.25)), static_cast<Float>(Helper::Random_Double(0.3, 0.6)) };
		}
		m_ElectricFrame = m_ElectricFrames > 0 ? static_cast<uint32>(Helper::Random_Int(0, static_cast<int32>(m_ElectricFrames) - 1)) : 0;
		m_ElectricRoll = static_cast<Float>(Helper::Random_Double(0.0, XM_2PI));
		m_Duration = static_cast<Float>(Helper::Random_Double(ElectricMinTime, ElectricMaxTime));
	}
	else if (m_Impact > 0.f)
	{
		// A blow on the floor: streaks flung up and out all round, falling back in arcs; a bigger blow throws more of
		// them, faster and longer.
		const Float reach = sqrtf(m_Impact);
		m_NumSparks = std::min(MaxSparks, static_cast<uint32>(ImpactSparks * m_Impact));
		m_Gravity = ImpactGravity;
		for (uint32 i = 0; i < m_NumSparks; ++i)
		{
			const Float around = static_cast<Float>(Helper::Random_Double(0.0, XM_2PI));
			const Float up = static_cast<Float>(Helper::Random_Double(ImpactMinAngle, ImpactMaxAngle));
			const Vector3 direction{ cosf(around) * cosf(up), sinf(up), sinf(around) * cosf(up) };
			m_Sparks[i] = { static_cast<uint32>(Helper::Random_Int(0, lastFrame)), desc.position,
				direction * static_cast<Float>(Helper::Random_Double(ImpactMinSpeed, ImpactMaxSpeed)) * reach, direction,
				static_cast<Float>(Helper::Random_Double(0.4, 0.9)), static_cast<Float>(Helper::Random_Double(1.5, 3.0)) * reach };
		}
	}
	else if (m_AnchorBottom)
	{
		// Sword: a few long Melee_Spark streaks rooted at the hit point, shooting out along the swing (see Render).
		m_NumSparks = 5;
		for (uint32 i = 0; i < m_NumSparks; ++i)
			m_Sparks[i] = { static_cast<uint32>(Helper::Random_Int(0, lastFrame)), desc.position, Vector3::Zero, spread(0.3f),
				static_cast<Float>(Helper::Random_Double(0.15, 0.25)), static_cast<Float>(Helper::Random_Double(2.25, 3.75)) };
	}
	else
	{
		// Pod bullet: a shower of small Melee_Spark streaks thrown back out of the hit, falling a little (tuned on screen, not source data).
		m_NumSparks = MaxSparks;
		for (uint32 i = 0; i < m_NumSparks; ++i)
		{
			const Vector3 direction = spread(0.6f);
			m_Sparks[i] = { static_cast<uint32>(Helper::Random_Int(0, lastFrame)), desc.position,
				direction * static_cast<Float>(Helper::Random_Double(6.0, 12.0)), direction,
				static_cast<Float>(Helper::Random_Double(0.15, 0.3)), static_cast<Float>(Helper::Random_Double(0.6, 1.3)) };
		}
	}
	for (uint32 i = 0; i < m_NumSparks; ++i)
	{
		m_Sparks[i].launchSpeed = m_Sparks[i].velocity.Length();
		m_Duration = std::max(m_Duration, m_Sparks[i].lifeTime);
	}
	if (m_Refract)
		m_Duration = std::max(m_Duration, RefractTime);
	if (m_Spray.count > 0)
		m_Duration = std::max(m_Duration, m_Spray.life);
	if (m_Impact > 0.f)
		m_Duration = std::max(m_Duration, m_FlareSRVs[FLARE_CROSS] ? FlareTime : ImpactGlowTime);

	if (m_AnchorBottom)
	{
		m_SlashAxis = axis;
		m_SlashRoll = static_cast<Float>(Helper::Random_Double(0.0, XM_2PI));
		for (Float& offset : m_LineSparkOffsets)
			offset = static_cast<Float>(Helper::Random_Double(-0.3, 0.3)) * SlashLength;
		m_Duration = std::max(m_Duration, SlashTime);
	}

	return S_OK;
}

void SparkEffect::Priority_Update(Float timeDelta)
{
	
}

void SparkEffect::Update(Float timeDelta)
{
	m_Age += timeDelta;
	if (m_Age >= m_Duration)
	{
		GAME_INSTANCE->Destroy(Get_ObjectGuid());
		return;
	}

	for (uint32 i = 0; i < m_NumSparks; ++i)
	{
		SPARK& spark = m_Sparks[i];
		if (spark.velocity == Vector3::Zero)
			continue;
		spark.velocity.y -= m_Gravity * timeDelta;
		spark.position += spark.velocity * timeDelta;
		spark.direction = spark.velocity;
		spark.direction.Normalize();
	}
}

void SparkEffect::Late_Update(Float timeDelta)
{
	
}

void SparkEffect::Fixed_Update(Float fixedDelta)
{
	
}

HRESULT SparkEffect::Render()
{
	const auto camera = GAME_INSTANCE->Get_MainCamera();
	if (camera == nullptr)
		return S_OK;
	if (FAILED(Bind_ShaderResources()))
		return E_FAIL;

	const Vector3 cameraPosition = camera->Get_Transform()->Get_Position();
	const auto cameraTransform = camera->Get_Transform();
	// A glowing effect draws twice: its ricochet ring in DISTORTION, the rest in EMISSIVE.
	const RENDERGROUP group = GAME_INSTANCE->Get_DrawingGroup();
	const Bool glowing = Is_Glowing();

	auto draw = [this](const ComPtr<ID3D11ShaderResourceView>& srv, const Matrix& world, const Vector4& tint, Float fade, uint32 glow,
		const Vector2& dissolve = Vector2::Zero)
	{
		if (FAILED(m_Shader->Bind_RawValue("g_QuadGlow", &glow, sizeof(uint32))) ||
			FAILED(m_Shader->Bind_RawValue("g_QuadDissolve", &dissolve, sizeof(Vector2))) ||
			FAILED(m_Shader->Bind_RawValue("g_QuadTint", &tint, sizeof(Vector4))) ||
			FAILED(m_Shader->Bind_RawValue("g_FadeAlpha", &fade, sizeof(Float))) ||
			FAILED(m_Shader->Bind_Matrix(WorldMatrix, &world)) ||
			FAILED(m_Shader->Bind_SRV(DiffuseMap, srv)) ||
			FAILED(m_Shader->Begin("Quad")) ||
			FAILED(m_Buffer->Bind_Resources()) ||
			FAILED(m_Buffer->Render(1)))
			return E_FAIL;
		return S_OK;
	};
	// Stands along the direction and turns about it toward the camera.
	// A square facing the camera, turned by roll.
	auto facing = [&cameraTransform](const Vector3& center, Float size, Float roll)
	{
		const Float c = cosf(roll) * size, s = sinf(roll) * size;
		Matrix world{ cameraTransform->Get_Right() * c + cameraTransform->Get_Up() * s,
			cameraTransform->Get_Up() * c - cameraTransform->Get_Right() * s, cameraTransform->Get_Look() };
		world.Translation(center);
		return world;
	};
	auto alongAxis = [&cameraPosition, &cameraTransform](const Vector3& center, const Vector3& direction, Float height, Float width)
	{
		Vector3 right = direction.Cross(cameraPosition - center);
		if (right.LengthSquared() < 0.000001f)
			right = cameraTransform->Get_Right();
		right.Normalize();
		Matrix world{ right * width, direction * height, right.Cross(direction) };
		world.Translation(center);
		return world;
	};

	if (m_AnchorBottom)
	{
		// The cut: haze, the line over it, sparks strung along it and the burst on top. It all narrows a little as it
		// fades so the cut reads as closing. Sizes are tuned on screen; the tints are the 031 frames' .est colours at 0.75.
		if (m_Age < SlashTime)
		{
			const Float progress = m_Age / SlashTime;
			const Float fade = 1.f - progress * progress;
			const Float narrow = 1.f - 0.4f * progress;
			if (FAILED(draw(m_SlashSRVs[SLASH_HAZE], alongAxis(m_HitPosition, m_SlashAxis, SlashLength, SlashLength * 0.15f * narrow),
					Vector4{ 1.2f, 0.75f, 0.45f, 1.5f }, fade, 0)))
				return E_FAIL;
			// The bar is 1/8 of its frame's width, so this draws a line about 6 cm thick.
			if (FAILED(draw(m_SlashSRVs[SLASH_LINE], alongAxis(m_HitPosition, m_SlashAxis, SlashLength, SlashLength * 0.1f * narrow),
					Vector4{ 1.5f, 1.125f, 0.75f, 3.f }, fade, 0)))
				return E_FAIL;
			for (const Float offset : m_LineSparkOffsets)
				if (FAILED(draw(m_SlashSRVs[SLASH_SPARKS], alongAxis(m_HitPosition + m_SlashAxis * offset, m_SlashAxis, 1.5f, 0.75f),
						Vector4{ 1.5f, 1.125f, 0.75f, 2.f }, fade, 0)))
					return E_FAIL;

			const Float burstSize = 2.f * narrow;
			const Float c = cosf(m_SlashRoll) * burstSize;
			const Float s = sinf(m_SlashRoll) * burstSize;
			Matrix burst{ cameraTransform->Get_Right() * c + cameraTransform->Get_Up() * s,
				cameraTransform->Get_Up() * c - cameraTransform->Get_Right() * s, cameraTransform->Get_Look() };
			burst.Translation(m_HitPosition);
			if (FAILED(draw(m_SlashSRVs[SLASH_BURST], burst, Vector4{ 1.5f, 1.125f, 0.75f, 2.f }, fade, 0)))
				return E_FAIL;
		}
	}
	else
	{
		// A ricochet first bends the screen in a ring around the hit; the sparks are drawn over it.
		if (m_Refract && m_Age < RefractTime && (!glowing || group == RENDERGROUP::DISTORTION))
		{
			const Float refractSize = RefractSize * std::max(m_Impact, 1.f);
			Matrix world{ cameraTransform->Get_Right() * refractSize, cameraTransform->Get_Up() * refractSize, cameraTransform->Get_Look() };
			world.Translation(m_HitPosition - cameraTransform->Get_Look() * RefractLift);
			const Vector4 ring{ m_Age / RefractTime, RefractStrength, RefractEmissive, 0.f };
			if (FAILED(GAME_INSTANCE->Bind_RenderTarget_ShaderResource(m_Shader, SceneMap, RT_SCENE)) ||
				FAILED(m_Shader->Bind_RawValue("g_ShockRing", &ring, sizeof(Vector4))) ||
				FAILED(m_Shader->Bind_Matrix(WorldMatrix, &world)) ||
				FAILED(m_Shader->Begin("ShockRing")) ||
				FAILED(m_Buffer->Bind_Resources()) ||
				FAILED(m_Buffer->Render(1)))
				return E_FAIL;
		}
		if (glowing && group != RENDERGROUP::EMISSIVE)
			return S_OK;
		// Electricity: the crackle facing the camera, turned at random, running through its frames and flickering out.
		if (m_Electric > 0.f && m_ElectricFrames > 0)
		{
			tChar path[MAX_PATH] = TEXT("");
			const uint32 frame = (m_ElectricFrame + static_cast<uint32>(m_Age * ElectricRate)) % m_ElectricFrames;
			swprintf_s(path, m_ElectricPath.c_str(), frame);
			const ComPtr<ID3D11ShaderResourceView> crackle = GAME_INSTANCE->Get_Texture(m_ElectricLevel, path);
			const Float c = cosf(m_ElectricRoll) * m_Electric, s = sinf(m_ElectricRoll) * m_Electric;
			Matrix world{ cameraTransform->Get_Right() * c + cameraTransform->Get_Up() * s,
				cameraTransform->Get_Up() * c - cameraTransform->Get_Right() * s, cameraTransform->Get_Look() };
			world.Translation(m_HitPosition);
			const Float progress = m_Age / m_Duration;
			if (crackle && FAILED(draw(crackle, world, ElectricTint, 1.f - progress * progress, 0)))
				return E_FAIL;
			if (m_FlareSRVs[FLARE_GLOW] && FAILED(draw(m_FlareSRVs[FLARE_GLOW], facing(m_HitPosition, m_Electric * 0.6f, 0.f), ElectricGlowTint,
					1.f - progress, 0)))
				return E_FAIL;
		}
		// A blow's flash: a cross star over a starburst, both at full size at once and fading over FlareTime.
		if (m_Impact > 0.f && m_FlareSRVs[FLARE_CROSS] && m_Age < FlareTime)
		{
			const Float size = GlowSize * std::max(m_Impact * 3.f, 1.f) * FlareSize;
			const Float fade = (1.f - m_Age / FlareTime) * (1.f - m_Age / FlareTime);
			if (FAILED(draw(m_FlareSRVs[FLARE_STARBURST], facing(m_HitPosition, size, m_FlareRoll), FlareTint, fade, 0)) ||
				FAILED(draw(m_FlareSRVs[FLARE_CROSS], facing(m_HitPosition, size * 1.3f, 0.f), FlareTint, fade, 0)))
				return E_FAIL;
		}
		// Hot core: a camera-facing glow that flashes white-gold and is gone in GlowTime (a blow on the floor: larger with
		// the blow and a little longer).
		const Float glowTime = m_Impact > 0.f ? ImpactGlowTime : GlowTime;
		const Float glowSize = GlowSize * std::max(m_Impact * 3.f, 1.f);
		if (m_Age < glowTime)
		{
			Matrix world{ cameraTransform->Get_Right() * glowSize, cameraTransform->Get_Up() * glowSize, cameraTransform->Get_Look() };
			world.Translation(m_HitPosition);
			if (FAILED(draw(m_Texture->Get_Textures()[0], world, Vector4{ 2.5f, 2.f, 1.2f, 2.f }, 1.f - m_Age / glowTime, 1)))
				return E_FAIL;
		}
	}

	// The spray: every fine spark in one draw under the streaks, placed by the shader (see SPRAY_SHAPE).
	if (m_Spray.count > 0 && m_Age < m_Spray.life)
	{
		const Vector4 burst{ m_HitPosition.x, m_HitPosition.y, m_HitPosition.z, m_Age };
		const Vector4 aim{ m_SprayAxis.x, m_SprayAxis.y, m_SprayAxis.z, m_Spray.spread };
		const Vector4 motion{ m_Spray.minSpeed, m_Spray.maxSpeed, m_Spray.gravity, m_Spray.drag };
		const Vector4 look{ m_Spray.life, m_Spray.width, SprayTrail, m_FlareRoll };
		if (FAILED(m_Shader->Bind_RawValue("g_SprayBurst", &burst, sizeof(Vector4))) ||
			FAILED(m_Shader->Bind_RawValue("g_SprayAim", &aim, sizeof(Vector4))) ||
			FAILED(m_Shader->Bind_RawValue("g_SprayMotion", &motion, sizeof(Vector4))) ||
			FAILED(m_Shader->Bind_RawValue("g_SprayLook", &look, sizeof(Vector4))) ||
			FAILED(m_Shader->Bind_RawValue("g_SprayCount", &m_Spray.count, sizeof(uint32))) ||
			FAILED(m_Shader->Bind_RawValue("g_QuadTint", &SprayTint, sizeof(Vector4))) ||
			FAILED(m_Shader->Begin("Spray")) ||
			FAILED(m_Buffer->Bind_Resources()) ||
			FAILED(m_Buffer->Render(m_Spray.count)))
			return E_FAIL;
	}

	// The textures' lines are dim orange; brighten them toward gold-white. The Pod's small sparks burn twice as bright
	// so they still read at their size.
	// A blow on the floor throws hot metal, oranger than a bullet's glance.
	const Vector4 tint = m_AnchorBottom ? Vector4{ 2.5f, 2.f, 1.5f, 4.f } : m_Electric > 0.f ? ElectricSparkTint
		: m_Impact > 0.f ? ImpactSparkTint : Vector4{ 5.f, 4.f, 3.f, 4.f };
	for (uint32 i = 0; i < m_NumSparks; ++i)
	{
		const SPARK& spark = m_Sparks[i];
		if (m_Age >= spark.lifeTime)
			continue;
		const Float progress = m_Age / spark.lifeTime;
		const Float fade = 1.f - progress * progress;

		// Sword streaks shoot out from 40% to full length within their first 0.2 seconds.
		const Float grow = std::min(m_Age / 0.2f, 1.f);
		// A flying spark stretches along its flight with its speed; its width stays.
		const Float stretch = spark.launchSpeed > 0.f
			? std::clamp(StretchScale * spark.velocity.Length() / spark.launchSpeed, StretchMin, StretchMax) : 1.f;
		const Float height = m_AnchorBottom ? spark.height * (0.4f + 0.6f * (1.f - (1.f - grow) * (1.f - grow))) : spark.height * stretch;
		Vector3 center = spark.position;
		if (m_AnchorBottom)
			center += spark.direction * (height * 0.5f);

		// Melee_Spark is 128x256; the sword's streaks are drawn twice as long again so the lines read as long and thin.
		const Float width = m_AnchorBottom ? height * 0.25f : spark.height * 0.5f;
		// From DissolveFrom of its life the streak burns away in bits (seeded per spark and per effect).
		const Vector2 dissolve{ std::max((progress - DissolveFrom) / (1.f - DissolveFrom), 0.f), i + m_FlareRoll * 10.f };
		if (FAILED(draw(m_Texture->Get_Textures()[spark.frame], alongAxis(center, spark.direction, height, width), tint, fade, 0, dissolve)))
			return E_FAIL;
		// A glowing spark twinkles at its head (the streak's leading end), each on its own beat.
		if (glowing && m_FlareSRVs[FLARE_CROSS])
		{
			const Float twinkle = 0.5f + 0.5f * sinf(m_Age * GlintRate * XM_2PI + i * 2.4f);
			if (FAILED(draw(m_FlareSRVs[FLARE_CROSS], facing(center + spark.direction * (height * 0.5f), height * GlintSize, i * 0.7f),
					tint, fade * twinkle, 0)))
				return E_FAIL;
		}
	}

	// The effect is shared by every clone of this shader: leave the Quad pass whole for the others (EmTelegraph, PointLight...).
	const Vector2 whole{};
	return m_Shader->Bind_RawValue("g_QuadDissolve", &whole, sizeof(Vector2));
}

HRESULT SparkEffect::Ready_Components()
{
	Shader::SHADER_DESC shaderDesc{ VTXPARTICLE_POINT_DESC::Tag, VTXPARTICLE_POINT_DESC::Elements, VTXPARTICLE_POINT_DESC::numElements };
	m_Shader = Add_Component<Shader>(ETOI(LEVEL::GAMEPLAY), &shaderDesc);
	if (nullptr == m_Shader)
		return E_FAIL;

	// Unit points at the origin: the first is every quad's, its world matrix placing it; the spray places all of them.
	VIBuffer_Particle_Point::VIBUFFER_INSTANCE_POINT_DESC instanceDesc{};
	instanceDesc.numInstances = std::max(m_Spray.count, 1u);
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

	auto textureDesc = Texture::TEXTURE_DESC{ Get_LevelIndex(), L"Effect_Spark" };
	m_Texture = Add_Component<Texture>(ETOI(LEVEL::GAMEPLAY), &textureDesc);
	if (nullptr == m_Texture)
		return E_FAIL;

	const Texture::TEXTURE_DESC* slashDesc = GAME_INSTANCE->Get_TextureDesc(Get_LevelIndex(), L"Effect_SlashLine");
	if (nullptr == slashDesc || slashDesc->m_NumSRVs < SLASH_END)
		return E_FAIL;
	for (uint32 i = 0; i < SLASH_END; ++i)
	{
		tChar path[MAX_PATH] = TEXT("");
		swprintf_s(path, slashDesc->m_FilePath.c_str(), i);
		m_SlashSRVs[i] = GAME_INSTANCE->Get_Texture(slashDesc->m_levIndex, path);
		if (nullptr == m_SlashSRVs[i])
			return E_FAIL;
	}

	// The glints are optional too.
	if (const Texture::TEXTURE_DESC* flareDesc = GAME_INSTANCE->Get_TextureDesc(Get_LevelIndex(), L"Effect_Flare"))
		for (uint32 i = 0; i < std::min<uint32>(flareDesc->m_NumSRVs, FLARE_END); ++i)
		{
			tChar path[MAX_PATH] = TEXT("");
			swprintf_s(path, flareDesc->m_FilePath.c_str(), i);
			m_FlareSRVs[i] = GAME_INSTANCE->Get_Texture(flareDesc->m_levIndex, path);
		}

	// The crackle is optional: without it an electric spark is only its small sparks.
	if (const Texture::TEXTURE_DESC* electricDesc = GAME_INSTANCE->Get_TextureDesc(Get_LevelIndex(), L"Effect_Electric"))
	{
		m_ElectricLevel = electricDesc->m_levIndex;
		m_ElectricPath = electricDesc->m_FilePath;
		m_ElectricFrames = electricDesc->m_NumSRVs;
	}

	return S_OK;
}

HRESULT SparkEffect::Bind_ShaderResources()
{
	if (FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ViewMatrix, D3DTS::VIEW)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ProjMatrix, D3DTS::PROJ)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Bind_CameraPosition(m_Shader, CameraPosition)))
		return E_FAIL;

	return S_OK;
}

void SparkEffect::Submit_RenderGroup()
{
	// A ricochet reads the lit screen for its ring, so it draws in the distortion pass after the screen is copied. A
	// glowing effect draws the rest into the emissive target.
	if (Is_Glowing())
	{
		if (m_Refract)
			GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::DISTORTION, shared_from_this());
		GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::EMISSIVE, shared_from_this());
		return;
	}
	GAME_INSTANCE->Add_RenderGroup(m_Refract ? RENDERGROUP::DISTORTION : RENDERGROUP::BLEND, shared_from_this());
}

Shared<SparkEffect> SparkEffect::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<SparkEffect>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : SparkEffect");
		return nullptr;
	}

	return prototype;
}

Shared<GameObject> SparkEffect::Clone(void* arg)
{
	auto instance = make_shared<SparkEffect>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : SparkEffect");
		return nullptr;
	}

	return instance;
}


