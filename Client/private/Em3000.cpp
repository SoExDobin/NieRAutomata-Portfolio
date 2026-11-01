#include "pch.h"
#include "Em3000.h"
#include "ClientApp.h"
#include "UIManager.h"

#include <Game.h>
#include <SpdLogger.h>

#include "Em3000Body.h"
#include "EmMovement.h"
#include "Em3001.h"
#include "Em3004.h"
#include "Random_Helper.h"
#include "Light.h"
#include "SphereCollider.h"
#include "HpBarWorldUI.h"
#include "MonsterAOE.h"
#include "MonsterHitBox.h"
#include "MonsterSight.h"
#include "MonsterStateMachine.h"
#include "StateEm3000_Chase.h"
#include "StateEm3000_Dead.h"
#include "StateEm3000_Groggy.h"
#include "StateEm3000_Idle.h"
#include "StateEm3000_Intro.h"
#include "StateEm3000_Melee.h"
#include "StateEm3000_Phase2Groggy.h"
#include "StateEm3000_Puppet.h"
#include "StateEm3000_Range.h"
#include "StateEm3000_Stomp.h"
#include "StateEm3000_Transform.h"

namespace
{
	// The stage spots in the stage frame (x right, y up, z forward toward the entrance, the boss's at the reveal), hung
	// about 20 m up under the dome ceiling: the source and the aim are offsets from what the spot follows. Two on the
	// dress from the front and one on the mask follow the boss, one follows the player (lit only once cast), and two stay
	// on the curtain behind, as the original lights the opera. Their color, cones and beam are Inspector properties of Em3000.
	// After Cast_StageSpots a spot follows `cast` and aims at castAim instead (the curtain ones turn onto the cast).
	enum class SPOT_FOLLOW { BOSS, PLAYER, STAGE };
	struct STAGE_SPOT
	{
		SPOT_FOLLOW follow;
		Vector3 source;
		Vector3 aim;
		SPOT_FOLLOW cast;
		Vector3 castAim;
	};
	const STAGE_SPOT StageSpots[]
	{
		{ SPOT_FOLLOW::BOSS, { -5.f, 22.f, 6.f }, { -0.8f, 3.f, 0.f }, SPOT_FOLLOW::BOSS, { -0.8f, 3.f, 0.f } },
		{ SPOT_FOLLOW::BOSS, { 5.f, 22.f, 6.f }, { 0.8f, 3.f, 0.f }, SPOT_FOLLOW::BOSS, { 0.8f, 3.f, 0.f } },
		{ SPOT_FOLLOW::BOSS, { 0.f, 22.f, 4.f }, { 0.f, 6.5f, 0.f }, SPOT_FOLLOW::BOSS, { 0.f, 6.5f, 0.f } },
		{ SPOT_FOLLOW::PLAYER, { 0.f, 18.f, 5.f }, { 0.f, 1.f, 0.f }, SPOT_FOLLOW::PLAYER, { 0.f, 1.f, 0.f } },
		{ SPOT_FOLLOW::STAGE, { -9.f, 20.f, -14.f }, { -9.f, 0.f, -22.f }, SPOT_FOLLOW::PLAYER, { 0.f, 1.f, 0.f } },
		{ SPOT_FOLLOW::STAGE, { 9.f, 20.f, -14.f }, { 9.f, 0.f, -22.f }, SPOT_FOLLOW::BOSS, { 0.f, 3.f, 0.f } },
	};
	constexpr Float StageSpotFollowRate = 5.f;	// 1/s: a follow spot closes 63% of the way in 0.2 s
	constexpr Float StageSpotTintRate = 3.f;	// 1/s: a new spot color is 95% there in 1 s
}

