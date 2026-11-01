#include "pch.h"
#include "WP0070Body.h"

#include <SpdLogger.h>

#include "Game.h"
#include "Model.h"
#include "SoftBody.h"
#include "OBBCollider.h"
#include "SphereCollider.h"
#include "Entity.h"
#include "Monster.h"
#include "Pl0000.h"
#include "Pl0000Shockwave.h"

namespace
{
	// The thrown sword: bone0's -Z runs down the blade to its tip (the attack OBB spans z 0.45 to -1.55).
	const Vector3 BladeTip{ 0.f, 0.f, -1.55f };
	constexpr Float ThrowSpeed = 50.f;
	constexpr Float ReturnSpeed = 40.f;
	constexpr Float SinkDepth = 0.7f;
	// Thrown at a lock-on target's feet unless that is flatter than this (the y of the unit direction); then 45 degrees down.
	constexpr Float MinThrowDrop = -0.2f;
}

WP0070Body::WP0070Body() : Pl0000Parts{} {}
WP0070Body::WP0070Body(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: Pl0000Parts{ device, context } {
}
WP0070Body::WP0070Body(const WP0070Body& rhs)
	: Pl0000Parts{ rhs } {
}

HRESULT WP0070Body::Initialize_Prototype()
{
	m_LayerMask.Set_Layer(L"PlayerWeapon");
	m_TagMask.Set_Tag({ L"PlayerWeapon" });

	return Pl0000Parts::Initialize_Prototype();
}

HRESULT WP0070Body::Initialize(void* arg)
{
	if (FAILED(Pl0000Parts::Initialize(arg)))
		return E_FAIL;

	if (FAILED(Ready_Components()))
	{
		LOG_ERROR(L"Failed To Ready_Components : WP0070Body");
		return E_FAIL;
	}

	if (FAILED(Ready_AnimationNotify()))
	{
		LOG_ERROR(L"Failed To Ready Animation Notify : WP0070Body");
		return E_FAIL;
	}

	m_RootBoneIndex = m_Model->Get_BoneIndexByName("wp0070");
	m_WeaponBoneIndex = m_Model->Get_BoneIndexByName("bone0");
	if (m_RootBoneIndex == -1)
	{
		LOG_ERROR(L"Failed to Find WP0070 Root Bone");
		return E_FAIL;
	}
	
	m_Model->Set_LocalRootNode(m_RootBoneIndex);
	return S_OK;
}

void WP0070Body::On_Enable()
{
	Pl0000Parts::On_Enable();
	// A newly drawn sword must not reuse the inactive instance's old cloth pose.
	if (m_SoftBody) m_SoftBody->Set_Simulation(m_SoftBody->Get_Simulation());
	// GameObject enables every component when the sword is drawn. The launch
	// collider belongs only to the sixth hit, so drawing must leave it off.
	if (m_LaunchCollider) m_LaunchCollider->Set_Active(false);
}

void WP0070Body::On_Destroy()
{
	PartObject::On_Destroy();
}

void WP0070Body::Priority_Update(Float timeDelta)
{
	
}

void WP0070Body::Update(Float timeDelta)
{
	m_Model->Update_ModelAnimation(timeDelta);
}

void WP0070Body::Late_Update(Float timeDelta)
{
	m_PrevAttackPivot = m_AttackCollider->Get_Pivot();
	if (m_ThrowPhase != THROW_PHASE::ATTACHED)
	{
		const auto player = static_pointer_cast<Pl0000>(Get_Parent());
		if (!player) return;
		const Matrix boneMatrix = m_Model->Get_BoneMatrix(m_WeaponBoneIndex);
		const Vector3 hilt = m_ThrownBlade.Translation();
		if (m_ThrowPhase == THROW_PHASE::FLYING)
		{
			// Swept from the hilt, so ground already between it and the tip counts on the first frame.
			const Float bladeLength = Vector3::TransformNormal(BladeTip, m_ThrownBlade).Length();
			const Float step = ThrowSpeed * timeDelta;
			Vector3 hit{};
			Vector3 normal{};
			if (GAME_INSTANCE->Raycast_Physics(hilt, m_ThrowDirection, bladeLength + step, hit, &normal)
				&& normal.y > 0.5f)
			{
				// The tip sinks into the ground and the sword stands there along its flight.
				m_ThrownBlade.Translation(hit + m_ThrowDirection * (SinkDepth - bladeLength));
				Impact_Shockwave(hit);
				GAME_INSTANCE->StopSound(SOUNDCHANNEL::CHANNEL_10);
				GAME_INSTANCE->PlaySoundFXOnce(L"Wp0070_Impact7", SOUNDCHANNEL::CHANNEL_10, 0.3f);
				m_ThrowPhase = THROW_PHASE::STUCK;
			}
			else
			{
				m_ThrownBlade.Translation(hilt + m_ThrowDirection * step);
				m_ThrowElapsed += timeDelta;
				if (m_ThrowElapsed >= 3.f) m_ThrowPhase = THROW_PHASE::RETURNING;
			}
		}
		else if (m_ThrowPhase == THROW_PHASE::STUCK)
		{
			// It stands in the ground until the seventh hit has played out, then flies home.
			if (!player->Is_LightGround7Playing()) m_ThrowPhase = THROW_PHASE::RETURNING;
		}
		else
		{
			m_Transform->Update_WorldMatrix();
			const Vector3 hand = (boneMatrix * *m_Transform->Get_WorldMatrixPtr()).Translation();
			Vector3 toHand = hand - hilt;
			const Float distance = toHand.Length();
			if (distance <= ReturnSpeed * timeDelta || distance < 0.1f)
			{
				m_ThrowPhase = THROW_PHASE::ATTACHED;
				m_SoftBody->Set_Simulation(m_SoftBody->Get_Simulation());
				if (m_ReturnToSheath) player->Sheathe_LightWeapon();
			}
			else
				m_ThrownBlade.Translation(hilt + toHand / distance * ReturnSpeed * timeDelta);
		}

		if (m_ThrowPhase != THROW_PHASE::ATTACHED)
		{
			// Drawn as boneMatrix * world, so the world undoes the clip's pose to leave the blade where it was thrown.
			m_CombinedWorldMatrix = boneMatrix.Invert() * m_ThrownBlade;
			m_AttackCollider->Update(m_ThrownBlade);
			m_SoftBody->Simulate(m_Model, m_CombinedWorldMatrix, timeDelta);
			return;
		}
	}

	m_Transform->Update_WorldMatrix();
	Update_CombineWorldMatrix(*m_Transform->Get_WorldMatrixPtr());
	Matrix boneMatrix = m_Model->Get_BoneMatrix(m_WeaponBoneIndex);
	m_AttackCollider->Update(boneMatrix * m_CombinedWorldMatrix);
	m_SoftBody->Simulate(m_Model, m_CombinedWorldMatrix, timeDelta);
	if (m_LaunchCollider)
	{
		const auto player = static_pointer_cast<Pl0000>(Get_Parent());
		if (player) m_LaunchCollider->Update(player->Get_Transform()->Get_WorldMatrix());
	}
}

void WP0070Body::Fixed_Update(Float fixedDelta)
{

}

HRESULT WP0070Body::Render()
{
	if (m_IsActive == false || !m_Visible) return S_OK;

	if (FAILED(Bind_ShaderResources()))
		return E_FAIL;

	size_t numMeshes = m_Model->Get_NumMeshes();
	for (uint32 i = 0; i < numMeshes; ++i)
	{
		if (FAILED(m_Model->Bind_BoneMatrices(m_Shader, BoneMatrices, i))) return E_FAIL;

		if (FAILED(m_Model->BindAndBeginMaterial(m_Shader, i)))
			return E_FAIL;

		if (FAILED(m_Model->Render(i))) return E_FAIL;
	}

	return S_OK;
}

void WP0070Body::Submit_RenderGroup()
{
	if (m_IsActive == false) return;
	GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::NONBLEND, shared_from_this());
}

