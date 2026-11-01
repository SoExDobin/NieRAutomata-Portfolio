#include "pch.h"
#include "Bullet.h"

#include <Game.h>
#include <SphereCollider.h>

#include "SpdLogger.h"
#include "ExplodeEffect.h"
#include "SparkEffect.h"
#include "Model.h"
#include "Monster.h"
#include "Shader.h"
#include "Texture.h"
#include "PartObject.h"
#include "Pl0000.h"

namespace
{
	// Frames per second of the blue lightning around a ball that can be cut down (Effect_Bullet 5..10 in turn).
	constexpr Float LightningRate = 12.f;
	// WAVE: rise speed, bob frequency (rad/s; quicker on request, 2026-10-24) and height of the bob. The bob starts
	// upward, so a ball fired at floor level never dips under it.
	constexpr Float WaveRise = 0.5f;
	constexpr Float WaveFrequency = 16.f;
	constexpr Float WaveAmplitude = 0.5f;
	// RISE: upward acceleration (m/s^2).
	constexpr Float RiseAcceleration = 1.f;
	// ARC: share of the gravity given as the initial lift, and the burst size where it lands.
	constexpr Float ArcLift = 0.75f;
	constexpr Float ArcBurstSize = 4.f;
	constexpr Float ArcBurstVolume = 0.2f;
	// A shot glancing off (Ricochet) pings where it struck.
	constexpr Float RicochetVolume = 0.4f;
	constexpr Float RicochetNear = 5.f;
	constexpr Float RicochetFar = 50.f;
	constexpr Float ArcBurstNear = 10.f;
	constexpr Float ArcBurstFar = 80.f;
	// ARC: the burst glitches the screen (Game::Start_ScreenGlitch), weaker than a hit on the player. Tuned on screen.
	constexpr Float ArcBurstGlitch = 0.35f;
	constexpr Float ArcBurstGlitchTime = 0.2f;
}

map<Bullet::BATCH_KEY, Bullet::BATCH> Bullet::s_Batches{};
Weak<GameObject> Bullet::s_Leader{};
Bool Bullet::s_Drawn{ false };

