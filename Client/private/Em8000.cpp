#include "pch.h"
#include "Em8000.h"

#include <Game.h>
#include <SpdLogger.h>

#include "Em8000Body.h"
#include "EmMovement.h"
#include "SphereCollider.h"
#include "HpBarWorldUI.h"
#include "MonsterAOE.h"
#include "MonsterHitBox.h"
#include "MonsterSight.h"
#include "MonsterStateMachine.h"
#include "Pl0000.h"
#include "StateEm8000_Attack.h"
#include "StateEm8000_Ball.h"
#include "StateEm8000_Dead.h"
#include "StateEm8000_Groggy.h"
#include "StateEm8000_Idle.h"
#include "StateEm8000_Gun.h"
#include "StateEm8000_Intro.h"
#include "StateEm8000_Laser.h"
#include "StateEm8000_Move.h"
#include "StateEm8000_TailStab.h"

namespace
{
	/* A sphere at modelCentre (model space, metres) as MonsterHitBox and MonsterAOE take it: its centre in the bone's own
	   space and its radius there. Their sphere goes through the bone's matrix, which carries the model's import scale,
	   so both are read off the bind pose instead of guessing the bone's axes and units. */
	void To_BoneSpace(const Shared<Model>& model, int32 bone, const Vector3& modelCentre, Float radius,
		Vector3& outOffset, Float& outRadius)
	{
		const Matrix boneMatrix = model->Get_BindBoneMatrix(bone);
		outOffset = Vector3::Transform(modelCentre, boneMatrix.Invert());
		const Float scale = Vector3{ boneMatrix._11, boneMatrix._12, boneMatrix._13 }.Length();
		outRadius = scale > 1e-6f ? radius / scale : radius;
	}

	// The same for a box at modelCentre turned by modelOrientation (model space) with half extents in metres.
	void To_BoneSpace(const Shared<Model>& model, int32 bone, const Vector3& modelCentre, const Quaternion& modelOrientation,
		const Vector3& extents, Vector3& outOffset, Vector3& outRotation, Vector3& outExtents)
	{
		const Matrix boneMatrix = model->Get_BindBoneMatrix(bone);
		Vector3 scale{}, translation{};
		Quaternion boneRotation{};
		Matrix{ boneMatrix }.Decompose(scale, boneRotation, translation);
		outOffset = Vector3::Transform(modelCentre, boneMatrix.Invert());
		Quaternion inverse{};
		boneRotation.Inverse(inverse);
		// The collider turns its box by its own rotation first, then by the bone's.
		outRotation = (modelOrientation * inverse).ToEuler();
		outExtents = scale.x > 1e-6f ? extents / scale.x : extents;
	}

	// The armour plates are 5 m long along the toe and lie on its bone; this sphere takes their hits.
	constexpr Float ArmourRadius = 2.6f;
	// The seq AttackTrack sizes are half extents (Shape 2, a box) or the radius (Shape 0, a sphere): read so, the blades'
	// box (bone304 / bone560, 0600/0605/0610: 2 x 2 x 3 pushed 2 m out along the blade) runs from 1 m behind the bone to
	// the 6 m blade's tip, as the blade meshes do in Blender at 0600 f100.
	constexpr Float BladeReach = 2.f;
	const Vector3 BladeExtents{ 2.f, 2.f, 3.f };
	// 0500 AttackTrack f129-137: the body lands with a 9 x 4 x 5 box on bone3, over the body and the legs' spread.
	const Vector3 PressExtents{ 9.f, 4.f, 5.f };
	// 0520 AttackTrack f115-140: spheres on tail bones 162-174, on the tip (bone175, pushed 2 m on past it) and round the
	// feet (bone1), in Em8000::NumTailSpheres order.
	constexpr pair<const char*, Float> TailSpheres[Em8000::NumTailSpheres] = {
		{ "bone162", 3.f }, { "bone165", 3.f }, { "bone168", 3.f }, { "bone171", 2.5f }, { "bone174", 2.2f },
		{ "bone175", 1.6f }, { "bone1", 7.f } };
	constexpr Float TailTipReach = 2.f;
	// The ball's hits on bone3 (seq PartsNo 3): 0545-0548 and 054A f78-91 a sphere of 5.5, 054A's landing f91-94 one of 7.5.
	constexpr Float BallRadius = 5.5f;
	constexpr Float BallLandRadius = 7.5f;
}