void WP0070Body::OnCollisionEnter(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
	// The launch collider can cross a monster within one collision frame.
	if (ownCollider == m_LaunchCollider) OnCollisionStay(ownCollider, targetCollider);
}

void WP0070Body::OnCollisionStay(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
	if (ownCollider != m_AttackCollider && ownCollider != m_LaunchCollider) return;
	if (!ownCollider->Is_Active()) return;
	if (ownCollider == m_LaunchCollider && !Is_SphereSwing(m_Model->Get_AnimationIndex())) return;

	auto target = targetCollider->Get_Owner();
	if (target->Get_GameObjectType() != GAMEOBJECTTYPE::PART) return;
	if (target->Get_LayerMask().Get_LayerName() != L"Monster") return;

	auto monster = static_pointer_cast<Monster>(static_pointer_cast<PartObject>(target)->Get_Owner());
	const RuntimeObjectId targetID = monster->Get_RuntimeObjectId();
	if (m_HitEntities.contains(targetID) == false)
	{
		m_HitEntities.insert(targetID);

		Vector3 boneScale{};
		Quaternion boneQuat{};
		Vector3 boneTranslation{};
		m_Model->Get_BoneMatrix(m_WeaponBoneIndex).Decompose(boneScale, boneQuat, boneTranslation);

		Entity::DAMAGE_INFO dmgInfo{};
		dmgInfo.attacker = Get_Owner();
		dmgInfo.damage = 100.f;
		dmgInfo.groggyWeight = 100;
		dmgInfo.attackType = ATK_TYPE::LIGHT;
		dmgInfo.hitPosition = targetCollider->ClosestPoint(ownCollider->Get_Pivot());
		// The blade's travel since last frame, so the sparks fly along the swing.
		const Vector3 swing = m_AttackCollider->Get_Pivot() - m_PrevAttackPivot;
		dmgInfo.hitRotation = swing.LengthSquared() > 0.000001f
			? Quaternion::FromToRotation(Vector3::UnitY, swing / swing.Length())
			: m_AttackCollider->Get_CurrentOrientation();
		dmgInfo.knockbackForce = 1.5f;

		monster->TakeDamage(dmgInfo);
		if (m_Model->Get_AnimationIndex() == ETOI(WP0070_STATE::LIGHT_GROUND6))
		{
			// The machine hangs 1.5 m below her, under the sword through the air combo and its closing plunge.
			constexpr Float launchHeight = 10.f;
			constexpr Float monsterLaunchHeight = 8.5f;
			monster->Launch_Up(monsterLaunchHeight);
			if (!m_PlayerLaunched)
			{
				m_PlayerLaunched = true;
				static_pointer_cast<Pl0000>(Get_Owner())->Launch_FromLightGround6(launchHeight);
			}
		}
		monster->OnAttackHit(shared_from_this());

		auto attacker = static_pointer_cast<Pl0000>(dmgInfo.attacker.lock());
		attacker->OnAttackHit(monster);
		attacker->Trigger_GlobalLag(0.2f, 0.05f);
	}
}

