#include "pch.h"
#include "Em0000.h"

#include <Game.h>
#include <SpdLogger.h>
#include <SphereCollider.h>

#include "Em0000Body.h"
#include "Em0010Movement.h"
#include "HpBarWorldUI.h"
#include "MonsterAOE.h"
#include "MonsterSight.h"
#include "MonsterStateMachine.h"
#include "Random_Helper.h"
#include "StateEm0000_Attack.h"
#include "StateEm0000_Await.h"
#include "StateEm0000_Chase.h"
#include "StateEm0000_Dead.h"
#include "StateEm0000_Hit.h"
#include "StateEm0000_Idle.h"

Em0000::Em0000() : Monster{} {}
Em0000::Em0000(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: Monster{ device, context } {}
Em0000::Em0000(const Em0000& rhs)
	: Monster{ rhs } {}

Em0000::~Em0000()
{
	m_MainBody.reset();
}

HRESULT Em0000::Initialize_Prototype()
{
	return Monster::Initialize_Prototype();
}

HRESULT Em0000::Initialize(void* arg)
{
	if (FAILED(Monster::Initialize(arg)))
	{
		LOG_ERROR(L"Failed to Initialize em0000");
		return E_FAIL;
	}
	m_MaxHp = 1000.f;
	m_Hp = m_MaxHp;

	if (FAILED(Ready_PartObjects()))
	{
		LOG_ERROR(L"Failed to Ready PartObjects em0000");
		return E_FAIL;
	}

	if (FAILED(Ready_Components()))
	{
		LOG_ERROR(L"Failed to Ready Components em0000");
		return E_FAIL;
	}

	return S_OK;
}

void Em0000::On_Destroy()
{
	m_MainBody.reset();
	Monster::On_Destroy();
}

void Em0000::Priority_Update(Float timeDelta)
{
}

void Em0000::Update(Float timeDelta)
{
	m_States->Update_State(timeDelta);
	m_Movement->Update_Movement(timeDelta);
	m_Transform->Update_WorldMatrix();
	m_PhysicalZone->Update(m_Transform->Get_WorldMatrix());
}

void Em0000::Late_Update(Float timeDelta)
{
}

void Em0000::Fixed_Update(Float fixedDelta)
{
}

HRESULT Em0000::Render()
{
	return S_OK;
}

void Em0000::Submit_RenderGroup()
{
	GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::NONBLEND, shared_from_this());
}

Em0000::EM0000_STATE Em0000::Choose_Attack(Float distance) const
{
	// From the clips' root motion: the punch steps 2 m in, the swing lunges 0.85 m and sweeps the arms past it, the
	// tackle (only rolled from Chase beyond melee reach) covers about 9.5 m in all. Measured at the body's own size.
	const Float bodyDistance = distance / m_Transform->Get_Scale().x;
	if (bodyDistance <= 2.2f)
		return Helper::Random_Bool(0.7f) ? EM0000_STATE::PUNCH : EM0000_STATE::SWING_LUNGE;
	if (bodyDistance <= m_AttackReach.melee)
		return Helper::Random_Bool(0.7f) ? EM0000_STATE::SWING_LUNGE : EM0000_STATE::PUNCH;
	return EM0000_STATE::TACKLE_START;
}

Em0000::ATTACK_REACH Em0000::Get_AttackReach() const
{
	const Float scale = m_Transform->Get_Scale().x;
	return { m_AttackReach.melee * scale, m_AttackReach.farMin * scale, m_AttackReach.farMax * scale, m_AttackReach.farChance };
}

const TRANSFORM_FRAME& Em0000::Get_BodyModelTransform() const
{
	return m_MainBody->Get_ModelTransform();
}

void Em0000::TakeDamage(const DAMAGE_INFO& dmgInfo)
{
	if (Is_Dead()) return;

	Play_HitSFX(dmgInfo);
	DisplaySparkEffect(dmgInfo.attackType, dmgInfo.hitPosition, dmgInfo.hitRotation);

	// Pod bullets (no groggy weight) only chip HP; the pod laser staggers like a sword hit.
	if (dmgInfo.attackType != ATK_TYPE::POD || dmgInfo.groggyWeight > 0.f)
		m_States->Change_State(MonsterStateMachine::MONSTER_STATE::Hit);

	Monster::TakeDamage(dmgInfo);
}