Bullet::Bullet() : Projectile{} {}
Bullet::Bullet(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: Projectile{device, context} {}
Bullet::Bullet(const Bullet& rhs)
	: Projectile{rhs} {}

HRESULT Bullet::Initialize_Prototype()
{
	m_LayerMask.Set_Layer(L"Bullet");

	return Projectile::Initialize_Prototype();
}

HRESULT Bullet::Initialize(void* arg)
{
	if (FAILED(Projectile::Initialize(arg)))
	{
		LOG_ERROR(L"Failed to Initialize Bullet");
		return E_FAIL;
	}

	if (arg != nullptr)
		m_Desc = *static_cast<BULLET_DESC*>(arg);

	m_DamageInfo = m_Desc.damageInfo;
	// A random start, so the balls' patterns and lightning don't move in step.
	static std::mt19937 random{ std::random_device{}() };
	m_Age = std::uniform_real_distribution<Float>{ 0.f, 10.f }(random);
	m_Transform->Set_Position(m_Desc.initialPosition);
	m_Transform->LookAt(m_Desc.initialPosition + m_Desc.direction);
	m_Velocity = m_Desc.direction;
	m_Velocity.Normalize();
	m_Velocity *= m_Desc.speed;
	if (m_Desc.flight == FLIGHT::ARC)
		m_Velocity.y += m_Desc.gravity * ArcLift;

	if (FAILED(Ready_Components()))
	{
		LOG_ERROR(L"Failed to Ready_Components Bullet");
		return E_FAIL;
	}

	return S_OK;
}

void Bullet::Update(Float timeDelta)
{
	m_TravelDistance += m_Speed * timeDelta;
	m_Age += timeDelta;

	if (m_TravelDistance >= m_Desc.maxDistance)
	{
		GAME_INSTANCE->Destroy(Get_ObjectGuid());
		return;
	}
	m_FlightTime += timeDelta;
	const Vector3 previous = m_Transform->Get_Position();
	Vector3 position = previous;
	const auto burst = [this](const Vector3& at)
	{
		ExplodeEffect::EXPLODE_EFFECT_DESC desc{};
		desc.position = at;
		desc.size = ArcBurstSize;
		GAME_INSTANCE->Instantiate<ExplodeEffect>(L"ExplodeEffect", Get_LevelIndex(), &desc);
		GAME_INSTANCE->Start_ScreenGlitch(ArcBurstGlitch, ArcBurstGlitchTime);
		if (!m_Desc.burstSound.empty())
			GAME_INSTANCE->PlaySound3D(m_Desc.burstSound, ArcBurstVolume, at, ArcBurstNear, ArcBurstFar, false);
	};
	switch (m_Desc.flight)
	{
	case FLIGHT::STRAIGHT:
		position += m_Velocity * timeDelta;
		break;
	case FLIGHT::WAVE:
		position += m_Velocity * timeDelta;
		position.y += (WaveRise + sinf(m_FlightTime * WaveFrequency) * WaveAmplitude * WaveFrequency) * timeDelta;
		break;
	case FLIGHT::RISE:
		m_Velocity.y += RiseAcceleration * timeDelta;
		position += m_Velocity * timeDelta;
		break;
	case FLIGHT::ARC:
		m_Velocity.y -= m_Desc.gravity * timeDelta;
		position += m_Velocity * timeDelta;
		if (position.y <= m_Desc.groundY)
		{
			burst(Vector3{ position.x, m_Desc.groundY, position.z });
			GAME_INSTANCE->Destroy(Get_ObjectGuid());
			return;
		}
		break;
	}
	// The map's collision (COL) stops every bullet along this frame's flight; a lobbed shell bursts on it as on the floor.
	Vector3 travel = position - previous;
	const Float step = travel.Length();
	Vector3 hit{};
	if (step > 0.0001f && GAME_INSTANCE->Raycast_Physics(previous, travel / step, step, hit))
	{
		if (m_Desc.flight == FLIGHT::ARC)
			burst(hit);
		GAME_INSTANCE->Destroy(Get_ObjectGuid());
		return;
	}
	m_Transform->Set_Position(position);
	m_Collider->Update(m_Transform->Get_WorldMatrix());
	m_Transform->Update_WorldMatrix();
}

void Bullet::Late_Update(Float timeDelta)
{
	
}

HRESULT Bullet::Render()
{
	// Only the leader is in the render group; it draws every bullet's batch (Submit_RenderGroup).
	if (FAILED(Bind_ShaderResources()))
		return E_FAIL;

	// Effect_Bullet (coreeff): 0 = 001/11 the ball's disc and rim, 1 = 001/9 the Pod shot's core,
	// 3 = 009/1 the ball's fiery mottle, 4 = 009/0 the unbreakable ball's curls, 5..10 = 036/0..5 the blue lightning.
	// Bullets at the player that can be cut down are salmon-orange mottled balls with a pale rim, wrapped in flickering
	// blue lightning; the ones that can't are dark with a thick hot-pink rim and pink curls inside. The player's own
	// (Pod) are white-hot cores with no halo. Each bullet's age (instance params.x) drives its patterns.
	const auto draw = [this](const BATCH& batch, const char* pass, const Vector4& color, const Vector4* glow, uint32 image,
		const char* secondMap, uint32 secondImage)
	{
		const Shared<Model> model = batch.model.lock();
		if (!model)
			return S_OK;
		if (FAILED(m_Shader->Bind_RawValue("g_EffectColor", &color, sizeof(color))) ||
			(glow && FAILED(m_Shader->Bind_RawValue("g_EffectGlowColor", glow, sizeof(*glow)))) ||
			FAILED(m_Texture->Bind_ShaderResourceView(m_Shader, "g_EffectMap", image)) ||
			(secondMap && FAILED(m_Texture->Bind_ShaderResourceView(m_Shader, secondMap, secondImage))))
			return E_FAIL;
		const uint32 numMeshes = model->Get_NumMeshes();
		for (uint32 i = 0; i < numMeshes; ++i)
		{
			if (FAILED(model->Set_Instances(i, batch.instances)) || FAILED(m_Shader->Begin(pass)) || FAILED(model->Render(i)))
				return E_FAIL;
		}
		return S_OK;
	};

	static const Vector4 solidGlow{ 0.95f, 0.3f, 0.5f, 1.f };
	static const Vector4 ballGlow{ 1.f, 0.9f, 0.86f, 1.f };
	s_Drawn = true;
	const Vector4 camera = GAME_INSTANCE->Get_CamTransform();
	const Vector3 eye{ camera.x, camera.y, camera.z };
	for (auto& [key, batch] : s_Batches)
	{
		if (batch.instances.empty())
			continue;
		// Back to front from this View's camera, so a near ball blends over the far ones behind it whatever order the
		// bullets submitted in (the trumpet's newest, furthest orb was drawn last, over the near ones).
		std::sort(batch.instances.begin(), batch.instances.end(), [&eye](const INSTANCE_DATA& a, const INSTANCE_DATA& b)
			{ return Vector3::DistanceSquared(a.world.Translation(), eye) > Vector3::DistanceSquared(b.world.Translation(), eye); });
		HRESULT result = S_OK;
		switch (key.kind)
		{
		case BATCH_KIND::POD:
			result = draw(batch, "BulletGlow", Vector4{ 1.f, 0.97f, 0.9f, 1.f }, nullptr, 1, nullptr, 0);
			break;
		case BATCH_KIND::SOLID:
			result = draw(batch, "BulletSolid", Vector4{ 0.14f, 0.07f, 0.13f, 0.97f }, &solidGlow, 0, "g_BulletPatternMap", 4);
			break;
		case BATCH_KIND::BALL:
			result = draw(batch, "Bullet", Vector4{ 0.72f, 0.18f, 0.14f, 0.95f }, &ballGlow, 0, "g_BulletPatternMap", 3);
			break;
		case BATCH_KIND::LIGHTNING:
			result = draw(batch, "BulletLightning", Vector4{ 0.45f, 0.6f, 1.f, 0.7f }, nullptr, 0, "g_BulletLightningMap", key.image);
			break;
		}
		if (FAILED(result))
			return E_FAIL;
	}
	return S_OK;
}

void Bullet::Submit_RenderGroup()
{
	/* Submit runs once a frame and Render once per View, so the batches are filled here and drawn by the leader.
	   A frame starts at the first submit after the leader drew, or when there is no leader (expired, disabled), or
	   when the leader submits again without having drawn (paused: Submit repeats without a draw), so the batches
	   never grow past one frame's bullets. The frame counter is not used: it stops while editing and paused. */
	const Shared<GameObject> leader = s_Leader.lock();
	if (s_Drawn || !leader || leader.get() == this)
	{
		for (auto& [key, batch] : s_Batches)
		{
			batch.instances.clear();
			batch.model.reset();
		}
		s_Drawn = false;
		s_Leader = shared_from_this();
		// Lit by itself. Drawn after the screen grade like LaserEffect: the fog and volumetric haze measure the opaque depth
		// behind the sprite (bullets write none), so in BLEND they would fade a near bullet like the far wall.
		GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::WORLDUI, shared_from_this());
	}

	const INSTANCE_DATA instance{ m_Transform->Get_WorldMatrix(), Vector4{ m_Age, 0.f, 0.f, 0.f } };
	if (m_Desc.targetLayer != L"Player")
		Add_ToBatch(BATCH_KIND::POD, 0, instance);
	else if (m_Desc.isPermanent)
		Add_ToBatch(BATCH_KIND::SOLID, 0, instance);
	else
	{
		Add_ToBatch(BATCH_KIND::BALL, 0, instance);
		Add_ToBatch(BATCH_KIND::LIGHTNING, 5 + static_cast<uint32>(m_Age * LightningRate) % 6, instance);
	}
}

