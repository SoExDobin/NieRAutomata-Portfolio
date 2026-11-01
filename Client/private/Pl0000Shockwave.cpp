#include "pch.h"
#include "Pl0000Shockwave.h"

#include <SpdLogger.h>
#include <Game.h>
#include <SphereCollider.h>
#include "ExplodeEffect.h"
#include "Monster.h"
#include "VIBuffer_Particle_Point.h"
#include "PartObject.h"

Pl0000Shockwave::Pl0000Shockwave() : GameObject{} {}
Pl0000Shockwave::Pl0000Shockwave(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: GameObject{ device, context } {
}
Pl0000Shockwave::Pl0000Shockwave(const Pl0000Shockwave& rhs)
	: GameObject{ rhs } {
}

HRESULT Pl0000Shockwave::Initialize_Prototype()
{
	m_LayerMask.Set_Layer(L"PlayerWeapon");
	m_TagMask.Set_Tag({ L"PlayerWeapon" });

	return GameObject::Initialize_Prototype();
}

HRESULT Pl0000Shockwave::Initialize(void* arg)
{
	if (FAILED(GameObject::Initialize(arg)))
		return E_FAIL;

	if (arg == nullptr) return E_FAIL;

	PLAYER_SHOCKWAVE_DESC* desc = static_cast<PLAYER_SHOCKWAVE_DESC*>(arg);
	m_Transform->Set_Position(desc->position);
	m_RingRadius = desc->radius * RingReach;
	// The blast point sits on or just above the floor; one with no floor under it raises no dust and no ring.
	m_HasGround = GAME_INSTANCE->Raycast_Physics(desc->position + Vector3{ 0.f, 1.5f, 0.f }, -Vector3::UnitY, 4.f, m_Ground);
	m_DamageInfo = desc->damageInfo;
	m_MonsterLayerIndex = ETOI(GAME_INSTANCE->Get_LayerRegister()->Get_LayerByName(L"Monster"));
	m_DamageInfo.blowAway = true;

	if (FAILED(Ready_Components(*desc)))
	{
		LOG_ERROR(L"Failed To Ready_Components : PlayerShockwave");
		return E_FAIL;
	}

	if (m_HasGround)
		Raise_GroundDust();
	GAME_INSTANCE->Start_ScreenGlitch(ColourSplit, ColourSplitTime, false);

	return S_OK;
}

void Pl0000Shockwave::On_Destroy()
{
	GameObject::On_Destroy();
}

void Pl0000Shockwave::Priority_Update(Float timeDelta)
{
}

void Pl0000Shockwave::Update(Float timeDelta)
{
	m_Age += timeDelta;
	if (m_Age >= (m_HasGround ? std::max(HitWindow, RingDuration) : HitWindow))
		GAME_INSTANCE->Destroy(Get_ObjectGuid());
}

void Pl0000Shockwave::Late_Update(Float timeDelta)
{
	m_Transform->Update_WorldMatrix();
	m_ShockwaveCollider->Update(m_Transform->Get_WorldMatrix());
}
void Pl0000Shockwave::Fixed_Update(Float fixedDelta)
{

}

void Pl0000Shockwave::Submit_RenderGroup()
{
	if (m_HasGround && m_Age < RingDuration)
		GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::DISTORTION, shared_from_this());
}

HRESULT Pl0000Shockwave::Render()
{
	// The quad's right and up rows span it, so it lies on the floor, just above it.
	const Float diameter = m_RingRadius * 2.f;
	Matrix world{ Vector3{ diameter, 0.f, 0.f }, Vector3{ 0.f, 0.f, diameter }, Vector3::UnitY };
	world.Translation(m_Ground + Vector3{ 0.f, 0.05f, 0.f });
	const Vector4 ring{ m_Age / RingDuration, RingRefraction, RingEmissive, 0.f };

	if (FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ViewMatrix, D3DTS::VIEW)) ||
		FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ProjMatrix, D3DTS::PROJ)) ||
		FAILED(GAME_INSTANCE->Bind_RenderTarget_ShaderResource(m_Shader, SceneMap, RT_SCENE)) ||
		FAILED(m_Shader->Bind_RawValue("g_ShockRing", &ring, sizeof(Vector4))) ||
		FAILED(m_Shader->Bind_Matrix(WorldMatrix, &world)) ||
		FAILED(m_Shader->Begin("ShockRing")) ||
		FAILED(m_Buffer->Bind_Resources()))
		return E_FAIL;

	return m_Buffer->Render();
}