void Em0000::Launch_Up(Float height)
{
	if (Is_Dead() || !m_Movement) return;
	m_Movement->Begin_EaseOutAscent(height, 0.2f);
	if (m_States->Get_CurMonsterState() == MonsterStateMachine::MONSTER_STATE::Hit)
		static_pointer_cast<StateEm0000_Hit>(m_States->Get_CurrentState())->Start_AirLaunch();
}

void Em0000::Slam_Down(const Shared<GameObject>& plunger)
{
	if (Is_Dead() || !m_Movement || m_Movement->Is_Grounded()) return;
	if (m_States->Get_CurMonsterState() == MonsterStateMachine::MONSTER_STATE::Hit)
		static_pointer_cast<StateEm0000_Hit>(m_States->Get_CurrentState())->Start_SlamDown(plunger);
}

void Em0000::OnDeath()
{
	m_Movement->Set_Gravity(30.f);
	m_Movement->Reset_VerticalVelocity();
	// A dying machine no longer collides, pushes, blocks or shows its HP bar.
	m_MainBody->Off_Colliders();
	m_PhysicalZone->Set_Active(false);
	if (m_HpBarUI)
		m_HpBarUI->Set_Active(false);
	m_States->Change_State(MonsterStateMachine::MONSTER_STATE::DEAD);
}

void Em0000::OnAttackHit(const Shared<GameObject>& target)
{
	if (m_Movement)
		m_Movement->Reduce_RootMotion();
}

void Em0000::Apply_PushoutCorrection(const Vector3& correction)
{
	if (m_Movement)
		m_Movement->Add_Correction(correction);
}

void Em0000::OnCollisionEnter(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
}

void Em0000::OnCollisionStay(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
	Monster::OnCollisionStay(ownCollider, targetCollider);
}

void Em0000::OnCollisionExit(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
}

HRESULT Em0000::Ready_PartObjects()
{
	const wstring& prefix = m_PartPrefix;
	Em0000Body::EM0000BODY_DESC bodyDesc{};
	if (FAILED(Add_Child(Get_LevelIndex(), prefix + L"Body", prefix + L"Body", &bodyDesc)))
		return E_FAIL;

	MonsterSight::MONSTER_SIGHT_DESC sightDesc{};
	sightDesc.radius = 10.f;
	sightDesc.offset = Vector3::Zero;
	if (FAILED(Add_Child(Get_LevelIndex(), L"MonsterSight", prefix + L"Sight", &sightDesc)))
		return E_FAIL;

	m_MainBody = static_pointer_cast<Em0000Body>(Find_Child(prefix + L"Body"));
	if (nullptr == m_MainBody)
		return E_FAIL;

	// Fists hit like Em0010's arms; the tackle lands the whole body and knocks back harder.
	DAMAGE_INFO fistDamage{};
	fistDamage.attacker = shared_from_this();
	fistDamage.attackType = ATK_TYPE::HEAVY;
	fistDamage.damage = 30.f;
	fistDamage.groggyWeight = 10.f;
	fistDamage.knockbackForce = 1.f;
	DAMAGE_INFO tackleDamage{ fistDamage };
	tackleDamage.damage = 40.f;
	tackleDamage.knockbackForce = 2.f;

	const struct { const tChar* part; const Char* bone; Float radius; const DAMAGE_INFO& damage; } volumes[] = {
		{ L"LeftArm", "bone304", 0.5f, fistDamage },
		{ L"RightArm", "bone560", 0.5f, fistDamage },
		{ L"Torso", "bone3", 0.8f, tackleDamage },
	};
	for (const auto& volume : volumes)
	{
		MonsterAOE::MONSTER_AOE_DESC aoeDesc{};
		aoeDesc.model = m_MainBody->Get_ModelComponent();
		aoeDesc.offset = Vector3::Zero;
		aoeDesc.radius = volume.radius;
		aoeDesc.targetBoneName = volume.bone;
		aoeDesc.dmgInfo = volume.damage;
		if (FAILED(Add_Child(Get_LevelIndex(), L"MonsterAOE", prefix + volume.part, &aoeDesc)))
			return E_FAIL;
	}

	HpBarWorldUI::HP_BAR_WORLD_UI_DESC hpBarDesc{};
	hpBarDesc.target = static_pointer_cast<Entity>(shared_from_this());
	hpBarDesc.worldOffset = Vector3{ 0.f, m_HpBarHeight, 0.f };
	hpBarDesc.anchor = UI_ANCHOR::TOP_LEFT;
	hpBarDesc.sizeX = 160.f;
	hpBarDesc.sizeY = 8.f;
	if (FAILED(Add_Child(Get_LevelIndex(), L"HpBarWorldUI", prefix + L"HpBar", &hpBarDesc)))
		return E_FAIL;
	m_HpBarUI = static_pointer_cast<HpBarWorldUI>(Find_Child(prefix + L"HpBar"));

	if (FAILED(m_MainBody->Begin()))
	{
		LOG_ERROR(L"Failed to Begin {}Body", prefix);
		return E_FAIL;
	}

	return S_OK;
}