void WP0070Body::OnCollisionExit(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
	
}

void WP0070Body::Set_Sheathing()
{
	if (Is_Thrown())
	{
		m_ReturnToSheath = true;
		return;
	}

	if (m_IsSheathing) return;

	m_AttackCollider->Set_Active(false);
	End_LaunchAttack();
	m_IsSheathing = true;
	m_SnapNextAnimation = true;
	m_IsActive = false;
}

void WP0070Body::DrawWP0070()
{
	if (m_IsSheathing == false) return;

	m_Transform->Set_WorldMatrix(Matrix::Identity);
	m_IsSheathing = false;
}

void WP0070Body::Set_Animation(uint32 animIndex, Float blendDuration, Bool isLoop)
{
	if (Is_Thrown()) Recall_ThrownWeapon();

	if (m_SnapNextAnimation)
	{
		blendDuration = 0.f;
		m_SnapNextAnimation = false;
	}

	m_Transform->Set_WorldMatrix(Matrix::Identity);
	Pl0000Parts::Set_Animation(animIndex, blendDuration, isLoop);
	// Model::Set_Animation ignores the clip already playing; a new swing of that clip starts over to stay in step with the body.
	if (m_Model->Get_AnimationIndex() == animIndex && !m_Model->Is_Blending())
		m_Model->Set_AnimationProgress(0.f);
	m_Model->Update_ModelAnimation(0.001f);
	if (animIndex == ETOI(WP0070_STATE::LIGHT_GROUND7)) Begin_Throw();
}