void Bullet::Add_ToBatch(BATCH_KIND kind, uint32 image, const INSTANCE_DATA& instance)
{
	BATCH& batch = s_Batches[BATCH_KEY{ m_Desc.resourceTag, kind, image }];
	if (batch.instances.empty())
		batch.model = m_Model;
	batch.instances.push_back(instance);
}

void Bullet::On_Disable()
{
	if (s_Leader.lock().get() == this)
		s_Leader.reset();
	Projectile::On_Disable();
}

void Bullet::On_Destroy()
{
	if (s_Leader.lock().get() == this)
		s_Leader.reset();
	Projectile::On_Destroy();
}

void Bullet::OnCollisionEnter(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
	auto target = targetCollider->Get_Owner();
	if (!target) return;
	// Destroy is deferred, so a bullet touching two colliders in one frame (Em3000's two spheres) must still hit once.
	if (m_IsSpent) return;

	if (target->Get_LayerMask().Get_Layer() == Get_LayerMask().Get_Layer())
	{
		auto otherBullet = static_pointer_cast<Bullet>(target);

		if (otherBullet->Is_Permanent() || m_Desc.isPermanent) return;
		if (m_Desc.targetLayer == otherBullet->Get_TargetLayerName()) return;
		m_IsSpent = true;
		GAME_INSTANCE->Destroy(Get_ObjectGuid());
		return;
	}

	if (target->Get_GameObjectType() != GAMEOBJECTTYPE::PART) return;
	
	auto entity = static_pointer_cast<PartObject>(target)->Get_Owner();
	const wstring& layerTag = target->Get_LayerMask().Get_LayerName();
	if (m_Desc.targetLayer == L"Monster" &&
		m_Desc.targetLayer == layerTag)
	{
		auto monster = static_pointer_cast<Monster>(entity);
		if (monster->Deflects(target))
		{
			Ricochet(targetCollider);
			return;
		}

		m_DamageInfo.hitPosition = targetCollider->ClosestPoint(ownCollider->Get_Pivot());
		// Sparks bounce back the way the bullet came.
		Vector3 back = -m_Desc.direction;
		back.Normalize();
		m_DamageInfo.hitRotation = Quaternion::FromToRotation(Vector3::UnitY, back);

		monster->TakeDamage(m_DamageInfo);
		m_IsSpent = true;
		GAME_INSTANCE->Destroy(Get_ObjectGuid());
		return;
	}
	
	if (m_Desc.targetLayer == L"Player")
	{
		if (layerTag == L"PlayerWeapon"
			&& false == m_Desc.isPermanent)
		{
			if (m_Desc.isPermanent) return;
			m_IsSpent = true;
			GAME_INSTANCE->Destroy(Get_ObjectGuid());
			return;
		}
		if (layerTag == L"Player")
		{
			m_DamageInfo.hitPosition = targetCollider->ClosestPoint(ownCollider->Get_Pivot());
			// Pl0000::TryEvade refuses dash evasion when the attacker is a bullet.
			m_DamageInfo.attacker = shared_from_this();
			auto player = static_pointer_cast<Pl0000>(entity);
			player->TakeDamage(m_DamageInfo);
			m_IsSpent = true;
			GAME_INSTANCE->Destroy(Get_ObjectGuid());
		}
		return;
	}
}