HRESULT Em0000::Ready_Components()
{
	Em0010Movement::EM0010_MOVEMENT_DESC movementDesc{};
	movementDesc.velocity = Vector3::Zero;
	movementDesc.moveSpeed = 0.f;
	movementDesc.targetDirection = Vector3::Zero;
	movementDesc.turnSpeed = 7.5f;
	m_Movement = Add_Component<Em0010Movement>(ETOI(LEVEL::GAMEPLAY), &movementDesc);
	if (nullptr == m_Movement)
		return E_FAIL;

	// The state machine comes last: its states look up the body and movement.
	m_States = Add_Component<MonsterStateMachine>(ETOI(LEVEL::GAMEPLAY));
	if (nullptr == m_States)
		return E_FAIL;

	using msState = MonsterStateMachine::MONSTER_STATE;
	const auto em0000 = static_pointer_cast<Em0000>(shared_from_this());
	const auto tag = [](msState state) { return Helper::To_wString(magic_enum::enum_name(state)); };
	if (FAILED(m_States->Add_State(StateEm0000_Idle::Create(tag(msState::IDLE), em0000))) ||
		FAILED(m_States->Add_State(StateEm0000_Hit::Create(tag(msState::Hit), em0000))) ||
		FAILED(m_States->Add_State(StateEm0000_Chase::Create(tag(msState::CHASE), em0000))) ||
		FAILED(m_States->Add_State(StateEm0000_Attack::Create(tag(msState::ATTACK), em0000))) ||
		FAILED(m_States->Add_State(StateEm0000_Await::Create(tag(msState::AWAIT), em0000))) ||
		FAILED(m_States->Add_State(StateEm0000_Dead::Create(tag(msState::DEAD), em0000))))
		return E_FAIL;
	m_States->Change_State(msState::IDLE);

	m_Movement->Begin();

	SphereCollider::SPHERE_COLLIDER_DESC physicalZoneDesc{};
	physicalZoneDesc.offset = Vector3{ 0.f, 0.7f, 0.f };
	physicalZoneDesc.radius = 0.55f;
	m_PhysicalZone = Add_Component<SphereCollider>(ETOI(LEVEL::STATIC), &physicalZoneDesc);
	if (nullptr == m_PhysicalZone)
		return E_FAIL;

	return S_OK;
}

Shared<Em0000> Em0000::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<Em0000>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Em0000");
		return nullptr;
	}

	return prototype;
}

Shared<GameObject> Em0000::Clone(void* arg)
{
	auto instance = make_shared<Em0000>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : Em0000");
		return nullptr;
	}

	return instance;
}