void WP0070Body::Begin_LaunchAttack()
{
	m_HitEntities.clear();
	m_PlayerLaunched = false;
	m_LaunchCollider->Set_Active(true);
}

void WP0070Body::End_LaunchAttack()
{
	if (m_LaunchCollider) m_LaunchCollider->Set_Active(false);
}

Bool WP0070Body::Is_SphereSwing(uint32 animIndex) const
{
	return animIndex == ETOI(WP0070_STATE::LIGHT_GROUND6) || animIndex == ETOI(WP0070_STATE::LIGHT_AIR1) ||
		animIndex == ETOI(WP0070_STATE::LIGHT_AIR2) || animIndex == ETOI(WP0070_STATE::LIGHT_AIR3);
}

void WP0070Body::Begin_Throw()
{
	End_LaunchAttack();
	m_AttackCollider->Set_Active(false);
	auto player = static_pointer_cast<Pl0000>(Get_Parent());
	if (!player) return;
	m_Transform->Update_WorldMatrix();
	Update_CombineWorldMatrix(*m_Transform->Get_WorldMatrixPtr());
	Vector3 forward = player->Get_Transform()->Get_Look();
	forward.y = 0.f;
	if (forward.LengthSquared() < 0.0001f) forward = Vector3::UnitZ;
	forward.Normalize();
	m_ThrowDirection = (forward - Vector3::UnitY);
	m_ThrowDirection.Normalize();

	// The sword leaves the hand as it is held now, turned about its hilt so the tip leads the flight.
	m_ThrownBlade = m_Model->Get_BoneMatrix(m_WeaponBoneIndex) * m_CombinedWorldMatrix;
	const Vector3 hilt = m_ThrownBlade.Translation();
	if (const auto target = player->Get_LockOnTarget().lock())
	{
		Vector3 toFeet = target->Get_Transform()->Get_Position() - hilt;
		toFeet.Normalize();
		if (toFeet.y <= MinThrowDrop) m_ThrowDirection = toFeet;
	}
	Vector3 blade = Vector3::TransformNormal(BladeTip, m_ThrownBlade);
	blade.Normalize();
	m_ThrownBlade.Translation(Vector3::Zero);
	m_ThrownBlade *= Matrix::CreateFromQuaternion(Quaternion::FromToRotation(blade, m_ThrowDirection));
	m_ThrownBlade.Translation(hilt);
	m_ThrowElapsed = 0.f;
	m_ReturnToSheath = false;
	m_ThrowPhase = THROW_PHASE::FLYING;
}

void WP0070Body::Recall_ThrownWeapon()
{
	// Forced recall can teleport from the thrown blade back to an animated hand.
	if (Is_Thrown()) m_SoftBody->Set_Simulation(m_SoftBody->Get_Simulation());
	m_ThrowPhase = THROW_PHASE::ATTACHED;
	m_ReturnToSheath = false;
	m_AttackCollider->Set_Active(false);
}

void WP0070Body::Active_LightWeapon()
{
	m_AttackCollider->Set_Active(true);
	if (m_Model->Get_AnimationIndex() != ETOI(WP0070_STATE::LIGHT_GROUND6)) m_HitEntities.clear();
	if (const auto player = static_pointer_cast<Pl0000>(Get_Owner()))
		player->Swing_SwordWave(static_pointer_cast<Pl0000Parts>(shared_from_this()));
}

