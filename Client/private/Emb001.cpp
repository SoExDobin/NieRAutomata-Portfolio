#include "pch.h"
#include "Emb001.h"
#include "ClientApp.h"
#include "UIManager.h"

#include <Camera.h>
#include <Game.h>
#include <Light.h>
#include <Model.h>
#include <SpdLogger.h>

#include "Bullet.h"
#include "Em0000Body.h"
#include "HpBarWorldUI.h"
#include "MonsterStateMachine.h"
#include "Random_Helper.h"

Emb001::Emb001() : Em0000{} {}
Emb001::Emb001(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: Em0000{ device, context } {}
Emb001::Emb001(const Emb001& rhs)
	: Em0000{ rhs } {}

HRESULT Emb001::Initialize(void* arg)
{
	// Reach at the body's own size (Get_AttackReach scales it): the headbutt steps 2 m in and the punch reaches about as
	// far, so it strikes from 2.5; the trumpet (EmInfo statue trumpet attack, 2 to 20 m) from 3.5 to 8, rolled each second.
	m_PartPrefix = L"Emb001";
	m_HpBarHeight = 2.3f * BodyScale;
	m_AttackReach = { 2.5f, 3.5f, 8.f, 0.5f };
	if (FAILED(Em0000::Initialize(arg)))
	{
		LOG_ERROR(L"Failed to Initialize emb001");
		return E_FAIL;
	}
	m_IsStatic = true;
	m_MaxHp = 6000.f;
	m_Hp = m_MaxHp;
	// A new one comes in at the reference size; a scene keeps its own Transform scale.
	m_Transform->Set_Scale(BodyScale, BodyScale, BodyScale);

	m_TrumpetBoneIndex = m_MainBody->Get_ModelComponent()->Get_BoneIndexByName("bone307");
	if (m_TrumpetBoneIndex < 0)
	{
		LOG_ERROR(L"Failed to Find emb001 trumpet bone307");
		return E_FAIL;
	}

	// A statue shows no HP bar until it wakes.
	if (m_HpBarUI)
		m_HpBarUI->Set_Active(false);

	return S_OK;
}

void Emb001::On_Destroy()
{
	Clear_FountainSpots();
	Em0000::On_Destroy();
}

void Emb001::Update(Float timeDelta)
{
	Em0000::Update(timeDelta);
	Blow_Trumpet(timeDelta);
	if (!m_FountainSpotsLit && !Is_Dead())
		Light_FountainSpots();
}

void Emb001::Light_FountainSpots()
{
	const Shared<Camera> camera = GAME_INSTANCE->Get_MainCamera();
	if (!camera)
		return;
	const Vector3 feet = m_Transform->Get_Position();
	const Vector3 chest = feet + Vector3{ 0.f, SpotAimHeight, 0.f };
	if (Vector3::Distance(camera->Get_Transform()->Get_Position(), chest) > SpotLookDistance ||
		!GAME_INSTANCE->Is_Visible(BoundingBox{ chest, Vector3{ 2.f, SpotAimHeight, 2.f } }))
		return;
	m_FountainSpotsLit = true;

	for (const Vector3& source : FountainLights)
	{
		Vector3 direction = chest - source;
		direction.Normalize();
		LIGHT_DESC desc{};
		desc.type = LIGHT::SPOT;
		desc.position = Vector4{ source.x, source.y, source.z, 1.f };
		desc.direction = Vector4{ direction.x, direction.y, direction.z, 0.f };
		desc.diffuse = SpotColor;
		desc.specular = SpotColor;
		desc.range = SpotRange;
		desc.innerCone = XMConvertToRadians(SpotInner);
		desc.outerCone = XMConvertToRadians(SpotOuter);
		desc.beam = SpotBeam;
		const Shared<Light> spot = Light::Create(desc);
		if (nullptr == spot || FAILED(GAME_INSTANCE->Add_Light(spot)))
		{
			LOG_ERROR(L"Failed to add a fountain spot of emb001 {}", m_ObjectName);
			continue;
		}
		m_FountainSpots.push_back(spot);
	}
	LOG_INFO(L"emb001 {}: fountain spots on ({})", m_ObjectName, m_FountainSpots.size());
}

void Emb001::Clear_FountainSpots()
{
	for (const Shared<Light>& spot : m_FountainSpots)
		GAME_INSTANCE->Remove_Light(spot);
	m_FountainSpots.clear();
}

Em0000::EM0000_STATE Emb001::Choose_Attack(Float distance) const
{
	// Up close: the headbutt (0500) or the one-handed punch (0510), now and then a step back (0041) to blow the trumpet
	// from further out; beyond melee reach, the trumpet.
	if (distance > Get_AttackReach().melee)
		return EM0000_STATE::TRUMPET_START;
	const Float roll = static_cast<Float>(Helper::Random_Double(0.0, 1.0));
	if (roll < 0.2f)
		return EM0000_STATE::ESCAPE_BACK;
	return roll < 0.6f ? EM0000_STATE::PUNCH : EM0000_STATE::SWING_LUNGE;
}

Float Emb001::Get_AnimationRate() const
{
	return m_States && m_States->Get_CurMonsterState() == MonsterStateMachine::MONSTER_STATE::ATTACK ? m_AttackSpeed : 1.f;
}

void Emb001::TakeDamage(const DAMAGE_INFO& dmgInfo)
{
	if (Is_Dead()) return;

	Play_HitSFX(dmgInfo);
	DisplaySparkEffect(dmgInfo.attackType, dmgInfo.hitPosition, dmgInfo.hitRotation);

	if (m_Dormant)
	{
		// The statue rings under the blows but loses no HP; enough of them wake it facing the attacker.
		if (m_TargetPlayer.expired())
			m_TargetPlayer = dmgInfo.attacker;
		m_DormantDamage += dmgInfo.damage;
		if (m_DormantDamage >= m_WakeDamage)
		{
			m_Dormant = false;
			if (const auto client = ClientApp::GetInstance())
				client->Get_UIManager()->Queue_Scenario(COMMUNICATION_CUE::RABBIT_ENCOUNTER);
			if (m_HpBarUI)
				m_HpBarUI->Set_Active(true);
		}
		return;
	}

	// Super armour throughout: the hit lands without a stagger or knockback.
	const Float previousHp = m_Hp;
	Monster::TakeDamage(dmgInfo);
	// The rabbit complains once as its HP first drops past 75% and once past 50%.
	if (!Is_Dead())
		if (const auto client = ClientApp::GetInstance())
		{
			if (previousHp > m_MaxHp * .75f && m_Hp <= m_MaxHp * .75f)
				client->Get_UIManager()->Queue_Scenario(COMMUNICATION_CUE::RABBIT_TAUNT);
			if (previousHp > m_MaxHp * .5f && m_Hp <= m_MaxHp * .5f)
				client->Get_UIManager()->Queue_Scenario(COMMUNICATION_CUE::RABBIT_SULK);
		}
}

void Emb001::OnDeath()
{
	Clear_FountainSpots();
	Em0000::OnDeath();
	if (const auto client = ClientApp::GetInstance())
		client->Get_UIManager()->Queue_Scenario(COMMUNICATION_CUE::AFTER_RABBIT);
}

void Emb001::Blow_Trumpet(Float timeDelta)
{
	const Shared<GameObject> target = m_TargetPlayer.lock();
	if (!target || Is_Dead() || m_Dormant ||
		m_States->Get_CurMonsterState() != MonsterStateMachine::MONSTER_STATE::ATTACK ||
		m_MainBody->Get_CurrentAnimationIndex() != ETOI(EM0000_STATE::STATUE_POSE))
	{
		m_TrumpetTimer = 0.f;
		return;
	}

	m_TrumpetTimer -= timeDelta;
	if (m_TrumpetTimer > 0.f)
		return;
	m_TrumpetTimer += TrumpetInterval;

	const Matrix bell = m_MainBody->Get_ModelComponent()->Get_BoneMatrix(m_TrumpetBoneIndex) *
		m_MainBody->Get_CombinedWorldMatrix();
	const Vector3 muzzle = Vector3::Transform(TrumpetBell, bell);
	Vector3 aim = target->Get_Transform()->Get_Position() + Vector3{ 0.f, 1.f, 0.f } - muzzle;
	aim.Normalize();
	// Spread evenly over the cone's disc: the off-axis angle grows with the square root of a uniform roll.
	Vector3 across = aim.Cross(Vector3::UnitY);
	if (across.LengthSquared() < 1e-6f) across = Vector3::UnitX;
	across.Normalize();
	const Vector3 up = across.Cross(aim);
	const Float off = XMConvertToRadians(OrbSpread) * sqrtf(static_cast<Float>(Helper::Random_Double(0.0, 1.0)));
	const Float around = static_cast<Float>(Helper::Random_Double(0.0, XM_2PI));
	Vector3 direction = aim * cosf(off) + (across * cosf(around) + up * sinf(around)) * sinf(off);
	direction.Normalize();

	// Like Em3000's danmak: every fourth orb can't be cut down.
	Bullet::BULLET_DESC desc{};
	desc.initialPosition = muzzle;
	desc.direction = direction;
	desc.damageInfo.attacker = shared_from_this();
	desc.damageInfo.attackType = ATK_TYPE::LIGHT;
	desc.damageInfo.damage = OrbDamage;
	desc.resourceTag = L"candy";
	desc.targetLayer = L"Player";
	desc.isPermanent = (++m_TrumpetVolley % 4) == 0;
	desc.speed = OrbSpeed;
	desc.maxDistance = OrbRange;
	GAME_INSTANCE->Instantiate<Bullet>(L"Bullet", Get_LevelIndex(), &desc);
	GAME_INSTANCE->PlaySoundFXOnce(L"core_hak_em_shot", SOUNDCHANNEL::CHANNEL_19, 0.3f, &muzzle);
}

Shared<Emb001> Emb001::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<Emb001>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Emb001");
		return nullptr;
	}

	return prototype;
}

Shared<GameObject> Emb001::Clone(void* arg)
{
	auto instance = make_shared<Emb001>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : Emb001");
		return nullptr;
	}

	return instance;
}