Em3000::Em3000(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: Monster{device, context} {}
Em3000::Em3000(const Em3000& rhs)
	: Monster{ rhs }, m_StageSpotColor{ rhs.m_StageSpotColor }, m_StageSpotIntensity{ rhs.m_StageSpotIntensity },
	m_StageSpotInner{ rhs.m_StageSpotInner }, m_StageSpotOuter{ rhs.m_StageSpotOuter }, m_StageSpotRange{ rhs.m_StageSpotRange },
	m_StageSpotBeam{ rhs.m_StageSpotBeam } { }

HRESULT Em3000::Initialize_Prototype()
{
	return Monster::Initialize_Prototype();
}

HRESULT Em3000::Initialize(void* arg)
{
	if (FAILED(Monster::Initialize(arg)))
	{
		LOG_ERROR(L"Failed to Initialize Em3000");
		return E_FAIL;
	}

	if (FAILED(Ready_PartObjects()))
	{
		LOG_ERROR(L"Failed to Ready_PartObjects Em3000");
		return E_FAIL;
	}

	if (FAILED(Ready_Components()))
	{
		LOG_ERROR(L"Failed to Ready_Components Em3000");
		return E_FAIL;
	}

	m_IsStatic = true;
	m_MaxHp = 20000.f;
	m_Hp = m_MaxHp;

	return S_OK;
}

void Em3000::On_Destroy()
{
	for (const Shared<Light>& spot : m_StageSpots)
		GAME_INSTANCE->Remove_Light(spot);
	m_StageSpots.clear();
	m_StageSpotAims.clear();
	// The Editor's Stop and a level change destroy the boss but leave 2D channels playing.
	GAME_INSTANCE->StopSound(BgmChannel);
	GAME_INSTANCE->StopSound(BgmIntroChannel);
	Monster::On_Destroy();
}

void Em3000::Priority_Update(Float timeDelta)
{
	// Half HP ends phase 1: the boss staggers, then transforms (StateEm3000_Groggy).
	if (m_IsSecondPhase || Is_Dead() || m_Hp > m_MaxHp * 0.5f)
		return;
	if (Change_State(EM3000_STATE::GROGGY))
		m_IsSecondPhase = true;
}

void Em3000::Update(Float timeDelta)
{
	m_States->Update_State(timeDelta);
	// Off once the boss has broken up (StateEm3000_Dead): the wreck lies scattered, with no capsule where it stood.
	if (m_EmMovement->Is_Active())
		m_EmMovement->Update_Movement(timeDelta);
	/* Get_Position reads the world matrix, so it is refreshed after the move. Refreshed before it, the states steered
	   from the previous frame's spot: the mixer's circle overshot by a frame's step every frame and shook. */
	m_Transform->Update_WorldMatrix();
	m_PhysicalZone->Update(*m_Transform->Get_WorldMatrixPtr());
	Update_StageSpots(timeDelta);
}

void Em3000::Late_Update(Float timeDelta)
{

}

void Em3000::Fixed_Update(Float fixedDelta)
{
	
}

HRESULT Em3000::Render()
{
	return S_OK;
}

void Em3000::Submit_RenderGroup()
{

}

const TRANSFORM_FRAME& Em3000::Get_BodyModelTransform() const
{
	return m_MainBody->Get_ModelTransform();
}

void Em3000::TakeDamage(const DAMAGE_INFO& dmgInfo)
{
	if (Is_Dead()) return;

	Play_HitSFX(dmgInfo);
	DisplaySparkEffect(dmgInfo.attackType, dmgInfo.hitPosition, dmgInfo.hitRotation);
	// Blades only: the Pod's stream of shots would keep her crying.
	if (dmgInfo.attackType != ATK_TYPE::POD)
		m_MainBody->Play_HurtVoice();

	// The boss has no flinch: hits only take HP.
	const Float previousHp = m_Hp;
	Monster::TakeDamage(dmgInfo);
	if (!Is_Dead())
		if (const auto client = ClientApp::GetInstance())
		{
			if (!m_IsSecondPhase && previousHp > m_MaxHp * .8f && m_Hp <= m_MaxHp * .8f)
				client->Get_UIManager()->Queue_Scenario(COMMUNICATION_CUE::EM3000_COMBAT);
			if (m_IsSecondPhase && previousHp > m_MaxHp * .2f && m_Hp <= m_MaxHp * .2f)
				client->Get_UIManager()->Queue_Scenario(COMMUNICATION_CUE::EM3000_CRITICAL);
		}
}

Bool Em3000::Deflects(const Shared<GameObject>& hitPart) const
{
	// The dress's skirt (the body part's sphere) turns Pod shots aside until the phase 1 stagger strips the dress.
	return hitPart == m_MainBody && !m_IsSecondPhase && !Is_Dead();
}

void Em3000::OnDeath()
{
	m_PhysicalZone->Set_Active(false);
	m_MainBody->Off_Colliders();
	if (const Shared<GameObject> upper = Find_Child(L"Em3000UpperHitBox"))
		upper->Set_Active(false);
	if (m_HpBarUI)
		m_HpBarUI->Set_Active(false);
	m_MainBody->End_Attacks();
	Burst_Puppets();
	Change_State(EM3000_STATE::DEAD);
}

void Em3000::Light_StageSpots(const Vector3& origin, const Vector3& forward)
{
	if (!m_StageSpots.empty())
		return;
	m_StageOrigin = origin;
	m_StageForward = forward;
	m_StageForward.y = 0.f;
	m_StageForward.Normalize();
	m_StageRight = Vector3::UnitY.Cross(m_StageForward);
	m_SpotPlayer = Get_Target();

	for (size_t index = 0; index < size(StageSpots); ++index)
	{
		LIGHT_DESC desc{};
		desc.type = LIGHT::SPOT;
		const Shared<Light> spot = Light::Create(desc);
		if (nullptr == spot || FAILED(GAME_INSTANCE->Add_Light(spot)))
		{
			LOG_ERROR(L"Failed to add a stage spot of Em3000 {}", m_ObjectName);
			m_StageSpots.clear();
			return;
		}
		m_StageSpots.push_back(spot);
	}
	m_StageSpotAims.assign(m_StageSpots.size(), Vector3::Zero);
	Update_StageSpots(0.f);
	LOG_INFO(L"Em3000 {}: stage spots on ({})", m_ObjectName, m_StageSpots.size());
}

void Em3000::Update_StageSpots(Float timeDelta)
{
	if (m_StageSpots.empty())
		return;
	// The first placement (no time yet) snaps; after that the spots ease after what they follow.
	const Float follow = timeDelta > 0.f ? 1.f - expf(-StageSpotFollowRate * timeDelta) : 1.f;
	const auto toStage = [&](const Vector3& offset) {
		return m_StageRight * offset.x + Vector3::UnitY * offset.y + m_StageForward * offset.z;
	};
	const Shared<GameObject> player = m_SpotPlayer.lock();
	const Color& shade = m_IsStageSpotTinted ? m_StageSpotTint : m_StageSpotColor;
	m_StageSpotShown = timeDelta > 0.f ? Color::Lerp(m_StageSpotShown, shade, 1.f - expf(-StageSpotTintRate * timeDelta)) : shade;
	Color color = m_StageSpotShown * m_StageSpotIntensity;
	color.w = 1.f;

	for (size_t index = 0; index < m_StageSpots.size(); ++index)
	{
		const STAGE_SPOT& layout = StageSpots[index];
		const Shared<Light>& spot = m_StageSpots[index];
		const SPOT_FOLLOW follows = m_SpotsOnCast ? layout.cast : layout.follow;
		const Vector3& aim = m_SpotsOnCast ? layout.castAim : layout.aim;
		Vector3 anchor = m_StageOrigin;
		if (follows == SPOT_FOLLOW::BOSS)
			anchor = m_Transform->Get_Position();
		else if (follows == SPOT_FOLLOW::PLAYER)
		{
			// The player stays out of the light until the boss has come down (reference 0:36).
			spot->Set_Active(nullptr != player && m_SpotsOnCast);
			if (!player)
				continue;
			anchor = player->Get_Transform()->Get_Position();
		}

		LIGHT_DESC desc = spot->Get_LightDesc();
		const Vector3 source = Vector3::Lerp(Vector3{ desc.position.x, desc.position.y, desc.position.z },
			anchor + toStage(layout.source), follow);
		m_StageSpotAims[index] = Vector3::Lerp(m_StageSpotAims[index], anchor + toStage(aim), follow);
		Vector3 direction = m_StageSpotAims[index] - source;
		direction.Normalize();
		desc.position = Vector4{ source.x, source.y, source.z, 1.f };
		desc.direction = Vector4{ direction.x, direction.y, direction.z, 0.f };
		desc.diffuse = color;
		desc.specular = color;
		desc.range = m_StageSpotRange;
		desc.outerCone = XMConvertToRadians(m_StageSpotOuter);
		desc.innerCone = XMConvertToRadians((std::min)(m_StageSpotInner, m_StageSpotOuter));
		desc.beam = m_StageSpotBeam;
		spot->Set_LightDesc(desc);
	}
}

Bool Em3000::Change_State(EM3000_STATE state)
{
	return m_States->Change_StateByTag(Helper::To_wString(magic_enum::enum_name(state)));
}

Bool Em3000::Is_State(EM3000_STATE state) const
{
	const Shared<State> current = m_States ? m_States->Get_CurrentState() : nullptr;
	return current && current->IsSameTag(Helper::To_wString(magic_enum::enum_name(state)));
}

void Em3000::Summon_Puppets()
{
	std::erase_if(m_Puppets, [](const Weak<Em3004>& puppet) { return puppet.expired() || puppet.lock()->Is_Dead(); });
	const Shared<GameObject> target = Get_Target();
	if (!target || m_Puppets.size() >= MaxPuppets)
		return;

	// Evenly round the player from a random start, so the drops never land on each other. Each point must stand on the
	// player's floor, short of any wall: past the dome's wall or off its floor it comes in toward the player. Each puppet
	// starts high enough that the fall clip's first frame hangs its head just under the ceiling, and drops onto that
	// floor (StateEm3004_Fall).
	const size_t previousCount = m_Puppets.size();
	const uint32 count = MaxPuppets - static_cast<uint32>(previousCount);
	const Vector3 center = target->Get_Transform()->Get_Position();
	const Vector3 chest = center + Vector3{ 0.f, 1.f, 0.f };
	Vector3 hit{};
	if (!GAME_INSTANCE->Raycast_Physics(chest, -Vector3::UnitY, PuppetDropHeight, hit))
		return;		// the player is off the floor (a long fall): no ring to drop onto
	const Float floorY = hit.y;
	const Float start = static_cast<Float>(Helper::Random_Double(0., XM_2PI));
	for (uint32 i = 0; i < count; ++i)
	{
		const Float angle = start + XM_2PI * i / count;
		const Vector3 direction{ sinf(angle), 0.f, cosf(angle) };
		const Vector3 eye{ chest.x, floorY + 1.f, chest.z };
		Float reach = PuppetRingRadius;
		if (GAME_INSTANCE->Raycast_Physics(eye, direction, PuppetRingRadius + PuppetWallMargin, hit))
			reach = Vector3::Distance(eye, hit) - PuppetWallMargin;
		Bool found = false;
		Vector3 position{};
		for (Float radius = reach; radius > 0.f && !found; radius -= PuppetWallMargin)
		{
			position = eye + direction * radius;
			found = GAME_INSTANCE->Raycast_Physics(position, -Vector3::UnitY, 1.f + PuppetFloorStep, hit) &&
				fabsf(hit.y - floorY) <= PuppetFloorStep;
		}
		if (!found)
		{
			LOG_WARN(L"Em3000 {}: no floor for a puppet at {} deg", m_ObjectName, XMConvertToDegrees(angle));
			continue;
		}
		position.y = hit.y;
		Float top = position.y + PuppetDropHeight;
		if (GAME_INSTANCE->Raycast_Physics(position + Vector3{ 0.f, 1.f, 0.f }, Vector3::UnitY, PuppetDropHeight, hit))
			top = (std::min)(top, hit.y - PuppetCeilingGap);
		position.y = (std::max)(top - Em3004::HeadHeight - Em3004::FallClipDrop, position.y);
		Em3004::EM3004_DESC desc{};
		const Shared<Em3004> puppet = GAME_INSTANCE->Instantiate<Em3004>(L"Em3004", Get_LevelIndex(), &desc);
		if (!puppet)
			continue;
		puppet->Get_Transform()->Set_Position(position);
		puppet->Get_Transform()->Set_Rotation(Quaternion::CreateFromYawPitchRoll(angle + XM_PI, 0.f, 0.f));
		// Get_Position reads the world matrix: without this its first update still finds it at the origin.
		puppet->Get_Transform()->Update_WorldMatrix();
		puppet->Set_Target(target);
		m_Puppets.push_back(puppet);
	}
	// Explain the scream only when it actually brought live puppets into the arena.
	if (m_Puppets.size() > previousCount)
		if (const auto client = ClientApp::GetInstance())
			client->Get_UIManager()->Queue_Scenario(COMMUNICATION_CUE::EM3000_PUPPETS);
}

Bool Em3000::Has_LivingPuppets() const
{
	return any_of(m_Puppets.begin(), m_Puppets.end(), [](const Weak<Em3004>& puppet) {
		const Shared<Em3004> alive = puppet.lock();
		return alive && !alive->Is_Dead();
	});
}

void Em3000::Burst_Puppets()
{
	for (const Weak<Em3004>& puppet : m_Puppets)
		if (const Shared<Em3004> alive = puppet.lock())
			alive->Burst();
	m_Puppets.clear();
}

void Em3000::OnCollisionEnter(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
	
}

void Em3000::OnCollisionStay(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
	Monster::OnCollisionStay(ownCollider, targetCollider);
}

void Em3000::OnCollisionExit(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
	
}

HRESULT Em3000::Ready_PartObjects()
{
	PartObject::PARTOBJECT_DESC bodyDesc{};
	if (FAILED(Add_Child(Get_LevelIndex(), L"Em3000Body", L"Em3000Body", &bodyDesc)))
		return E_FAIL;
	m_MainBody = static_pointer_cast<Em3000Body>(Find_Child(L"Em3000Body"));

	auto animMesh = Shader::SHADER_DESC{ VTXANIMMESH::Tag,  VTXANIMMESH::Elements, VTXANIMMESH::numElements };

	Em3000Parts::EM3000PART_DESC partsDesc{};
	partsDesc.shaderDesc = animMesh;
	partsDesc.modelResourceTag = L"em3001";
	// The two chains on the sides of the head hang from it: em3001's root bone0 sits exactly on em3000's head bone16 at rest.
	partsDesc.parentModel = m_MainBody->Get_ModelComponent();
	partsDesc.parentBoneName = "bone16";
	partsDesc.rootBoneName = "bone0";
	if (FAILED(Add_Child(Get_LevelIndex(), L"Em3001", L"Em3001", &partsDesc)))
		return E_FAIL;

	// The puppet strings fanning out from the head: em3002's root bone0 sits on the same head bone.
	partsDesc.modelResourceTag = L"em3002";
	if (FAILED(Add_Child(Get_LevelIndex(), L"Em3002", L"Em3002", &partsDesc)))
		return E_FAIL;

	// The red dress and its hanging puppets are modelled 4.3 m down from their root, which belongs on the pelvis bone0 (4.32 m).
	partsDesc.modelResourceTag = L"em3003";
	partsDesc.parentBoneName = "bone0";
	if (FAILED(Add_Child(Get_LevelIndex(), L"Em3003", L"Em3003", &partsDesc)))
		return E_FAIL;
	partsDesc.parentModel.reset();

	// Finds the player across the stage; melee range is a distance check on the found target.
	MonsterSight::MONSTER_SIGHT_DESC sightDesc{};
	sightDesc.radius = 20.f;
	sightDesc.offset = Vector3::Zero;
	if (FAILED(Add_Child(Get_LevelIndex(), L"MonsterSight", L"Em3000Sight", &sightDesc)))
		return E_FAIL;

	// Hit volume around the skirt for the front stomp and the blade mixer, switched on by Em3000Body notifies.
	DAMAGE_INFO bodyAttack{};
	bodyAttack.attacker = shared_from_this();
	bodyAttack.attackType = ATK_TYPE::HEAVY;
	bodyAttack.damage = 30.f;
	bodyAttack.groggyWeight = 10.f;
	bodyAttack.knockbackForce = 1.f;
	MonsterAOE::MONSTER_AOE_DESC centerDesc{};
	centerDesc.model = m_MainBody->Get_ModelComponent();
	centerDesc.offset = Vector3::Zero;
	centerDesc.radius = 4.25f;
	centerDesc.targetBoneName = "em3000";
	centerDesc.dmgInfo = bodyAttack;
	if (FAILED(Add_Child(Get_LevelIndex(), L"MonsterAOE", L"Em3000CenterAoe", &centerDesc)))
		return E_FAIL;

	// The danmak 1 floor lasers, reused by every laser attack.
	GAMEOBJECT_DESC laserDesc{};
	if (FAILED(Add_Child(Get_LevelIndex(), L"EmLaser", L"EmLaser", &laserDesc)))
		return E_FAIL;

	// The body part's sphere covers the skirt. This one covers the upper body (torso and head, not the outstretched arms),
	// carried by the chest bone5 (5.7 m up at rest). Scene documents list factory children in Add_Child order, so new
	// children go after the existing ones.
	MonsterHitBox::MONSTER_HITBOX_DESC upperDesc{};
	upperDesc.model = m_MainBody->Get_ModelComponent();
	upperDesc.targetBoneName = "bone5";
	upperDesc.radius = 1.7f;
	upperDesc.offset = Vector3{ 0.f, 0.4f, 0.f };
	if (FAILED(Add_Child(Get_LevelIndex(), L"MonsterHitBox", L"Em3000UpperHitBox", &upperDesc)))
		return E_FAIL;

	// A factory child like Em0010's, so a scene save keeps it under the boss instead of writing a loose root each time.
	HpBarWorldUI::HP_BAR_WORLD_UI_DESC hpBarDesc{};
	hpBarDesc.target = static_pointer_cast<Entity>(shared_from_this());
	// Just over her head, as the reference floats it (the chest bone5 is 5.7 m up at rest).
	hpBarDesc.worldOffset = Vector3{ 0.f, 8.5f, 0.f };
	hpBarDesc.displayName = L"보부아르";
	hpBarDesc.anchor = UI_ANCHOR::TOP_LEFT;
	hpBarDesc.sizeX = 236.f;
	hpBarDesc.sizeY = 10.f;
	if (FAILED(Add_Child(Get_LevelIndex(), L"HpBarWorldUI", L"Em3000HpBar", &hpBarDesc)))
		return E_FAIL;
	m_HpBarUI = static_pointer_cast<HpBarWorldUI>(Find_Child(L"Em3000HpBar"));

	if (FAILED(m_MainBody->Begin()))
		return E_FAIL;

	return S_OK;
}

HRESULT Em3000::Ready_Components()
{
	EmMovement::EM_MOVEMENT_DESC movementDesc{};
	movementDesc.velocity = Vector3{ 0.f, 0.f, 0.f };
	movementDesc.moveSpeed = 0.f;
	movementDesc.targetDirection = Vector3::Zero;
	movementDesc.turnSpeed = 7.5f;
	m_EmMovement = Add_Component<EmMovement>(ETOI(LEVEL::GAMEPLAY), &movementDesc);
	if (nullptr == m_EmMovement)
		return E_FAIL;

	// StateMachine은 마지막에 처리
	if (nullptr == (m_States = Add_Component<MonsterStateMachine>(ETOI(LEVEL::GAMEPLAY))))
		return E_FAIL;
	const auto em3000 = static_pointer_cast<Em3000>(shared_from_this());
	const auto tag = [](EM3000_STATE state) { return Helper::To_wString(magic_enum::enum_name(state)); };
	if (FAILED(m_States->Add_State(StateEm3000_Intro::Create(tag(EM3000_STATE::INTRO), em3000))) ||
		FAILED(m_States->Add_State(StateEm3000_Idle::Create(tag(EM3000_STATE::IDLE), em3000))) ||
		FAILED(m_States->Add_State(StateEm3000_Melee::Create(tag(EM3000_STATE::MELEE), em3000))) ||
		FAILED(m_States->Add_State(StateEm3000_Range::Create(tag(EM3000_STATE::RANGE), em3000))) ||
		FAILED(m_States->Add_State(StateEm3000_Groggy::Create(tag(EM3000_STATE::GROGGY), em3000))) ||
		FAILED(m_States->Add_State(StateEm3000_Transform::Create(tag(EM3000_STATE::TRANSFORM), em3000))) ||
		FAILED(m_States->Add_State(StateEm3000_Chase::Create(tag(EM3000_STATE::CHASE), em3000))) ||
		FAILED(m_States->Add_State(StateEm3000_Stomp::Create(tag(EM3000_STATE::STOMP), em3000))) ||
		FAILED(m_States->Add_State(StateEm3000_Phase2Groggy::Create(tag(EM3000_STATE::PHASE2_GROGGY), em3000))) ||
		FAILED(m_States->Add_State(StateEm3000_Puppet::Create(tag(EM3000_STATE::PUPPET), em3000))) ||
		FAILED(m_States->Add_State(StateEm3000_Dead::Create(tag(EM3000_STATE::DEAD), em3000))))
		return E_FAIL;

	if (FAILED(m_EmMovement->Begin()))
	{
		LOG_ERROR(L"Failed to Begin EM3000 Movement");
		return E_FAIL;
	}

	SphereCollider::SPHERE_COLLIDER_DESC physicalZoneDesc{};
	physicalZoneDesc.offset = Vector3::UnitY;
	physicalZoneDesc.radius = 3.f;
	m_PhysicalZone = Add_Component<SphereCollider>(ETOI(LEVEL::STATIC), &physicalZoneDesc);
	if (nullptr == m_PhysicalZone)
		return E_FAIL;

	Change_State(EM3000_STATE::INTRO);

	return S_OK;
}

Shared<Em3000> Em3000::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<Em3000>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Em3000");
		return nullptr;
	}

	return prototype;
}

Shared<GameObject> Em3000::Clone(void* arg)
{
	auto instance = make_shared<Em3000>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : Em3000");
		return nullptr;
	}

	return instance;
}