void WP0070Body::DeActive_LightWeapon()
{
	m_AttackCollider->Set_Active(false);
	if (m_Model->Get_AnimationIndex() != ETOI(WP0070_STATE::LIGHT_GROUND6)) m_HitEntities.clear();
}

void WP0070Body::Impact_Shockwave(const Vector3& position)
{
	Entity::DAMAGE_INFO dmgInfo{};
	dmgInfo.attacker = Get_Owner();
	dmgInfo.damage = 125.f;
	dmgInfo.groggyWeight = 125.f;
	dmgInfo.attackType = ATK_TYPE::LIGHT;
	dmgInfo.hitPosition = position;
	dmgInfo.hitRotation = Quaternion::Identity;
	dmgInfo.knockbackForce = 2.f;

	Pl0000Shockwave::PLAYER_SHOCKWAVE_DESC desc{};
	desc.damageInfo = dmgInfo;
	desc.position = position;
	desc.radius = 2.f;
	GAME_INSTANCE->Instantiate<Pl0000Shockwave>(L"Pl0000Shockwave", Get_LevelIndex(), &desc);
}

HRESULT WP0070Body::Bind_ShaderResources()
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

HRESULT WP0070Body::Ready_Components()
{
	Shader::SHADER_DESC shaderDesc{ WeaponShaderTag,  VTXANIMMESH::Elements, VTXANIMMESH::numElements };
	m_Shader = Add_Component<Shader>(ETOI(LEVEL::STATIC), &shaderDesc);
	if (nullptr == m_Shader)
		return E_FAIL;

	Model::MODEL_DESC modelDesc{ L"wp0070" };
	m_Model = Add_Component<Model>(ETOI(LEVEL::STATIC), &modelDesc);
	if (nullptr == m_Model)
		return E_FAIL;
	if (FAILED(m_Model->Validate_MaterialBindings(m_Shader))) return E_FAIL;

	m_SoftBody = Add_Component<SoftBody>(ETOI(LEVEL::STATIC));
	if (!m_SoftBody) return E_FAIL;

	OBBCollider::OBB_COLLIDER_DESC colDesc{};
	colDesc.extents = Vector3{ 0.075f, 0.1f, 1.f }; 
	colDesc.rotation = Vector3::Zero;
	colDesc.offset = Vector3{ 0.f, 0.f, -0.55f };
	m_AttackCollider = Add_Component<OBBCollider>(ETOI(LEVEL::STATIC), &colDesc);
	if (nullptr == m_AttackCollider)
		return E_FAIL;

	SphereCollider::SPHERE_COLLIDER_DESC launchDesc{};
	launchDesc.radius = 2.1f;
	launchDesc.offset = Vector3{ 0.f, 1.2f, 1.4f };
	m_LaunchCollider = Add_Component<SphereCollider>(ETOI(LEVEL::STATIC), &launchDesc);
	if (!m_LaunchCollider) return E_FAIL;
	m_LaunchCollider->Set_Active(false);
	
	return S_OK;
}