void Bullet::Ricochet(const Shared<Collider>& surface)
{
	m_IsSpent = true;
	const Vector3 position = m_Transform->Get_Position();
	Vector3 normal = position - surface->Get_Pivot();
	normal.Normalize();
	// Mirror the flight off the surface.
	const Float into = m_Velocity.Dot(normal);
	if (into < 0.f)
		m_Velocity -= normal * (2.f * into);
	m_Transform->LookAt(position + m_Velocity);

	SparkEffect::SPARK_EFFECT_DESC spark{};
	spark.atkType = ATK_TYPE::POD;
	spark.position = surface->ClosestPoint(position);
	spark.rotation = Quaternion::FromToRotation(Vector3::UnitY, normal);
	spark.refract = true;
	GAME_INSTANCE->Instantiate<SparkEffect>(L"SparkEffect", Get_LevelIndex(), &spark);
	GAME_INSTANCE->PlaySound3D(L"core_hack_em_hit_reflect", RicochetVolume, spark.position, RicochetNear, RicochetFar, false);
}

HRESULT Bullet::Ready_Components()
{
	// Bullets draw in instanced batches (Render): Shader_VtxMesh_Effect.hlsl with the per-instance input.
	Shader::SHADER_DESC shaderDesc{ L"VTX_Mesh_Instance_Effect", VTXMESH_INSTANCE::Elements, VTXMESH_INSTANCE::numElements };
	m_Shader = Add_Component<Shader>(ETOI(LEVEL::STATIC), &shaderDesc);
	if (nullptr == m_Shader)
		return E_FAIL;

	Texture::TEXTURE_DESC textureDesc{ Get_LevelIndex(), L"Effect_Bullet" };
	m_Texture = Add_Component<Texture>(Get_LevelIndex(), &textureDesc);
	if (nullptr == m_Texture)
		return E_FAIL;

	Model::MODEL_DESC modelDesc{ m_Desc.resourceTag };
	m_Model = Add_Component<Model>(ETOI(LEVEL::STATIC), &modelDesc);
	if (nullptr == m_Model)
		return E_FAIL;

	SphereCollider::SPHERE_COLLIDER_DESC sphereColliderDesc{};
	sphereColliderDesc.radius = 1.f;
	sphereColliderDesc.offset = Vector3::Zero;
	m_Collider = Add_Component<SphereCollider>(ETOI(LEVEL::GAMEPLAY), &sphereColliderDesc);
	if (nullptr == m_Collider)
		return E_FAIL;

	// Enemy bullets are big and easy to read, the Pod's small; the collider scales with them.
	const Float scale = m_Desc.targetLayer == L"Player" ? 0.6f : 0.12f;
	m_Transform->Set_Scale(scale, scale, scale);

	return S_OK;
}