void Pl0000Shockwave::OnCollisionEnter(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{

}

void Pl0000Shockwave::OnCollisionStay(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
	if (m_IsDestroy || m_Age >= HitWindow) return;
	auto target = targetCollider->Get_Owner();
	
	if (target->Get_LayerMask().Get_Layer() != m_MonsterLayerIndex) return;
	auto entity = target;
	if (target->Get_GameObjectType() == GAMEOBJECTTYPE::PART)
		entity = static_pointer_cast<PartObject>(target)->Get_Owner();
	auto monster = Cast<Monster>(entity);
	if (!monster) return;
	const RuntimeObjectId targetID = monster->Get_RuntimeObjectId();
	if (!m_HitEntities.contains(targetID))
	{
		m_HitEntities.insert(targetID);
		m_DamageInfo.hitPosition = targetCollider->ClosestPoint(ownCollider->Get_Pivot());
		
		monster->TakeDamage(m_DamageInfo);
	}
}

void Pl0000Shockwave::OnCollisionExit(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
	
}

HRESULT Pl0000Shockwave::Ready_Components(const PLAYER_SHOCKWAVE_DESC& desc)
{
	SphereCollider::SPHERE_COLLIDER_DESC colDesc{};
	colDesc.radius = desc.radius;
	colDesc.offset = Vector3::Zero;

	m_ShockwaveCollider = Add_Component<SphereCollider>(ETOI(LEVEL::STATIC), &colDesc);
	if (nullptr == m_ShockwaveCollider)
		return E_FAIL;

	Shader::SHADER_DESC shaderDesc{ VTXPARTICLE_POINT_DESC::Tag, VTXPARTICLE_POINT_DESC::Elements, VTXPARTICLE_POINT_DESC::numElements };
	m_Shader = Add_Component<Shader>(ETOI(LEVEL::GAMEPLAY), &shaderDesc);
	if (nullptr == m_Shader)
		return E_FAIL;

	// One point at the world matrix's origin; GS_Quad spans it by the world matrix.
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

	return S_OK;
}

void Pl0000Shockwave::Raise_GroundDust() const
{
	// The burst starts at the frame's centre (0.54 of its height) and spreads out from there, so that point is put on the
	// floor: the floor hides the lower half and the dust bursts up out of the ground.
	ExplodeEffect::EXPLODE_EFFECT_DESC dustDesc{};
	dustDesc.size = 5.f;
	dustDesc.position = m_Ground + Vector3{ 0.f, dustDesc.size * 0.04f, 0.f };
	dustDesc.textureTag = L"Effect_GroundDust";
	// The frames are a grey beige (about 160/158/145); lifted to a pale dust grey and thinned to sit in the world.
	dustDesc.tint = Vector4{ 1.25f, 1.25f, 1.3f, 0.3f };
	dustDesc.upright = true;
	// 64 frames in 0.71 s, thinning out from a third of the way in.
	dustDesc.framesPerSecond = 90.f;
	dustDesc.fadeStart = 0.35f;
	GAME_INSTANCE->Instantiate<ExplodeEffect>(L"ExplodeEffect", Get_LevelIndex(), &dustDesc);
}

Shared<Pl0000Shockwave> Pl0000Shockwave::Create(const ComPtr<ID3D11Device>& device,
                                                const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<Pl0000Shockwave>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Pl0000Shockwave");
		return nullptr;
	}

	return prototype;
}

Shared<GameObject> Pl0000Shockwave::Clone(void* arg)
{
	auto instance = make_shared<Pl0000Shockwave>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : Pl0000Shockwave");
		return nullptr;
	}

	return instance;
}