HRESULT WP0070Body::Ready_AnimationNotify()
{
	using Notify = AnimationTracker::ANIMATION_NOTIFY;

	auto active = [this]() { Active_LightWeapon(); };
	auto deActive = [this]() { DeActive_LightWeapon(); };
	// The air combo's swings also strike through the sphere: the blade alone grazes a machine hanging beside her.
	auto activeAir = [this]() { Active_LightWeapon(); m_LaunchCollider->Set_Active(true); };
	auto deActiveAir = [this]() { DeActive_LightWeapon(); End_LaunchAttack(); };

	m_Model->Add_AnimNotify(ETOI(WP0070_STATE::LIGHT_GROUND_RUN), {
	Notify{ L"Sound_Reset", 0, []() { GAME_INSTANCE->Reset_SoundSequence(L"Wp0070_Run"); } },
	Notify{ L"wpf000_combo_swing_01", 7, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0070_Run", SOUNDCHANNEL::CHANNEL_10, 0.3f); } },
		Notify{L"Swing1", 10, 20, active, deActive }, 
		});

	m_Model->Add_AnimNotify(ETOI(WP0070_STATE::LIGHT_GROUND1), {
	Notify{ L"Sound_Reset", 0, []() { GAME_INSTANCE->Reset_SoundSequence(L"Wp0070_Combo1"); } },
	Notify{ L"wpf000_combo_swing_01", 7, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0070_Combo1", SOUNDCHANNEL::CHANNEL_10, 0.3f); } },
		Notify{L"Swing1", 10, 20, active, deActive },
		});

	m_Model->Add_AnimNotify(ETOI(WP0070_STATE::LIGHT_GROUND2), {
	Notify{ L"Sound_Reset", 0, []() { GAME_INSTANCE->Reset_SoundSequence(L"Wp0070_Combo2"); } },
	Notify{ L"wpf000_combo_swing_02", 6, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0070_Combo2", SOUNDCHANNEL::CHANNEL_10, 0.3f); } },
		Notify{L"Swing2", 10, 20, active, deActive },
		});

	m_Model->Add_AnimNotify(ETOI(WP0070_STATE::LIGHT_GROUND3), {
	Notify{ L"Sound_Reset", 0, []() { GAME_INSTANCE->Reset_SoundSequence(L"Wp0070_Combo3"); } },
	Notify{ L"wpf000_combo_swing_01", 6, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0070_Combo3", SOUNDCHANNEL::CHANNEL_10, 0.3f); } },
	Notify{ L"wpf000_combo_swing_03_throw_wsh", 6, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0070_Combo3", SOUNDCHANNEL::CHANNEL_10, 0.3f); } },
	Notify{ L"wpf000_combo_swing_03_throw_sword", 7, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0070_Combo3", SOUNDCHANNEL::CHANNEL_10, 0.3f); } },
		Notify{L"Swing3", 10, 100, active, deActive },
		Notify{L"Swing3", 30, active,  },
		Notify{L"Swing3", 40, active,  },
		Notify{L"Swing3", 45, active,  },
		Notify{L"Swing3", 50, active },
		});

	m_Model->Add_AnimNotify(ETOI(WP0070_STATE::LIGHT_GROUND4), {
	Notify{ L"Sound_Reset", 0, []() { GAME_INSTANCE->Reset_SoundSequence(L"Wp0070_Combo4"); } },
	Notify{ L"wpf000_combo_swing_04_return", 0, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0070_Combo4", SOUNDCHANNEL::CHANNEL_10, 0.3f); } },
	Notify{ L"wpf000_combo_swing_04_catch", 8, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0070_Combo4", SOUNDCHANNEL::CHANNEL_10, 0.3f); } },
	Notify{ L"wpf000_combo_swing_02", 21, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0070_Combo4", SOUNDCHANNEL::CHANNEL_10, 0.3f); } },
	Notify{ L"wpf000_combo_swing_04_throw_wsh", 21, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0070_Combo4", SOUNDCHANNEL::CHANNEL_10, 0.3f); } },
	Notify{ L"wpf000_combo_swing_04_throw_sword", 22, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0070_Combo4", SOUNDCHANNEL::CHANNEL_10, 0.3f); } },
		Notify{L"Swing4", 25, 100, active, deActive },
		Notify{L"Swing4", 30, active,  },
		Notify{L"Swing4", 40, active,  },
		Notify{L"Swing4", 45, active,  },
		Notify{L"Swing4", 50, active },
		});

	m_Model->Add_AnimNotify(ETOI(WP0070_STATE::LIGHT_GROUND5), {
	Notify{ L"Sound_Reset", 0, []() { GAME_INSTANCE->Reset_SoundSequence(L"Wp0070_Combo5"); } },
	Notify{ L"wpf000_combo_swing_05_return", 0, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0070_Combo5", SOUNDCHANNEL::CHANNEL_10, 0.3f); } },
	Notify{ L"wpf000_combo_swing_06_catch", 7, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0070_Combo5", SOUNDCHANNEL::CHANNEL_10, 0.3f); } },
	Notify{ L"wpf000_combo_swing_01", 21, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0070_Combo5", SOUNDCHANNEL::CHANNEL_10, 0.3f); } },
	Notify{ L"wpf000_combo_swing_05_throw_wsh", 21, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0070_Combo5", SOUNDCHANNEL::CHANNEL_10, 0.3f); } },
	Notify{ L"wpf000_combo_swing_05_throw_sword", 22, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0070_Combo5", SOUNDCHANNEL::CHANNEL_10, 0.3f); } },
		Notify{L"Swing5", 25, 100, active, deActive },
		Notify{L"Swing5", 30, active,  },
		Notify{L"Swing5", 40, active,  },
		Notify{L"Swing5", 45, active,  },
		Notify{L"Swing5", 50, active },
		});

	m_Model->Add_AnimNotify(ETOI(WP0070_STATE::LIGHT_GROUND6), {
	Notify{ L"Sound_Reset", 0, []() { GAME_INSTANCE->Reset_SoundSequence(L"Wp0070_Combo6"); } },
	Notify{ L"wpf000_combo_swing_06_return", 0, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0070_Combo6", SOUNDCHANNEL::CHANNEL_10, 0.3f); } },
	Notify{ L"wpf000_combo_swing_06_catch", 9, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0070_Combo6", SOUNDCHANNEL::CHANNEL_10, 0.3f); } },
	Notify{ L"wpf000_combo_swing_06_wsh", 10, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0070_Combo6", SOUNDCHANNEL::CHANNEL_10, 0.3f); } },
		Notify{L"Swing6", 20, 50, active, deActive },
		});

	m_Model->Add_AnimNotify(ETOI(WP0070_STATE::LIGHT_GROUND7), {
	Notify{ L"Sound_Reset", 0, []() { GAME_INSTANCE->Reset_SoundSequence(L"Wp0070_Combo7"); } },
	Notify{ L"wpf000_combo_swing_07_throw_sword", 7, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0070_Combo7", SOUNDCHANNEL::CHANNEL_10, 0.3f); } },
		});

	m_Model->Add_AnimNotify(ETOI(WP0070_STATE::LIGHT_GROUND_HOLD), {
	Notify{ L"Sound_Reset", 0, []() { GAME_INSTANCE->Reset_SoundSequence(L"Wp0070_Hold"); } },
	Notify{ L"wpf000_combo_swing_01", 8, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0070_Hold", SOUNDCHANNEL::CHANNEL_10, 0.3f); } },
	Notify{ L"wpf000_0660_wsh_1st", 22, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0070_Hold", SOUNDCHANNEL::CHANNEL_10, 0.3f); } },
	Notify{ L"wpf000_0660_wsh_2nd", 45, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0070_Hold", SOUNDCHANNEL::CHANNEL_10, 0.3f); } },
		Notify{L"Hold", 20, 60, active, deActive },
		Notify{L"Hold", 33, active },
		Notify{L"Hold", 46, active },
		Notify{L"Hold", 59, active },
		});

	m_Model->Add_AnimNotify(ETOI(WP0070_STATE::LIGHT_COMBO), {
	Notify{ L"Sound_Reset", 0, []() { GAME_INSTANCE->Reset_SoundSequence(L"Wp0070_ComboExtra"); } },
	Notify{ L"wpf000_combo_swing_01", 8, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0070_ComboExtra", SOUNDCHANNEL::CHANNEL_10, 0.3f); } },
	Notify{ L"wpf000_combo_swing_02", 23, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0070_ComboExtra", SOUNDCHANNEL::CHANNEL_10, 0.3f); } },
	Notify{ L"wpf000_combo_swing_01", 63, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0070_ComboExtra", SOUNDCHANNEL::CHANNEL_10, 0.3f); } },
	Notify{ L"wpf000_sword_end", 179, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0070_ComboExtra", SOUNDCHANNEL::CHANNEL_10, 0.3f); } },
		Notify{L"Combo", 10, 70, active, deActive },
		Notify{L"Combo", 25, active },
		Notify{L"Combo", 60, active },
		});

	// Air combo clips (Animation_Documentation_Automata.xlsx): 0130 53 frames, 0131 71 frames, 0132 91 frames.
	m_Model->Add_AnimNotify(ETOI(WP0070_STATE::LIGHT_AIR1), {
	Notify{ L"Sound_Reset", 0, []() { GAME_INSTANCE->Reset_SoundSequence(L"Wp0070_Air1"); } },
	Notify{ L"wpf000_combo_swing_01", 7, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0070_Air1", SOUNDCHANNEL::CHANNEL_10, 0.3f); } },
		Notify{L"AirSwing1", 8, 22, activeAir, deActiveAir },
		});

	m_Model->Add_AnimNotify(ETOI(WP0070_STATE::LIGHT_AIR2), {
	Notify{ L"Sound_Reset", 0, []() { GAME_INSTANCE->Reset_SoundSequence(L"Wp0070_Air2"); } },
	Notify{ L"wpf000_combo_swing_02", 6, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0070_Air2", SOUNDCHANNEL::CHANNEL_10, 0.3f); } },
		Notify{L"AirSwing2", 8, 24, activeAir, deActiveAir },
		});

	m_Model->Add_AnimNotify(ETOI(WP0070_STATE::LIGHT_AIR3), {
	Notify{ L"Sound_Reset", 0, []() { GAME_INSTANCE->Reset_SoundSequence(L"Wp0070_Air3"); } },
	Notify{ L"wpf000_combo_swing_01", 6, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0070_Air3", SOUNDCHANNEL::CHANNEL_10, 0.3f); } },
	Notify{ L"wpf000_combo_swing_03_throw_wsh", 6, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0070_Air3", SOUNDCHANNEL::CHANNEL_10, 0.3f); } },
	Notify{ L"wpf000_combo_swing_03_throw_sword", 7, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0070_Air3", SOUNDCHANNEL::CHANNEL_10, 0.3f); } },
		Notify{L"AirSwing3", 12, 35, activeAir, deActiveAir },
		});

	// The air combo's closing stab (018d enter 28 frames, 018e hold, 018f landing): it strikes where the tip meets the ground.
	m_Model->Add_AnimNotify(ETOI(WP0070_STATE::LIGHT_AIR_DOWN_ENTER), {
	Notify{ L"Sound_Reset", 0, []() { GAME_INSTANCE->Reset_SoundSequence(L"Wp0070_AirDownEnter"); } },
	Notify{ L"wpf000_jump_s_atk_wsh", 24, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0070_AirDownEnter", SOUNDCHANNEL::CHANNEL_10, 0.3f); } },
	Notify{ L"wpf000_jump_atk_wsh", 25, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0070_AirDownEnter", SOUNDCHANNEL::CHANNEL_10, 0.3f); } },
		});

	m_Model->Add_AnimNotify(ETOI(WP0070_STATE::LIGHT_AIR_DOWN_END), {
	Notify{ L"Sound_Reset", 0, []() { GAME_INSTANCE->Reset_SoundSequence(L"Wp0070_AirDownEnd"); } },
	Notify{ L"wpf000_jump_atk_impact", 0, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0070_AirDownEnd", SOUNDCHANNEL::CHANNEL_10, 0.3f); } },
	Notify{ L"wpf000_sword_roll_02", 90, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0070_AirDownEnd", SOUNDCHANNEL::CHANNEL_10, 0.3f); } },
		Notify{L"Impact", 2, [this]() { Impact_Shockwave(Vector3::Transform(BladeTip, m_Model->Get_BoneMatrix(m_WeaponBoneIndex) * m_CombinedWorldMatrix)); } },
		});

	return S_OK;
}

Shared<WP0070Body> WP0070Body::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<WP0070Body>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : WP0070Body");
		return nullptr;
	}

	return prototype;
}

Shared<GameObject> WP0070Body::Clone(void* arg)
{
	auto instance = make_shared<WP0070Body>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : WP0070Body");
		return nullptr;
	}

	return instance;
}
