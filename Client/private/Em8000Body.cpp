#include "pch.h"
#include "Em8000Body.h"

#include <Game.h>
#include <SpdLogger.h>

#include <Shader.h>
#include <SoundEmitter.h>
#include <SphereCollider.h>

#include "ClientSettingManager.h"
#include "EmLaser.h"
#include "EmRing.h"
#include "Bullet.h"
#include "EmTelegraph.h"
#include "ExplodeEffect.h"
#include "MonsterAOE.h"
#include "Random_Helper.h"
#include "SparkEffect.h"

Em8000Body::Em8000Body() : PartObject{} {}
Em8000Body::Em8000Body(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: PartObject{ device, context } {}
Em8000Body::Em8000Body(const Em8000Body& rhs)
	: PartObject{ rhs } {}

const TRANSFORM_FRAME& Em8000Body::Get_ModelTransform() const
{
	return m_Model->Get_RootTransformVelocity(m_RootBoneIndex);
}

void Em8000Body::Set_Animation(uint32 animIndex, Float blendDuration, Bool isLoop)
{
	m_Model->Set_Animation(animIndex, blendDuration);
	m_Model->Set_AnimLoop(isLoop);
}

uint32 Em8000Body::Get_CurrentAnimationIndex() const
{
	return m_Model->Get_AnimationIndex();
}

Bool Em8000Body::Is_AnimationFinished() const
{
	return m_Model->Is_AnimationFinished();
}

HRESULT Em8000Body::Initialize_Prototype()
{
	m_LayerMask.Set_Layer(L"Monster");
	m_TagMask.Set_Tag({ L"Monster" });

	return PartObject::Initialize_Prototype();
}

HRESULT Em8000Body::Initialize(void* arg)
{
	if (FAILED(PartObject::Initialize(arg)))
		return E_FAIL;

	if (FAILED(Ready_Components()))
	{
		LOG_ERROR(L"Failed To Ready_Components : Em8000Body");
		return E_FAIL;
	}

	if (FAILED(Ready_AnimationNotify()))
	{
		LOG_ERROR(L"Failed To Ready_AnimationNotify : Em8000Body");
		return E_FAIL;
	}

	m_RootBoneIndex = m_Model->Get_BoneIndexByName("em8000");
	// bone3 carries the sphere body (its height moves in the jumps), bone16 the round lens on its front.
	m_BodyBoneIndex = m_Model->Get_BoneIndexByName("bone3");
	m_EyeBoneIndex = m_Model->Get_BoneIndexByName("bone16");
	m_GunBoneIndex = m_Model->Get_BoneIndexByName("bone32");
	m_MuzzleBoneIndex = m_Model->Get_BoneIndexByName("bone33");
	m_BladeBoneIndices = { m_Model->Get_BoneIndexByName("bone304"), m_Model->Get_BoneIndexByName("bone560") };
	m_TailBoneIndex = m_Model->Get_BoneIndexByName("bone168");
	m_TailTipBoneIndex = m_Model->Get_BoneIndexByName("bone175");
	if (m_RootBoneIndex == -1 || m_BodyBoneIndex == -1 || m_EyeBoneIndex == -1 || m_GunBoneIndex == -1 ||
		m_MuzzleBoneIndex == -1 || m_BladeBoneIndices[0] == -1 || m_BladeBoneIndices[1] == -1 || m_TailBoneIndex == -1 ||
		m_TailTipBoneIndex == -1)
	{
		LOG_ERROR(L"Failed to Find Em8000 Root/Body/Eye Bone");
		return E_FAIL;
	}
	for (uint32 i = 0; i < Em8000::NumArmour; ++i)
	{
		m_ArmourBoneIndices[i] = m_Model->Get_BoneIndexByName(Em8000::ArmourBones[i]);
		if (m_ArmourBoneIndices[i] == -1)
		{
			LOG_ERROR(L"Failed to Find Em8000 Armour Bone {}", i);
			return E_FAIL;
		}
	}
	m_Model->Set_LocalRootNode(m_RootBoneIndex);
	m_HiddenMeshes.assign(m_Model->Get_NumMeshes(), false);
	// The round lens faces out of the sphere body: from bone3 through bone16 in the bind pose, kept in bone16's space.
	const Matrix bindEye = m_Model->Get_BindBoneMatrix(m_EyeBoneIndex);
	m_EyeFacing = Vector3::TransformNormal(bindEye.Translation() - m_Model->Get_BindBoneMatrix(m_BodyBoneIndex).Translation(),
		bindEye.Invert());
	m_EyeFacing.Normalize();

	return S_OK;
}

HRESULT Em8000Body::Begin()
{
	const Shared<GameObject> owner = Get_Owner();
	if (!owner)
		return E_FAIL;
	m_BladeAttacks[0] = static_pointer_cast<MonsterAOE>(owner->Find_Child(L"Em8000BladeAoeA"));
	m_BladeAttacks[1] = static_pointer_cast<MonsterAOE>(owner->Find_Child(L"Em8000BladeAoeB"));
	m_PressAttack = static_pointer_cast<MonsterAOE>(owner->Find_Child(L"Em8000PressAoe"));
	for (uint32 i = 0; i < Em8000::NumTailSpheres; ++i)
		m_TailAttacks[i] = static_pointer_cast<MonsterAOE>(owner->Find_Child(L"Em8000TailAoe" + to_wstring(i)));
	m_StabAttack = static_pointer_cast<MonsterAOE>(owner->Find_Child(L"Em8000StabAoe"));
	m_BallAttack = static_pointer_cast<MonsterAOE>(owner->Find_Child(L"Em8000BallAoe"));
	m_BallLandAttack = static_pointer_cast<MonsterAOE>(owner->Find_Child(L"Em8000BallLandAoe"));
	m_SlamAttack = static_pointer_cast<MonsterAOE>(owner->Find_Child(L"Em8000TailSlamAoe"));
	m_Laser = static_pointer_cast<EmLaser>(owner->Find_Child(L"EmLaser"));
	// The hit volumes start off and only switch on in their clips' attack windows.
	for (const Weak<MonsterAOE>& attack : m_BladeAttacks)
		Set_Attack(attack, false);
	Set_Attack(m_PressAttack, false);
	for (const Weak<MonsterAOE>& attack : m_TailAttacks)
		Set_Attack(attack, false);
	Set_Attack(m_StabAttack, false);
	Set_Attack(m_BallAttack, false);
	Set_Attack(m_BallLandAttack, false);
	Set_Attack(m_SlamAttack, false);
	return S_OK;
}

Bool Em8000Body::Is_BallAttacking() const
{
	const auto attacking = [](const Weak<MonsterAOE>& attack) {
		const Shared<MonsterAOE> aoe = attack.lock();
		return aoe && aoe->Is_Attacking();
	};
	return attacking(m_BallAttack) || attacking(m_BallLandAttack);
}

void Em8000Body::Set_Lift(Float height)
{
	m_Transform->Set_LocalPosition(0.f, height, 0.f);
}

Vector3 Em8000Body::Get_BonePosition(int32 boneIndex) const
{
	return (m_Model->Get_BoneMatrix(boneIndex) * m_CombinedWorldMatrix).Translation();
}

Vector3 Em8000Body::Get_ArmourPosition(uint32 index) const
{
	return index < Em8000::NumArmour ? Get_BonePosition(m_ArmourBoneIndices[index]) : Vector3::Zero;
}

void Em8000Body::Break_Armour(uint32 index)
{
	if (index >= Em8000::NumArmour) return;

	Burst_Plate(index);
	Hide_Plate(index);
}

void Em8000Body::Hide_Armour()
{
	for (uint32 i = 0; i < Em8000::NumArmour; ++i)
		Hide_Plate(i);
}

void Em8000Body::Hide_Plate(uint32 index)
{
	m_PlateHidden[index] = true;
	const string plate = ArmourMeshes[index];
	for (uint32 i = 0; i < m_Model->Get_NumMeshes(); ++i)
	{
		string meshName{};
		uint32 materialIndex{};
		if (SUCCEEDED(m_Model->Get_MeshMaterialInfo(i, meshName, materialIndex)) &&
			(meshName == plate || meshName == plate + ".001"))
			m_HiddenMeshes[i] = true;
	}
}

void Em8000Body::Burst_Plate(uint32 index) const
{
	if (m_PlateHidden[index]) return;

	ExplodeEffect::EXPLODE_EFFECT_DESC blast{};
	blast.position = Get_ArmourPosition(index);
	blast.size = BreakBlastSize;
	GAME_INSTANCE->Instantiate<ExplodeEffect>(L"ExplodeEffect", Get_LevelIndex(), &blast);
	GAME_INSTANCE->PlaySound3D(L"em8000_armour_removable", m_SoundEmitter->Get_Volume(), blast.position,
		m_SoundEmitter->Get_NearDistance(), m_SoundEmitter->Get_FarDistance(), false);
}

Vector3 Em8000Body::Get_LaserDirection() const
{
	Vector3 out = Get_BonePosition(m_EyeBoneIndex) - Get_BonePosition(m_BodyBoneIndex);
	out.y = 0.f;
	out.Normalize();
	out.y = -m_LaserDrop;
	out.Normalize();
	return out;
}

void Em8000Body::Begin_Laser()
{
	const Shared<Em8000> owner = Cast<Em8000>(Get_Owner());
	const Shared<EmLaser> laser = m_Laser.lock();
	if (!owner || !laser) return;

	// Tipped down to land on the floor as far out as the player stands; the body's turn in 0701 then sweeps it round.
	const Vector3 eye = Get_BonePosition(m_EyeBoneIndex);
	const Float floorY = owner->Get_Transform()->Get_Position().y;
	Float reach = LaserMaxReach;
	if (const Shared<GameObject> target = owner->Get_Target())
	{
		Vector3 toTarget = target->Get_Transform()->Get_Position() - eye;
		toTarget.y = 0.f;
		reach = std::clamp(toTarget.Length(), LaserMinReach, LaserMaxReach);
	}
	m_LaserDrop = (std::max)(eye.y - floorY, 0.f) / reach;
	laser->Fire_Aimed(eye, Get_LaserDirection(), floorY, owner->Get_LaserWidth(), owner->Get_LaserLength());
}

void Em8000Body::End_Laser()
{
	if (const Shared<EmLaser> laser = m_Laser.lock())
		laser->Stop();
}

void Em8000Body::Telegraph()
{
	EmTelegraph::Play(static_pointer_cast<PartObject>(shared_from_this()), m_Model, TellSize, L"Em_Tele_L");
	m_TellAge = 0.f;
}

void Em8000Body::Track_Eye(Float timeDelta)
{
	const Shared<Em8000> owner = Cast<Em8000>(Get_Owner());
	if (!owner) return;

	// The clip's eye this frame (model space) and where it looks.
	const Matrix eye = m_Model->Get_BoneMatrix(m_EyeBoneIndex);
	Vector3 facing = Vector3::TransformNormal(m_EyeFacing, eye);
	facing.Normalize();

	// The turn that brings that facing round onto the player (in model space, as the bone matrices are), no more than
	// EyeTrackAngle; none when it should look ahead with the pose.
	Quaternion goal{};
	const Shared<GameObject> target = owner->Get_Target();
	const Shared<EmLaser> laser = m_Laser.lock();
	if (target && !owner->Is_Dead() && !(laser && laser->Is_Running()))
	{
		const Vector3 chest = target->Get_Transform()->Get_Position() + Vector3{ 0.f, EyeTargetHeight, 0.f };
		Vector3 look = Vector3::TransformNormal(chest - (eye * m_CombinedWorldMatrix).Translation(), m_CombinedWorldMatrix.Invert());
		look.Normalize();
		const Float angle = acosf(std::clamp(facing.Dot(look), -1.f, 1.f));
		const Float limit = XMConvertToRadians(owner->Get_EyeTrackAngle());
		if (angle <= limit)
			goal = Quaternion::FromToRotation(facing, look);
		else
		{
			Vector3 axis = facing.Cross(look);
			if (axis.LengthSquared() > 1e-8f)
			{
				axis.Normalize();
				goal = Quaternion::CreateFromAxisAngle(axis, limit);
			}
		}
	}

	// Toward the goal at EyeTrackSpeed.
	const Float apart = 2.f * acosf(std::clamp(fabsf(m_EyeTurn.Dot(goal)), 0.f, 1.f));
	const Float step = XMConvertToRadians(owner->Get_EyeTrackSpeed()) * timeDelta;
	m_EyeTurn = apart <= step ? goal : Quaternion::Slerp(m_EyeTurn, goal, step / apart);
	m_EyeTurn.Normalize();

	// Turned about its own place: out to the origin, round, and back.
	const Vector3 pivot = eye.Translation();
	const Matrix turned = eye * Matrix::CreateTranslation(-pivot) * Matrix::CreateFromQuaternion(m_EyeTurn) * Matrix::CreateTranslation(pivot);
	m_Model->Override_BoneMatrices({ static_cast<uint32>(m_EyeBoneIndex) }, { turned });
}

void Em8000Body::Spread_Ring(const Vector3& center, Float endRadius, Bool filled) const
{
	const Shared<Monster> owner = Cast<Monster>(Get_Owner());
	if (!owner) return;

	EmRing::EM_RING_DESC desc{};
	desc.mode = filled ? EmRing::MODE::AREA : EmRing::MODE::GROUND;
	desc.target = owner->Get_Target();
	desc.start = Vector3{ center.x, owner->Get_Transform()->Get_Position().y, center.z };
	desc.spreadSpeed = filled ? AreaSpreadSpeed : RingSpreadSpeed;
	desc.endRadius = endRadius;
	desc.tint = RingTint;
	desc.damage = RingDamage;
	desc.bandWidth = RingBandWidth;
	desc.glowWidth = RingGlowWidth;
	desc.haze = RingHaze;
	desc.glitch = filled ? AreaGlitch : RingGlitch;
	desc.glow = true;
	if (filled)
	{
		desc.flame = AreaFlame;
		desc.flash = AreaFlash;
	}
	GAME_INSTANCE->Instantiate<EmRing>(L"EmRing", Get_LevelIndex(), &desc);
}

void Em8000Body::Throw_Sparks(int32 boneIndex, Float impact, Bool onFloor, Float ahead) const
{
	const Shared<GameObject> owner = Get_Owner();
	if (!owner) return;

	SparkEffect::SPARK_EFFECT_DESC spark{};
	Vector3 look = owner->Get_Transform()->Get_Look();
	look.y = 0.f;
	look.Normalize();
	spark.position = Get_BonePosition(boneIndex) + look * ahead;
	if (onFloor)
		spark.position.y = owner->Get_Transform()->Get_Position().y + SparkLift;
	spark.impact = impact;
	GAME_INSTANCE->Instantiate<SparkEffect>(L"SparkEffect", Get_LevelIndex(), &spark);
}

void Em8000Body::Update_Crackle(Float timeDelta)
{
	const Shared<Monster> owner = Cast<Monster>(Get_Owner());
	if (!owner || owner->Is_Dead()) return;
	m_CrackleWait -= timeDelta;
	m_CrackleSnapWait -= timeDelta;
	if (m_CrackleWait > 0.f) return;
	m_CrackleWait = static_cast<Float>(Helper::Random_Double(CrackleMinWait, CrackleMaxWait));

	const int32 bone = Helper::Random_Int(0, static_cast<int32>(m_Model->Get_Num_Bones()) - 1);
	SparkEffect::SPARK_EFFECT_DESC spark{};
	spark.position = Get_BonePosition(bone) + Vector3{ static_cast<Float>(Helper::Random_Double(-CrackleSpread, CrackleSpread)),
		static_cast<Float>(Helper::Random_Double(-CrackleSpread, CrackleSpread)), static_cast<Float>(Helper::Random_Double(-CrackleSpread, CrackleSpread)) };
	spark.electric = static_cast<Float>(Helper::Random_Double(CrackleMinSize, CrackleMaxSize));
	if (m_CrackleSnapWait <= 0.f)
	{
		m_CrackleSnapWait = static_cast<Float>(Helper::Random_Double(CrackleSnapMinWait, CrackleSnapMaxWait));
		spark.refract = true;
	}
	GAME_INSTANCE->Instantiate<SparkEffect>(L"SparkEffect", Get_LevelIndex(), &spark);
}

void Em8000Body::Set_Attack(Weak<MonsterAOE> attack, Bool active) const
{
	const Shared<MonsterAOE> aoe = attack.lock();
	if (!aoe) return;
	if (active)
		aoe->Active_Attack();
	else
		aoe->DeActive_Attack();
}

void Em8000Body::Play_LoopSound(const wstring& soundTag)
{
	Stop_LoopSound();
	m_LoopSound = GAME_INSTANCE->PlaySound3D(soundTag, m_SoundEmitter->Get_Volume(), m_CombinedWorldMatrix.Translation(),
		m_SoundEmitter->Get_NearDistance(), m_SoundEmitter->Get_FarDistance(), true);
}

void Em8000Body::Stop_LoopSound()
{
	if (m_LoopSound == 0) return;
	GAME_INSTANCE->StopLoopSound3D(m_LoopSound);
	m_LoopSound = 0;
}

void Em8000Body::Start_Hum()
{
	if (m_HumSound != 0) return;
	m_HumSound = GAME_INSTANCE->PlaySound3D(L"em8000-0325-event", m_SoundEmitter->Get_Volume(), m_CombinedWorldMatrix.Translation(),
		m_SoundEmitter->Get_NearDistance(), m_SoundEmitter->Get_FarDistance(), true);
}

void Em8000Body::Power_Down()
{
	if (m_HumSound == 0) return;
	GAME_INSTANCE->StopLoopSound3D(m_HumSound);
	m_HumSound = 0;
	Play_Sound(L"em8000-0283-event");
}

void Em8000Body::Play_Sound(const wstring& soundTag) const
{
	m_SoundEmitter->Play(soundTag);
}

void Em8000Body::Fire_Gun(uint32 pellets, Float spread)
{
	const Shared<GameObject> owner = Get_Owner();
	if (!owner) return;

	const Vector3 breech = Get_BonePosition(m_GunBoneIndex);
	Vector3 barrel = Get_BonePosition(m_MuzzleBoneIndex) - breech;
	barrel.Normalize();
	const Vector3 muzzle = Get_BonePosition(m_MuzzleBoneIndex) + barrel * MuzzleReach;
	barrel.y = 0.f;
	barrel.Normalize();
	Bullet::BULLET_DESC desc{};
	desc.initialPosition = muzzle;
	desc.direction = barrel;
	desc.damageInfo.attacker = owner;
	desc.damageInfo.attackType = ATK_TYPE::LIGHT;
	desc.damageInfo.damage = GunShotDamage;
	desc.resourceTag = L"candy";
	desc.targetLayer = L"Player";
	desc.speed = GunShotSpeed;
	desc.maxDistance = GunShotRange;
	Bullet::Fire_Spread(desc, pellets, XMConvertToRadians(spread), 0.f, Get_LevelIndex());
	GAME_INSTANCE->PlaySound3D(L"em8000_default_shot", m_SoundEmitter->Get_Volume(), muzzle,
		m_SoundEmitter->Get_NearDistance(), m_SoundEmitter->Get_FarDistance(), false);
}

void Em8000Body::Fire_Cone(uint32 count, Bool permanent)
{
	const Shared<GameObject> owner = Get_Owner();
	if (!owner) return;

	// The cone's axis: level out from the sphere's centre through the lens, as the laser, so it follows the spin.
	const Vector3 eye = Get_BonePosition(m_EyeBoneIndex);
	Vector3 look = eye - Get_BodyPosition();
	look.y = 0.f;
	if (look.LengthSquared() < 1e-6f)
		look = owner->Get_Transform()->Get_Look();
	look.Normalize();
	Vector3 side = Vector3::UnitY.Cross(look);
	side.Normalize();
	const Vector3 up = look.Cross(side);

	Bullet::BULLET_DESC desc{};
	desc.initialPosition = eye;
	desc.damageInfo.attacker = owner;
	desc.damageInfo.attackType = ATK_TYPE::LIGHT;
	desc.damageInfo.damage = GunShotDamage;
	desc.resourceTag = L"candy";
	desc.targetLayer = L"Player";
	desc.isPermanent = permanent;
	desc.speed = ConeShotSpeed;
	desc.maxDistance = ConeShotRange;
	// Even over the cone's cap: cos of the angle off the axis uniform, round the axis uniform.
	const Double minCos = cos(XMConvertToRadians(ConeShotAngle));
	for (uint32 i = 0; i < count; ++i)
	{
		const Float cosOff = static_cast<Float>(Helper::Random_Double(minCos, 1.0));
		const Float sinOff = sqrtf((std::max)(1.f - cosOff * cosOff, 0.f));
		const Float around = static_cast<Float>(Helper::Random_Double(0.0, XM_2PI));
		desc.direction = look * cosOff + (side * cosf(around) + up * sinf(around)) * sinOff;
		GAME_INSTANCE->Instantiate<Bullet>(L"Bullet", Get_LevelIndex(), &desc);
	}
	GAME_INSTANCE->PlaySound3D(L"em8000_default_shot", m_SoundEmitter->Get_Volume(), eye,
		m_SoundEmitter->Get_NearDistance(), m_SoundEmitter->Get_FarDistance(), false);
}

void Em8000Body::End_Attacks()
{
	for (const Weak<MonsterAOE>& attack : m_BladeAttacks)
		Set_Attack(attack, false);
	Set_Attack(m_PressAttack, false);
	for (const Weak<MonsterAOE>& attack : m_TailAttacks)
		Set_Attack(attack, false);
	Set_Attack(m_StabAttack, false);
	Set_Attack(m_BallAttack, false);
	Set_Attack(m_BallLandAttack, false);
	Set_Attack(m_SlamAttack, false);
	End_Laser();
	Stop_LoopSound();
}

void Em8000Body::Burst(Float size, Bool atBody)
{
	const int32 bone = atBody ? m_BodyBoneIndex : Helper::Random_Int(0, static_cast<int32>(m_Model->Get_Num_Bones()) - 1);
	ExplodeEffect::EXPLODE_EFFECT_DESC blast{};
	blast.position = Get_BonePosition(bone);
	blast.size = size;
	GAME_INSTANCE->Instantiate<ExplodeEffect>(L"ExplodeEffect", Get_LevelIndex(), &blast);
	GAME_INSTANCE->PlaySound3D(L"em_common_death_bom_m", m_SoundEmitter->Get_Volume(), blast.position,
		m_SoundEmitter->Get_NearDistance(), m_SoundEmitter->Get_FarDistance(), false);
}

void Em8000Body::On_Destroy()
{
	Stop_LoopSound();
	if (m_HumSound != 0)
		GAME_INSTANCE->StopLoopSound3D(m_HumSound);
	m_HumSound = 0;
	PartObject::On_Destroy();
}

void Em8000Body::Priority_Update(Float timeDelta)
{
}

void Em8000Body::Update(Float timeDelta)
{
	m_Model->Update_ModelAnimation(timeDelta);
	EmTelegraph::Advance_Tell(m_TellAge, timeDelta);
}

void Em8000Body::Late_Update(Float timeDelta)
{
	m_Transform->Update_WorldMatrix();
	Update_CombineWorldMatrix(*m_Transform->Get_WorldMatrixPtr());
	// The held sounds go where the boss goes (the ball rolls 30 m and more under its rolling loop).
	GAME_INSTANCE->MoveLoopSound3D(m_LoopSound, m_CombinedWorldMatrix.Translation());
	GAME_INSTANCE->MoveLoopSound3D(m_HumSound, m_CombinedWorldMatrix.Translation());
	Track_Eye(timeDelta);
	// After this frame's pose and place, so the crackle sits on the bone where it is drawn.
	Update_Crackle(timeDelta);
	// The body's sphere rides bone3 up and down through the jumps.
	m_HitBox->Update(Matrix::CreateTranslation(Get_BonePosition(m_BodyBoneIndex)));
	if (const Shared<EmLaser> laser = m_Laser.lock(); laser && laser->Is_Running())
		laser->Set_Aim(Get_BonePosition(m_EyeBoneIndex), Get_LaserDirection());
}

void Em8000Body::Fixed_Update(Float fixedDelta)
{
}

HRESULT Em8000Body::Render()
{
	if (FAILED(Bind_ShaderResources()))
		return E_FAIL;
	// The front eye (the disc inside the head's rim) glows red the whole fight, as in the reference.
	if (GAME_INSTANCE->Get_DrawingGroup() == RENDERGROUP::EMISSIVE)
		return EmTelegraph::Render_Eyes(m_Model, m_Shader, m_TellAge);

	const uint32 numMeshes = m_Model->Get_NumMeshes();
	for (uint32 i = 0; i < numMeshes; ++i)
	{
		if (m_HiddenMeshes[i])
			continue;

		if (FAILED(m_Model->Bind_BoneMatrices(m_Shader, BoneMatrices, i))) return E_FAIL;

		if (FAILED(m_Model->BindAndBeginMaterial(m_Shader, i)))
			return E_FAIL;

		if (FAILED(m_Model->Render(i))) return E_FAIL;
	}

	return S_OK;
}

void Em8000Body::Submit_RenderGroup()
{
	// Also runs in edit mode, where Late_Update does not: keep the mesh on the owner's transform.
	m_Transform->Update_WorldMatrix();
	Update_CombineWorldMatrix(*m_Transform->Get_WorldMatrixPtr());
	GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::NONBLEND, shared_from_this());
	if (const Shared<Monster> owner = Cast<Monster>(Get_Owner()); owner && !owner->Is_Dead())
		GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::EMISSIVE, shared_from_this());
}