HRESULT Bullet::Bind_ShaderResources()
{
	// The batches carry each bullet's world matrix; the object world is identity.
	const Matrix identity = Matrix::Identity;
	if (FAILED(m_Shader->Bind_Matrix(WorldMatrix, &identity)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ViewMatrix, D3DTS::VIEW)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ProjMatrix, D3DTS::PROJ)))
		return E_FAIL;

	if (FAILED(GAME_INSTANCE->Bind_CameraPosition(m_Shader, CameraPosition)))
		return E_FAIL;

	return S_OK;
}

void Bullet::Fire_Spread(const BULLET_DESC& desc, uint32 count, Float spread, Float reach, uint32 levelIndex)
{
	if (count == 0) return;
	const Bool ring = spread >= XM_2PI;
	const Float step = ring ? XM_2PI / count : (count > 1 ? spread / (count - 1) : 0.f);
	const Float first = ring ? 0.f : -step * (count - 1) * 0.5f;
	BULLET_DESC shot = desc;
	for (uint32 i = 0; i < count; ++i)
	{
		shot.direction = Vector3::TransformNormal(desc.direction, Matrix::CreateRotationY(first + step * i));
		shot.initialPosition = desc.initialPosition + shot.direction * reach;
		GAME_INSTANCE->Instantiate<Bullet>(L"Bullet", levelIndex, &shot);
	}
}

Shared<Bullet> Bullet::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<Bullet>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Bullet");
		return nullptr;
	}

	return prototype;
}

Shared<GameObject> Bullet::Clone(void* arg)
{
	auto instance = make_shared<Bullet>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : Bullet");
		return nullptr;
	}

	return instance;
}