Em8000::Em8000(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: Monster{ device, context } {}
Em8000::Em8000(const Em8000& rhs)
	: Monster{ rhs }, m_ArmourMaxHp{ rhs.m_ArmourMaxHp }, m_GroggyTime{ rhs.m_GroggyTime },
	m_GroggyDamageScale{ rhs.m_GroggyDamageScale }, m_StompRingRadius{ rhs.m_StompRingRadius },
	m_PressAreaRadius{ rhs.m_PressAreaRadius }, m_LaserWidth{ rhs.m_LaserWidth }, m_LaserLength{ rhs.m_LaserLength },
	m_TailTurnSpeed{ rhs.m_TailTurnSpeed }, m_RageHpRatio{ rhs.m_RageHpRatio }, m_BallPressLift{ rhs.m_BallPressLift },
	m_EyeTrackAngle{ rhs.m_EyeTrackAngle }, m_EyeTrackSpeed{ rhs.m_EyeTrackSpeed } {}

HRESULT Em8000::Initialize_Prototype()
{
	return Monster::Initialize_Prototype();
}

HRESULT Em8000::Initialize(void* arg)
{
	if (FAILED(Monster::Initialize(arg)))
	{
		LOG_ERROR(L"Failed to Initialize Em8000");
		return E_FAIL;
	}

	if (FAILED(Ready_PartObjects()))
	{
		LOG_ERROR(L"Failed to Ready_PartObjects Em8000");
		return E_FAIL;
	}

	if (FAILED(Ready_Components()))
	{
		LOG_ERROR(L"Failed to Ready_Components Em8000");
		return E_FAIL;
	}

	m_IsStatic = true;
	m_MaxHp = 10000.f;
	m_Hp = m_MaxHp;
	m_ArmourHp.fill(m_ArmourMaxHp);

	return S_OK;
}

void Em8000::On_Destroy()
{
	// The Editor's Stop and a level change destroy the boss but leave 2D channels playing.
	GAME_INSTANCE->StopSound(BgmChannel);
	Monster::On_Destroy();
}

void Em8000::Start_Bgm()
{
	GAME_INSTANCE->PlaySoundLoopSection(L"BGM_Bt_EmilDespair", BgmChannel, BgmVolume, BgmLoopStartMs, UINT32_MAX, true);
}

void Em8000::Set_ArmourHp(Float armourHp)
{
	m_ArmourMaxHp = armourHp;
	for (Float& hp : m_ArmourHp)
		if (hp > 0.f)
			hp = armourHp;
}

void Em8000::Priority_Update(Float timeDelta)
{
	// A third plate broken knocks it dizzy, whatever it was doing.
	if (!m_GroggyDue || Is_Dead())
		return;
	m_GroggyDue = false;
	Change_State(EM8000_STATE::GROGGY);
}

void Em8000::Update(Float timeDelta)
{
	m_States->Update_State(timeDelta);
	m_Movement->Update_Movement(timeDelta);
	// Get_Position reads the world matrix, so it is refreshed after the move (as Em3000).
	m_Transform->Update_WorldMatrix();
	if (!m_BallForm)
		m_PhysicalZone->Update(*m_Transform->Get_WorldMatrixPtr());
}

void Em8000::Late_Update(Float timeDelta)
{
	if (!m_BallForm)
		return;
	// The ball keeps the player out where it is (it jumps and rolls about its own centre), not round the feet. While it
	// is out to hit, the hit throws the player back instead of the push shoving them aside first. Here, after the body's
	// clip has run its notifies this frame, so the push is off on the frame the hit comes on.
	Matrix world = *m_Transform->Get_WorldMatrixPtr();
	world.Translation(m_MainBody->Get_BodyPosition() - Vector3::TransformNormal(m_PhysicalZone->Get_Offset(), world));
	m_PhysicalZone->Update(world);
	m_PhysicalZone->Set_Active(!m_MainBody->Is_BallAttacking());
}

void Em8000::Fixed_Update(Float fixedDelta)
{
}

HRESULT Em8000::Render()
{
	return S_OK;
}

void Em8000::Submit_RenderGroup()
{
}

const TRANSFORM_FRAME& Em8000::Get_BodyModelTransform() const
{
	return m_MainBody->Get_ModelTransform();
}

uint32 Em8000::Find_HitArmour(const Vector3& hitPosition) const
{
	// The hit point lies on (or inside) the sphere it struck; the plates' spheres overlap along a foot, so the nearest wins.
	uint32 found = NumArmour;
	Float nearest = ArmourRadius + 0.05f;
	for (uint32 i = 0; i < NumArmour; ++i)
	{
		if (Is_ArmourBroken(i))
			continue;
		const Float distance = Vector3::Distance(hitPosition, m_MainBody->Get_ArmourPosition(i));
		if (distance <= nearest)
		{
			nearest = distance;
			found = i;
		}
	}
	return found;
}

void Em8000::TakeDamage(const DAMAGE_INFO& dmgInfo)
{
	if (Is_Dead()) return;

	Play_HitSFX(dmgInfo);
	DisplaySparkEffect(dmgInfo.attackType, dmgInfo.hitPosition, dmgInfo.hitRotation);

	// A hit on a plate wears the plate down instead of the body.
	const uint32 armour = Find_HitArmour(dmgInfo.hitPosition);
	if (armour < NumArmour)
	{
		if (m_TargetPlayer.expired())
			m_TargetPlayer = dmgInfo.attacker;
		m_ArmourHp[armour] -= dmgInfo.damage;
		if (m_ArmourHp[armour] <= 0.f)
			Break_Armour(armour);
		return;
	}

	// The boss has no flinch: hits only take HP, more of it while dizzy.
	DAMAGE_INFO damage = dmgInfo;
	if (Is_State(EM8000_STATE::GROGGY))
		damage.damage *= m_GroggyDamageScale;
	Monster::TakeDamage(damage);
	if (!m_BallForm && !Is_Dead() && m_Hp <= m_MaxHp * m_RageHpRatio)
		m_BallDue = true;
}

void Em8000::Enter_BallForm()
{
	m_BallForm = true;
	m_GroggyDue = false;
	for (uint32 i = 0; i < NumArmour; ++i)
	{
		m_ArmourHp[i] = 0.f;
		if (const Shared<MonsterHitBox> hitBox = m_ArmourHitBoxes[i].lock())
			hitBox->Set_Active(false);
	}
}

void Em8000::Break_Armour(uint32 index)
{
	m_ArmourHp[index] = 0.f;
	if (const Shared<MonsterHitBox> hitBox = m_ArmourHitBoxes[index].lock())
		hitBox->Set_Active(false);
	m_MainBody->Break_Armour(index);
	if (++m_BrokenArmour % ArmourPerGroggy == 0)
		m_GroggyDue = true;
	LOG_INFO(L"Em8000 {}: armour {} broken ({} of {})", m_ObjectName, index, m_BrokenArmour, NumArmour);
}

void Em8000::OnDeath()
{
	// The wreck stays, so its push-out zone stays too and the player still cannot walk into it.
	if (m_HpBarUI)
		m_HpBarUI->Set_Active(false);
	m_MainBody->End_Attacks();
	GAME_INSTANCE->FadeSound(BgmChannel, 0.f, BgmFadeOutTime, true);
	GAME_INSTANCE->FadeSound(Pl0000::LevelBgmChannel, 1.f, Pl0000::LevelBgmFadeInTime);
	Change_State(EM8000_STATE::DEAD);
}

Bool Em8000::Change_State(EM8000_STATE state)
{
	return m_States->Change_StateByTag(Helper::To_wString(magic_enum::enum_name(state)));
}

Bool Em8000::Is_State(EM8000_STATE state) const
{
	const Shared<State> current = m_States ? m_States->Get_CurrentState() : nullptr;
	return current && current->IsSameTag(Helper::To_wString(magic_enum::enum_name(state)));
}

void Em8000::OnCollisionEnter(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
}

void Em8000::OnCollisionStay(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
	Monster::OnCollisionStay(ownCollider, targetCollider);
}

void Em8000::OnCollisionExit(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
}

HRESULT Em8000::Ready_PartObjects()
{
	// Scene documents list factory children in Add_Child order, so new children go after the existing ones.
	PartObject::PARTOBJECT_DESC bodyDesc{};
	if (FAILED(Add_Child(Get_LevelIndex(), L"Em8000Body", L"Em8000Body", &bodyDesc)))
		return E_FAIL;
	m_MainBody = static_pointer_cast<Em8000Body>(Find_Child(L"Em8000Body"));
	const Shared<Model> model = m_MainBody->Get_ModelComponent();
	const auto bonePosition = [&model](const char* bone) {
		return model->Get_BindBoneMatrix(model->Get_BoneIndexByName(bone)).Translation();
	};

	// Finds the player across the arena (EmInfo reaches run to 40 m).
	MonsterSight::MONSTER_SIGHT_DESC sightDesc{};
	sightDesc.radius = 35.f;
	sightDesc.offset = Vector3::Zero;
	if (FAILED(Add_Child(Get_LevelIndex(), L"MonsterSight", L"Em8000Sight", &sightDesc)))
		return E_FAIL;

	// The two blades, switched on in the scissors' and the slam's attack windows by Em8000Body notifies.
	DAMAGE_INFO bladeAttack{};
	bladeAttack.attacker = shared_from_this();
	bladeAttack.attackType = ATK_TYPE::HEAVY;
	bladeAttack.damage = 40.f;
	bladeAttack.groggyWeight = 10.f;
	bladeAttack.knockbackForce = 1.5f;
	const pair<const char*, const char*> blades[2] = { { "bone304", "bone288" }, { "bone560", "bone544" } };
	for (uint32 i = 0; i < 2; ++i)
	{
		Vector3 tip = bonePosition(blades[i].first), axis = tip - bonePosition(blades[i].second);
		axis.Normalize();
		MonsterAOE::MONSTER_AOE_DESC bladeDesc{};
		bladeDesc.model = model;
		bladeDesc.targetBoneName = blades[i].first;
		bladeDesc.dmgInfo = bladeAttack;
		To_BoneSpace(model, model->Get_BoneIndexByName(blades[i].first), tip + axis * BladeReach,
			Quaternion::FromToRotation(Vector3::UnitZ, axis), BladeExtents, bladeDesc.offset, bladeDesc.rotation, bladeDesc.extents);
		if (FAILED(Add_Child(Get_LevelIndex(), L"MonsterAOE", i == 0 ? L"Em8000BladeAoeA" : L"Em8000BladeAoeB", &bladeDesc)))
			return E_FAIL;
	}

	// The jump press's landing around the body.
	DAMAGE_INFO pressAttack = bladeAttack;
	pressAttack.damage = 50.f;
	pressAttack.blowAway = true;
	MonsterAOE::MONSTER_AOE_DESC pressDesc{};
	pressDesc.model = model;
	pressDesc.targetBoneName = "bone3";
	pressDesc.dmgInfo = pressAttack;
	To_BoneSpace(model, model->Get_BoneIndexByName("bone3"), bonePosition("bone3"), Quaternion::Identity, PressExtents,
		pressDesc.offset, pressDesc.rotation, pressDesc.extents);
	if (FAILED(Add_Child(Get_LevelIndex(), L"MonsterAOE", L"Em8000PressAoe", &pressDesc)))
		return E_FAIL;

	// The eye laser, shared with Em3000.
	GAMEOBJECT_DESC laserDesc{};
	if (FAILED(Add_Child(Get_LevelIndex(), L"EmLaser", L"EmLaser", &laserDesc)))
		return E_FAIL;

	// One sphere on each plate of armour; it reaches the boss through the Monster layer like the body, and
	// Find_HitArmour tells which plate a hit landed on.
	for (uint32 i = 0; i < NumArmour; ++i)
	{
		MonsterHitBox::MONSTER_HITBOX_DESC armourDesc{};
		armourDesc.model = model;
		armourDesc.targetBoneName = ArmourBones[i];
		const int32 bone = model->Get_BoneIndexByName(ArmourBones[i]);
		To_BoneSpace(model, bone, bonePosition(ArmourBones[i]), ArmourRadius, armourDesc.offset, armourDesc.radius);
		const wstring tag = L"Em8000Armour" + to_wstring(i);
		if (FAILED(Add_Child(Get_LevelIndex(), L"MonsterHitBox", tag, &armourDesc)))
			return E_FAIL;
		m_ArmourHitBoxes[i] = static_pointer_cast<MonsterHitBox>(Find_Child(tag));
	}

	// A factory child like Em3000's, so a scene save keeps it under the boss.
	HpBarWorldUI::HP_BAR_WORLD_UI_DESC hpBarDesc{};
	hpBarDesc.target = static_pointer_cast<Entity>(shared_from_this());
	hpBarDesc.worldOffset = Vector3{ 0.f, 9.f, 0.f };
	hpBarDesc.displayName = L"묵자";
	hpBarDesc.anchor = UI_ANCHOR::TOP_LEFT;
	hpBarDesc.sizeX = 236.f;
	hpBarDesc.sizeY = 10.f;
	if (FAILED(Add_Child(Get_LevelIndex(), L"HpBarWorldUI", L"Em8000HpBar", &hpBarDesc)))
		return E_FAIL;
	m_HpBarUI = static_pointer_cast<HpBarWorldUI>(Find_Child(L"Em8000HpBar"));

	// The tail swung round (0520), switched on in its attack window by Em8000Body.
	DAMAGE_INFO tailAttack = bladeAttack;
	for (uint32 i = 0; i < NumTailSpheres; ++i)
	{
		const auto& [bone, radius] = TailSpheres[i];
		Vector3 centre = bonePosition(bone);
		if (i == NumTailSpheres - 2)
		{
			Vector3 along = centre - bonePosition("bone174");
			along.Normalize();
			centre += along * TailTipReach;
		}
		MonsterAOE::MONSTER_AOE_DESC tailDesc{};
		tailDesc.model = model;
		tailDesc.targetBoneName = bone;
		tailDesc.dmgInfo = tailAttack;
		To_BoneSpace(model, model->Get_BoneIndexByName(bone), centre, radius, tailDesc.offset, tailDesc.radius);
		if (FAILED(Add_Child(Get_LevelIndex(), L"MonsterAOE", L"Em8000TailAoe" + to_wstring(i), &tailDesc)))
			return E_FAIL;
	}

	// The tail's drill stabbing (0556), its box long along the tail.
	{
		const Vector3 tip = bonePosition("bone175");
		Vector3 along = tip - bonePosition("bone174");
		along.Normalize();
		MonsterAOE::MONSTER_AOE_DESC stabDesc{};
		stabDesc.model = model;
		stabDesc.targetBoneName = "bone175";
		stabDesc.dmgInfo = tailAttack;
		To_BoneSpace(model, model->Get_BoneIndexByName("bone175"), tip - along * StabInset,
			Quaternion::FromToRotation(Vector3::UnitY, along), StabExtents, stabDesc.offset, stabDesc.rotation, stabDesc.extents);
		if (FAILED(Add_Child(Get_LevelIndex(), L"MonsterAOE", L"Em8000StabAoe", &stabDesc)))
			return E_FAIL;
	}

	// The ball rolling, landing and spinning into the player: spheres round the body's centre.
	DAMAGE_INFO ballAttack = pressAttack;
	ballAttack.damage = 40.f;
	for (const auto& [tag, radius] : { pair{ L"Em8000BallAoe", BallRadius }, pair{ L"Em8000BallLandAoe", BallLandRadius } })
	{
		MonsterAOE::MONSTER_AOE_DESC ballDesc{};
		ballDesc.model = model;
		ballDesc.targetBoneName = "bone3";
		ballDesc.dmgInfo = ballAttack;
		To_BoneSpace(model, model->Get_BoneIndexByName("bone3"), bonePosition("bone3"), radius, ballDesc.offset, ballDesc.radius);
		if (FAILED(Add_Child(Get_LevelIndex(), L"MonsterAOE", tag, &ballDesc)))
			return E_FAIL;
	}

	// The tail slammed down ahead (0530): a box on the root, whose turn in the clip it follows. Ahead is the way the eye
	// looks from the body in the bind pose, along the model's Z.
	{
		Vector3 ahead = bonePosition("bone16") - bonePosition("bone3");
		ahead.y = 0.f;
		ahead.Normalize();
		MonsterAOE::MONSTER_AOE_DESC slamDesc{};
		slamDesc.model = model;
		slamDesc.targetBoneName = "em8000";
		slamDesc.dmgInfo = tailAttack;
		To_BoneSpace(model, model->Get_BoneIndexByName("em8000"), ahead * SlamReach + Vector3{ 0.f, SlamHeight, 0.f },
			Quaternion::Identity, SlamExtents, slamDesc.offset, slamDesc.rotation, slamDesc.extents);
		if (FAILED(Add_Child(Get_LevelIndex(), L"MonsterAOE", L"Em8000TailSlamAoe", &slamDesc)))
			return E_FAIL;
	}

	if (FAILED(m_MainBody->Begin()))
		return E_FAIL;

	return S_OK;
}

HRESULT Em8000::Ready_Components()
{
	EmMovement::EM_MOVEMENT_DESC movementDesc{};
	movementDesc.velocity = Vector3{ 0.f, 0.f, 0.f };
	movementDesc.moveSpeed = 0.f;
	movementDesc.targetDirection = Vector3::Zero;
	movementDesc.turnSpeed = 2.5f;		// slower than Em3000 (7.5): the body spans 30 m
	m_Movement = Add_Component<EmMovement>(ETOI(LEVEL::GAMEPLAY), &movementDesc);
	if (nullptr == m_Movement)
		return E_FAIL;

	// StateMachine은 마지막에 처리
	if (nullptr == (m_States = Add_Component<MonsterStateMachine>(ETOI(LEVEL::GAMEPLAY))))
		return E_FAIL;
	const auto em8000 = static_pointer_cast<Em8000>(shared_from_this());
	const auto tag = [](EM8000_STATE state) { return Helper::To_wString(magic_enum::enum_name(state)); };
	if (FAILED(m_States->Add_State(StateEm8000_Idle::Create(tag(EM8000_STATE::IDLE), em8000))) ||
		FAILED(m_States->Add_State(StateEm8000_Move::Create(tag(EM8000_STATE::MOVE), em8000))) ||
		FAILED(m_States->Add_State(StateEm8000_Attack::Create(tag(EM8000_STATE::ATTACK), em8000))) ||
		FAILED(m_States->Add_State(StateEm8000_Laser::Create(tag(EM8000_STATE::LASER), em8000))) ||
		FAILED(m_States->Add_State(StateEm8000_Groggy::Create(tag(EM8000_STATE::GROGGY), em8000))) ||
		FAILED(m_States->Add_State(StateEm8000_Dead::Create(tag(EM8000_STATE::DEAD), em8000))) ||
		FAILED(m_States->Add_State(StateEm8000_Intro::Create(tag(EM8000_STATE::INTRO), em8000))) ||
		FAILED(m_States->Add_State(StateEm8000_TailStab::Create(tag(EM8000_STATE::TAIL_STAB), em8000))) ||
		FAILED(m_States->Add_State(StateEm8000_Gun::Create(tag(EM8000_STATE::GUN), em8000))) ||
		FAILED(m_States->Add_State(StateEm8000_Ball::Create(tag(EM8000_STATE::BALL), em8000))))
		return E_FAIL;

	if (FAILED(m_Movement->Begin()))
	{
		LOG_ERROR(L"Failed to Begin Em8000 Movement");
		return E_FAIL;
	}

	// Keeps the player out of the sphere body; the legs are left open to run under.
	SphereCollider::SPHERE_COLLIDER_DESC physicalZoneDesc{};
	physicalZoneDesc.offset = Vector3{ 0.f, 2.5f, 0.f };
	physicalZoneDesc.radius = 4.f;
	m_PhysicalZone = Add_Component<SphereCollider>(ETOI(LEVEL::STATIC), &physicalZoneDesc);
	if (nullptr == m_PhysicalZone)
		return E_FAIL;

	// It waits hanging on the building beside the arena and jumps in when the player comes (StateEm8000_Intro).
	Change_State(EM8000_STATE::INTRO);

	return S_OK;
}

Shared<Em8000> Em8000::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<Em8000>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Em8000");
		return nullptr;
	}

	return prototype;
}

Shared<GameObject> Em8000::Clone(void* arg)
{
	auto instance = make_shared<Em8000>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : Em8000");
		return nullptr;
	}

	return instance;
}