void Em8000Body::OnCollisionEnter(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
}

void Em8000Body::OnCollisionStay(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
}

void Em8000Body::OnCollisionExit(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
}

HRESULT Em8000Body::Bind_ShaderResources()
{
	if (FAILED(m_Shader->Bind_Matrix(WorldMatrix, &m_CombinedWorldMatrix)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ViewMatrix, D3DTS::VIEW)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ProjMatrix, D3DTS::PROJ)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Bind_CameraPosition(m_Shader, CameraPosition)))
		return E_FAIL;

	return S_OK;
}

HRESULT Em8000Body::Ready_Components()
{
	Shader::SHADER_DESC shaderDesc{ VTXANIMMESH::Tag, VTXANIMMESH::Elements, VTXANIMMESH::numElements };
	m_Shader = Add_Component<Shader>(ETOI(LEVEL::STATIC), &shaderDesc);
	if (nullptr == m_Shader)
		return E_FAIL;

	Model::MODEL_DESC modelDesc{ L"em8000" };
	m_Model = Add_Component<Model>(ETOI(LEVEL::STATIC), &modelDesc);
	if (nullptr == m_Model)
		return E_FAIL;
	if (FAILED(m_Model->Validate_MaterialBindings(m_Shader))) return E_FAIL;
	if (m_Model->Get_AnimationPreset().Is_Empty())
	{
		AnimationPresetSnapshot preset;
		if (FAILED(ClientSettingManager::GetInstance()->Load_AnimationPreset(L"em8000", preset)) ||
			FAILED(m_Model->Apply_AnimationPreset(preset)))
			return E_FAIL;
	}

	SoundEmitter::SOUND_EMITTER_DESC emitterDesc{};
	emitterDesc.volume = 0.3f;
	emitterDesc.nearDistance = 10.f;
	emitterDesc.farDistance = 80.f;
	m_SoundEmitter = Add_Component<SoundEmitter>(ETOI(LEVEL::STATIC), &emitterDesc);
	if (nullptr == m_SoundEmitter)
		return E_FAIL;

	SphereCollider::SPHERE_COLLIDER_DESC sphereDesc{};
	sphereDesc.radius = BodyRadius;
	sphereDesc.offset = Vector3::Zero;
	m_HitBox = Add_Component<SphereCollider>(ETOI(LEVEL::STATIC), &sphereDesc);
	if (nullptr == m_HitBox)
		return E_FAIL;

	return S_OK;
}

HRESULT Em8000Body::Ready_AnimationNotify()
{
	using anim = Em8000::EM8000_ANIM;
	using notify = AnimationTracker::ANIMATION_NOTIFY;

	// 원작 SeTrack 사운드 이벤트는 몸의 SoundEmitter로 낸다(카메라와의 거리만큼 줄어든다).
	auto playSound = [this](const tChar* tag) { m_SoundEmitter->Play(tag); };
	const auto bladesOn = [this]() { for (const Weak<MonsterAOE>& attack : m_BladeAttacks) Set_Attack(attack, true); };
	const auto bladesOff = [this]() { for (const Weak<MonsterAOE>& attack : m_BladeAttacks) Set_Attack(attack, false); };
	const auto footRing = [this](uint32 armour) { return [this, armour]() {
		if (const Shared<Em8000> owner = Cast<Em8000>(Get_Owner()))
			Spread_Ring(Get_ArmourPosition(armour), owner->Get_StompRingRadius());
	}; };
	// Metal flies where a blow lands, on the seq's impact sound frames.
	const auto footSparks = [this](uint32 armour) { return [this, armour]() { Throw_Sparks(m_ArmourBoneIndices[armour], FootImpact); }; };
	const auto bladeSparks = [this]() { for (const int32 bone : m_BladeBoneIndices) Throw_Sparks(bone, FootImpact, true, HitSparkAhead); };
	const auto bodySparks = [this]() { Throw_Sparks(m_BodyBoneIndex, BodyImpact); };

	// Frames are 60 per second. The hover loops' hum is held by StateEm8000_Move (Play_LoopSound) over the whole step.
	for (const auto& [start, end] : { std::pair{ anim::MOVE_F_START, anim::MOVE_F_END }, std::pair{ anim::MOVE_B_START, anim::MOVE_B_END },
		std::pair{ anim::MOVE_L_START, anim::MOVE_L_END }, std::pair{ anim::MOVE_R_START, anim::MOVE_R_END } })
	{
		m_Model->Add_AnimNotify(ETOI(start), {
			notify{ L"em8000_hover_start", 0.f, [playSound]() { playSound(L"em8000_hover_start"); } },
		});
		m_Model->Add_AnimNotify(ETOI(end), {
			notify{ L"em8000_hover_loop_end", 0.f, [playSound]() { playSound(L"em8000_hover_loop_end"); } },
		});
	}
	m_Model->Add_AnimNotify(ETOI(anim::DEAD), {
		notify{ L"em8000_death", 2.f, [playSound]() { playSound(L"em8000_death"); } },
	});
	// 0541 seq: the ball dying unfolds back onto its legs before it collapses (StateEm8000_Dead).
	m_Model->Add_AnimNotify(ETOI(anim::BALL_OUT), {
		notify{ L"em8000_robo_tranceform", 1.f, [playSound]() { playSound(L"em8000_robo_tranceform"); } },
	});
	m_Model->Add_AnimNotify(ETOI(anim::GROGGY_IN), {
		notify{ L"em8000_dizzy_start", 1.f, [playSound]() { playSound(L"em8000_dizzy_start"); } },
		notify{ L"em8000_tail_down", 26.f, [playSound]() { playSound(L"em8000_tail_down"); } },
	});
	m_Model->Add_AnimNotify(ETOI(anim::GROGGY_END), {
		notify{ L"em8000_dizzy_end", 0.f, [playSound]() { playSound(L"em8000_dizzy_end"); } },
		notify{ L"em8000_tail_down_recover", 10.f, [playSound]() { playSound(L"em8000_tail_down_recover"); } },
	});
	// 2050/2051 seq: stirring on the wall, then the jump, the fall and the landing, which sends the jump press's
	// shockwave over the floor (2051 has no attack track of its own; 0515, the other way down, lands with ground boxes).
	m_Model->Add_AnimNotify(ETOI(anim::INTRO_HANG), {
		notify{ L"em8000_factory_appear_wait_move", 310.f, [playSound]() { playSound(L"em8000_factory_appear_wait_move"); } },
	});
	m_Model->Add_AnimNotify(ETOI(anim::INTRO_JUMP), {
		notify{ L"em8000_body_move_01_ev", 0.f, [playSound]() { playSound(L"em8000_body_move_01_ev"); } },
		notify{ L"em8000_factory_appear_jump_wsh", 18.f, [playSound]() { playSound(L"em8000_factory_appear_jump_wsh"); } },
		notify{ L"em8000_factory_appear_fall_wsh", 66.f, [playSound]() { playSound(L"em8000_factory_appear_fall_wsh"); } },
		notify{ L"em8000_factory_appear_land", 96.f, [playSound]() { playSound(L"em8000_factory_appear_land"); } },
		notify{ L"em8000_body_move_03_ev", 204.f, [playSound]() { playSound(L"em8000_body_move_03_ev"); } },
		notify{ L"em8000_step_move_ev", 205.f, [playSound]() { playSound(L"em8000_step_move_ev"); } },
		notify{ L"Intro_Sparks", 96.f, bodySparks },
		notify{ L"Intro_Shockwave", 96.f, [this]() {
			if (const Shared<Em8000> owner = Cast<Em8000>(Get_Owner()))
				Spread_Ring(Get_BonePosition(m_BodyBoneIndex), owner->Get_PressAreaRadius(), true);
		} },
	});
	// 0500 seq AttackTrack: the body's box lands at 129-137, and at 135-137 ground boxes of half extent 13 turned four ways
	// round it hurt anyone standing on the floor there (the shockwave).
	m_Model->Add_AnimNotify(ETOI(anim::JUMP_PRESS), {
		notify{ L"em8000_blade_dash_atk_start", 0.f, [playSound]() { playSound(L"em8000_blade_dash_atk_start"); } },
		notify{ L"em8000_jump_small", 73.f, [playSound]() { playSound(L"em8000_jump_small"); } },
		notify{ L"em8000_jump_press", 130.f, [playSound]() { playSound(L"em8000_jump_press"); } },
		notify{ L"em8000_body_move_03", 234.f, [playSound]() { playSound(L"em8000_body_move_03"); } },
		notify{ L"em8000_step_move", 235.f, [playSound]() { playSound(L"em8000_step_move"); } },
		notify{ L"Press_Impact", 129.f, 137.f, [this]() { Set_Attack(m_PressAttack, true); }, [this]() { Set_Attack(m_PressAttack, false); } },
		notify{ L"Press_Sparks", 130.f, bodySparks },
		notify{ L"Press_Shockwave", 135.f, [this]() {
			if (const Shared<Em8000> owner = Cast<Em8000>(Get_Owner()))
				Spread_Ring(owner->Get_Transform()->Get_Position(), owner->Get_PressAreaRadius(), true);
		} },
	});
	// 0560 seq: six stomps, each toe in turn; a FlagsTrack flag 8 frames after each impact (FreeArg = the toe) sends its
	// ring over the floor.
	m_Model->Add_AnimNotify(ETOI(anim::STOMP_A), {
		notify{ L"em8000_foot_stomp_start", 0.f, [playSound]() { playSound(L"em8000_foot_stomp_start"); } },
		notify{ L"em8000_foot_stomp_wsh", 145.f, [playSound]() { playSound(L"em8000_foot_stomp_wsh"); } },
		notify{ L"em8000_foot_stomp_impact", 148.f, [playSound]() { playSound(L"em8000_foot_stomp_impact"); } },
		notify{ L"em8000_foot_stomp_wsh", 185.f, [playSound]() { playSound(L"em8000_foot_stomp_wsh"); } },
		notify{ L"em8000_foot_stomp_impact", 188.f, [playSound]() { playSound(L"em8000_foot_stomp_impact"); } },
		notify{ L"em8000_foot_stomp_wsh", 218.f, [playSound]() { playSound(L"em8000_foot_stomp_wsh"); } },
		notify{ L"em8000_foot_stomp_impact", 221.f, [playSound]() { playSound(L"em8000_foot_stomp_impact"); } },
		notify{ L"em8000_foot_stomp_wsh", 283.f, [playSound]() { playSound(L"em8000_foot_stomp_wsh"); } },
		notify{ L"em8000_foot_stomp_impact", 286.f, [playSound]() { playSound(L"em8000_foot_stomp_impact"); } },
		notify{ L"em8000_foot_stomp_wsh", 317.f, [playSound]() { playSound(L"em8000_foot_stomp_wsh"); } },
		notify{ L"em8000_foot_stomp_impact", 320.f, [playSound]() { playSound(L"em8000_foot_stomp_impact"); } },
		notify{ L"em8000_foot_stomp_wsh", 360.f, [playSound]() { playSound(L"em8000_foot_stomp_wsh"); } },
		notify{ L"em8000_foot_stomp_impact", 363.f, [playSound]() { playSound(L"em8000_foot_stomp_impact"); } },
		notify{ L"em8000_foot_stomp_end", 411.f, [playSound]() { playSound(L"em8000_foot_stomp_end"); } },
		notify{ L"Stomp_Ring1", 156.f, footRing(0) },
		notify{ L"Stomp_Ring4", 196.f, footRing(3) },
		notify{ L"Stomp_Ring2", 229.f, footRing(1) },
		notify{ L"Stomp_Ring5", 294.f, footRing(4) },
		notify{ L"Stomp_Ring3", 328.f, footRing(2) },
		notify{ L"Stomp_Ring6", 371.f, footRing(5) },
		notify{ L"Stomp_Sparks1", 148.f, footSparks(0) },
		notify{ L"Stomp_Sparks4", 188.f, footSparks(3) },
		notify{ L"Stomp_Sparks2", 221.f, footSparks(1) },
		notify{ L"Stomp_Sparks5", 286.f, footSparks(4) },
		notify{ L"Stomp_Sparks3", 320.f, footSparks(2) },
		notify{ L"Stomp_Sparks6", 363.f, footSparks(5) },
	});
	m_Model->Add_AnimNotify(ETOI(anim::STOMP_B), {
		notify{ L"em8000_foot_stomp_start", 0.f, [playSound]() { playSound(L"em8000_foot_stomp_start"); } },
		notify{ L"em8000_foot_stomp_wsh", 145.f, [playSound]() { playSound(L"em8000_foot_stomp_wsh"); } },
		notify{ L"em8000_foot_stomp_impact", 148.f, [playSound]() { playSound(L"em8000_foot_stomp_impact"); } },
		notify{ L"em8000_foot_stomp_wsh", 221.f, [playSound]() { playSound(L"em8000_foot_stomp_wsh"); } },
		notify{ L"em8000_foot_stomp_impact", 224.f, [playSound]() { playSound(L"em8000_foot_stomp_impact"); } },
		notify{ L"em8000_foot_stomp_wsh", 327.f, [playSound]() { playSound(L"em8000_foot_stomp_wsh"); } },
		notify{ L"em8000_foot_stomp_impact", 330.f, [playSound]() { playSound(L"em8000_foot_stomp_impact"); } },
		notify{ L"em8000_foot_stomp_wsh", 378.f, [playSound]() { playSound(L"em8000_foot_stomp_wsh"); } },
		notify{ L"em8000_foot_stomp_impact", 381.f, [playSound]() { playSound(L"em8000_foot_stomp_impact"); } },
		notify{ L"em8000_foot_stomp_wsh", 484.f, [playSound]() { playSound(L"em8000_foot_stomp_wsh"); } },
		notify{ L"em8000_foot_stomp_impact", 487.f, [playSound]() { playSound(L"em8000_foot_stomp_impact"); } },
		notify{ L"em8000_foot_stomp_wsh", 555.f, [playSound]() { playSound(L"em8000_foot_stomp_wsh"); } },
		notify{ L"em8000_foot_stomp_impact", 558.f, [playSound]() { playSound(L"em8000_foot_stomp_impact"); } },
		notify{ L"em8000_foot_stomp_end", 606.f, [playSound]() { playSound(L"em8000_foot_stomp_end"); } },
		notify{ L"Stomp_Ring4", 156.f, footRing(3) },
		notify{ L"Stomp_Ring3", 232.f, footRing(2) },
		notify{ L"Stomp_Ring1", 338.f, footRing(0) },
		notify{ L"Stomp_Ring6", 389.f, footRing(5) },
		notify{ L"Stomp_Ring2", 495.f, footRing(1) },
		notify{ L"Stomp_Ring5", 566.f, footRing(4) },
		notify{ L"Stomp_Sparks4", 148.f, footSparks(3) },
		notify{ L"Stomp_Sparks3", 224.f, footSparks(2) },
		notify{ L"Stomp_Sparks1", 330.f, footSparks(0) },
		notify{ L"Stomp_Sparks6", 381.f, footSparks(5) },
		notify{ L"Stomp_Sparks2", 487.f, footSparks(1) },
		notify{ L"Stomp_Sparks5", 558.f, footSparks(4) },
	});
	// 0600/0605/0610 seq AttackTrack: the two blades (bone304, bone560) hit at 98-102, 112-118 and 98-103.
	m_Model->Add_AnimNotify(ETOI(anim::SCISSORS), {
		notify{ L"em8000_blade_slice_start", 2.f, [playSound]() { playSound(L"em8000_blade_slice_start"); } },
		notify{ L"em8000_blade_slice_wsh", 71.f, [playSound]() { playSound(L"em8000_blade_slice_wsh"); } },
		notify{ L"em8000_step", 85.f, [playSound]() { playSound(L"em8000_step"); } },
		notify{ L"em8000_blade_slice_impact", 86.f, [playSound]() { playSound(L"em8000_blade_slice_impact"); } },
		notify{ L"Blade_Sparks", 86.f, bladeSparks },
		notify{ L"em8000_blade_slice_end", 158.f, [playSound]() { playSound(L"em8000_blade_slice_end"); } },
		notify{ L"Blade_Hit", 98.f, 102.f, bladesOn, bladesOff },
	});
	m_Model->Add_AnimNotify(ETOI(anim::JUMP_SCISSORS), {
		notify{ L"em8000_blade_slice_start", 2.f, [playSound]() { playSound(L"em8000_blade_slice_start"); } },
		notify{ L"em8000_jump_small", 72.f, [playSound]() { playSound(L"em8000_jump_small"); } },
		notify{ L"em8000_blade_slice_wsh", 90.f, [playSound]() { playSound(L"em8000_blade_slice_wsh"); } },
		notify{ L"em8000_blade_slice_impact", 103.f, [playSound]() { playSound(L"em8000_blade_slice_impact"); } },
		notify{ L"em8000_jump_land", 103.f, [playSound]() { playSound(L"em8000_jump_land"); } },
		notify{ L"Blade_Sparks", 103.f, bladeSparks },
		notify{ L"em8000_blade_slice_end", 174.f, [playSound]() { playSound(L"em8000_blade_slice_end"); } },
		notify{ L"Blade_Hit", 112.f, 118.f, bladesOn, bladesOff },
	});
	m_Model->Add_AnimNotify(ETOI(anim::BLADE_SLAM), {
		notify{ L"em8000_blade_hammer_start", 3.f, [playSound]() { playSound(L"em8000_blade_hammer_start"); } },
		notify{ L"em8000_step", 83.f, [playSound]() { playSound(L"em8000_step"); } },
		notify{ L"em8000_blade_hammer_wsh", 88.f, [playSound]() { playSound(L"em8000_blade_hammer_wsh"); } },
		notify{ L"em8000_blade_hammer_impact", 96.f, [playSound]() { playSound(L"em8000_blade_hammer_impact"); } },
		notify{ L"Blade_Sparks", 96.f, bladeSparks },
		notify{ L"em8000_blade_hammer_end", 160.f, [playSound]() { playSound(L"em8000_blade_hammer_end"); } },
		notify{ L"Blade_Hit", 98.f, 103.f, bladesOn, bladesOff },
	});
	// 0520 seq: the tail comes out, swings round with its spheres on at 115-140, and goes back in.
	const auto tailOn = [this]() { for (const Weak<MonsterAOE>& attack : m_TailAttacks) Set_Attack(attack, true); };
	const auto tailOff = [this]() { for (const Weak<MonsterAOE>& attack : m_TailAttacks) Set_Attack(attack, false); };
	m_Model->Add_AnimNotify(ETOI(anim::TAIL_SWEEP), {
		notify{ L"em8000_tail_put_out", 0.f, [playSound]() { playSound(L"em8000_tail_put_out"); } },
		notify{ L"em8000_tail_swing_start", 0.f, [playSound]() { playSound(L"em8000_tail_swing_start"); } },
		notify{ L"em8000_tail_swing_wsh", 97.f, [playSound]() { playSound(L"em8000_tail_swing_wsh"); } },
		notify{ L"em8000_tail_swing_end", 208.f, [playSound]() { playSound(L"em8000_tail_swing_end"); } },
		notify{ L"em8000_tail_put_in", 250.f, [playSound]() { playSound(L"em8000_tail_put_in"); } },
		notify{ L"Tail_Hit", 115.f, 140.f, tailOn, tailOff },
	});
	// 0530 seq: the tail comes out and over the body; its spheres down the tail (not the feet's) are on at 65-75 as it
	// comes down, and the box where it lands ahead at 72-80.
	const auto tailSlamOn = [this]() { for (uint32 i = 0; i + 1 < Em8000::NumTailSpheres; ++i) Set_Attack(m_TailAttacks[i], true); };
	m_Model->Add_AnimNotify(ETOI(anim::TAIL_SLAM), {
		notify{ L"em8000_tail_put_out", 0.f, [playSound]() { playSound(L"em8000_tail_put_out"); } },
		notify{ L"em8000_tail_stomp_start", 0.f, [playSound]() { playSound(L"em8000_tail_stomp_start"); } },
		notify{ L"em8000_tail_stomp_wsh", 58.f, [playSound]() { playSound(L"em8000_tail_stomp_wsh"); } },
		notify{ L"em8000_tail_stomp_impact", 69.f, [playSound]() { playSound(L"em8000_tail_stomp_impact"); } },
		notify{ L"em8000_tail_stomp_end", 129.f, [playSound]() { playSound(L"em8000_tail_stomp_end"); } },
		notify{ L"em8000_tail_put_in", 278.f, [playSound]() { playSound(L"em8000_tail_put_in"); } },
		notify{ L"Slam_TailHit", 65.f, 75.f, tailSlamOn, tailOff },
		notify{ L"Slam_Sparks", 69.f, [this]() { Throw_Sparks(m_TailBoneIndex, TailImpact, true, HitSparkAhead);
			Throw_Sparks(m_TailTipBoneIndex, TailImpact, true, HitSparkAhead); } },
		notify{ L"Slam_Hit", 72.f, 80.f, [this]() { Set_Attack(m_SlamAttack, true); }, [this]() { Set_Attack(m_SlamAttack, false); } },
	});
	// 0555/055a/0556/0557 seq: the tail's drill turned (or stepped) round onto the player, its stab out and back with the
	// box on bone175 at 10-17 every lap, and the turn back.
	m_Model->Add_AnimNotify(ETOI(anim::TAIL_STAB_START), {
		notify{ L"em8000_tail_stinger_start", 0.f, [playSound]() { playSound(L"em8000_tail_stinger_start"); } },
	});
	m_Model->Add_AnimNotify(ETOI(anim::TAIL_STAB_BACK), {
		notify{ L"em8000_body_move_01", 0.f, [playSound]() { playSound(L"em8000_body_move_01"); } },
		notify{ L"em8000_step_move", 35.f, [playSound]() { playSound(L"em8000_step_move"); } },
		notify{ L"em8000_step", 41.f, [playSound]() { playSound(L"em8000_step"); } },
		notify{ L"em8000_step", 51.f, [playSound]() { playSound(L"em8000_step"); } },
		notify{ L"em8000_step", 61.f, [playSound]() { playSound(L"em8000_step"); } },
	});
	m_Model->Add_AnimNotify(ETOI(anim::TAIL_STAB_LOOP), {
		notify{ L"em8000_tail_stinger_impact", 4.f, [playSound]() { playSound(L"em8000_tail_stinger_impact"); } },
		notify{ L"Stab_Sparks", 4.f, [this]() { Throw_Sparks(m_TailTipBoneIndex, StabImpact, false, HitSparkAhead); } },
		notify{ L"Stab_Hit", 10.f, 17.f, [this]() { Set_Attack(m_StabAttack, true); },
			[this]() { Set_Attack(m_StabAttack, false); } },
	});
	m_Model->Add_AnimNotify(ETOI(anim::TAIL_STAB_END), {
		notify{ L"em8000_tail_stinger_end", 0.f, [playSound]() { playSound(L"em8000_tail_stinger_end"); } },
	});
	// 0540 seq: the plates are thrown off one by one at 73-75 (their toes' armour sounds); StateEm8000_Ball hides them
	// once the clip ends.
	const auto shed = [this](uint32 armour) { return [this, armour]() { Burst_Plate(armour); }; };
	m_Model->Add_AnimNotify(ETOI(anim::BALL_IN), {
		notify{ L"em8000_step_move", 0.f, [playSound]() { playSound(L"em8000_step_move"); } },
		notify{ L"em8000_ball_tranceform", 1.f, [playSound]() { playSound(L"em8000_ball_tranceform"); } },
		notify{ L"Shed_Armour3", 73.f, shed(2) },
		notify{ L"Shed_Armour4", 73.f, shed(3) },
		notify{ L"Shed_Armour2", 74.f, shed(1) },
		notify{ L"Shed_Armour5", 74.f, shed(4) },
		notify{ L"Shed_Armour1", 75.f, shed(0) },
		notify{ L"Shed_Armour6", 75.f, shed(5) },
	});
	// 0545-0548 seq AttackTrack: the ball's sphere is on from 0545 f44 through the roll to 0548 f33, off for the
	// U-turn's first 73 frames (0547) and on again for the way back. The rolling hum is held by StateEm8000_Ball.
	const auto ballOn = [this]() { Set_Attack(m_BallAttack, true); };
	const auto ballOff = [this]() { Set_Attack(m_BallAttack, false); };
	m_Model->Add_AnimNotify(ETOI(anim::BALL_DASH_START), {
		notify{ L"em8000_ball_rolling_start", 0.f, [playSound]() { playSound(L"em8000_ball_rolling_start"); } },
		notify{ L"em8000_ball_rolling_loop_start", 70.f, [playSound]() { playSound(L"em8000_ball_rolling_loop_start"); } },
		notify{ L"Ball_HitOn", 44.f, ballOn },
	});
	m_Model->Add_AnimNotify(ETOI(anim::BALL_DASH_TURN), {
		notify{ L"em8000_ball_rolling_turn", 0.f, [playSound]() { playSound(L"em8000_ball_rolling_turn"); } },
		notify{ L"em8000_ball_rolling_loop_start", 70.f, [playSound]() { playSound(L"em8000_ball_rolling_loop_start"); } },
		notify{ L"Ball_HitOff", 0.f, ballOff },
		notify{ L"Ball_HitOn", 73.f, ballOn },
	});
	m_Model->Add_AnimNotify(ETOI(anim::BALL_DASH_END), {
		notify{ L"em8000_ball_rolling_end", 0.f, [playSound]() { playSound(L"em8000_ball_rolling_end"); } },
		notify{ L"Ball_HitOff", 33.f, ballOff },
	});
	// 054A seq: the jump at 24 and the landing at 85, the body's sphere on at 78-91 and the wider one at 91-94. The
	// landing sends a ring of fire over the floor (reference video late 0:00-0:02), like the stomps' rings.
	m_Model->Add_AnimNotify(ETOI(anim::BALL_PRESS), {
		notify{ L"Ball_PressRing", 91.f, [this]() {
			if (const Shared<Em8000> owner = Cast<Em8000>(Get_Owner()))
				Spread_Ring(owner->Get_Transform()->Get_Position(), owner->Get_StompRingRadius());
		} },
		notify{ L"em8000_ball_jump", 24.f, [playSound]() { playSound(L"em8000_ball_jump"); } },
		notify{ L"em8000_ball_jump_impact", 85.f, [playSound]() { playSound(L"em8000_ball_jump_impact"); } },
		// The sound comes ahead of the ball, which the extra lift keeps up until the clip's landing at 91.
		notify{ L"Ball_PressSparks", 91.f, bodySparks },
		notify{ L"Ball_Hit", 78.f, 91.f, ballOn, ballOff },
		notify{ L"Ball_LandHit", 91.f, 94.f, [this]() { Set_Attack(m_BallLandAttack, true); },
			[this]() { Set_Attack(m_BallLandAttack, false); } },
	});
	// 054B-054D: no attack track; the spin's flag goes up at 054B f85 and stays up through 054C, so the body's sphere
	// hurts from then until 054D (once a lap of 054C).
	m_Model->Add_AnimNotify(ETOI(anim::BALL_SPIN_START), {
		notify{ L"em8000_ball_side_spin_start", 1.f, [playSound]() { playSound(L"em8000_ball_side_spin_start"); } },
		notify{ L"Spin_HitOn", 85.f, ballOn },
	});
	m_Model->Add_AnimNotify(ETOI(anim::BALL_SPIN_LOOP), {
		notify{ L"Spin_HitOn", 0.f, ballOn },
	});
	m_Model->Add_AnimNotify(ETOI(anim::BALL_SPIN_END), {
		notify{ L"em8000_ball_side_spin_end", 0.f, [playSound]() { playSound(L"em8000_ball_side_spin_end"); } },
		notify{ L"Spin_HitOff", 0.f, ballOff },
	});
	// 0703-0705 / 0720-0722 seq: the gun's wind-up and wind-down (the shots are timed by StateEm8000_Gun).
	m_Model->Add_AnimNotify(ETOI(anim::SPIN_FIRE_START), {
		notify{ L"em8000_blade_dash_atk_start", 12.f, [playSound]() { playSound(L"em8000_blade_dash_atk_start"); } },
		notify{ L"em8000_hover_turn_start", 71.f, [playSound]() { playSound(L"em8000_hover_turn_start"); } },
	});
	m_Model->Add_AnimNotify(ETOI(anim::SPIN_FIRE_END), {
		notify{ L"em8000_hover_loop_end", 0.f, [playSound]() { playSound(L"em8000_hover_loop_end"); } },
		notify{ L"em8000_foot_stomp_end", 56.f, [playSound]() { playSound(L"em8000_foot_stomp_end"); } },
	});
	m_Model->Add_AnimNotify(ETOI(anim::SHOTGUN_START), {
		notify{ L"em8000_blade_dash_atk_start", 12.f, [playSound]() { playSound(L"em8000_blade_dash_atk_start"); } },
	});
	m_Model->Add_AnimNotify(ETOI(anim::SHOTGUN_END), {
		notify{ L"em8000_step_move", 31.f, [playSound]() { playSound(L"em8000_step_move"); } },
		notify{ L"em8000_body_move_02", 71.f, [playSound]() { playSound(L"em8000_body_move_02"); } },
	});
	// 0700 seq: the laser flag goes up at 80. The beam charges for 0.8 s (48 frames) and so fires as 0701's full turn
	// begins; 0700's own last 22 degrees of turn, which 0701 starts over from, never show in the beam.
	m_Model->Add_AnimNotify(ETOI(anim::LASER_START), {
		notify{ L"em8000_blade_dash_atk_start", 12.f, [playSound]() { playSound(L"em8000_blade_dash_atk_start"); } },
		notify{ L"em8000_hover_turn_start", 71.f, [playSound]() { playSound(L"em8000_hover_turn_start"); } },
		notify{ L"Laser_Begin", 77.f, [this]() { Begin_Laser(); } },
	});
	m_Model->Add_AnimNotify(ETOI(anim::LASER_END), {
		notify{ L"em8000_hover_loop_end", 0.f, [playSound]() { playSound(L"em8000_hover_loop_end"); } },
		notify{ L"em8000_foot_stomp_end", 56.f, [playSound]() { playSound(L"em8000_foot_stomp_end"); } },
	});
	return S_OK;
}

Shared<Em8000Body> Em8000Body::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<Em8000Body>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Em8000Body");
		return nullptr;
	}

	return prototype;
}

Shared<GameObject> Em8000Body::Clone(void* arg)
{
	auto instance = make_shared<Em8000Body>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : Em8000Body");
		return nullptr;
	}

	return instance;
}
