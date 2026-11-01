#include "pch.h"
#include "WP0220Body.h"

#include <SpdLogger.h>

#include "Game.h"
#include "Model.h"
#include "Monster.h"
#include "OBBCollider.h"
#include "Pl0000Shockwave.h"
#include "Entity.h"
#include "Pl0000.h"

WP0220Body::WP0220Body() : Pl0000Parts{} {}
WP0220Body::WP0220Body(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: Pl0000Parts{ device, context } {}
WP0220Body::WP0220Body(const WP0220Body& rhs)
	: Pl0000Parts{ rhs } {}

HRESULT WP0220Body::Initialize_Prototype()
{
	m_LayerMask.Set_Layer(L"PlayerWeapon");
	m_TagMask.Set_Tag({L"PlayerWeapon" });

	return Pl0000Parts::Initialize_Prototype();
}

HRESULT WP0220Body::Initialize(void* arg)
{
	if (FAILED(Pl0000Parts::Initialize(arg)))
		return E_FAIL;

	if (FAILED(Ready_Components()))
	{
		LOG_ERROR(L"Failed To Ready_Components : WP0220Body");
		return E_FAIL;
	}

	if (FAILED(Ready_AnimationNotify()))
	{
		LOG_ERROR(L"Failed To Ready Animation Notify : WP0220Body");
		return E_FAIL;
	}

	m_RootBoneIndex = m_Model->Get_BoneIndexByName("wp0220");
	m_WeaponBoneIndex = m_Model->Get_BoneIndexByName("bone0");
	if (m_RootBoneIndex == -1)
	{
		LOG_ERROR(L"Failed to Find WP0220 Root Bone");
		return E_FAIL;
	}

	m_Model->Set_LocalRootNode(m_RootBoneIndex);

	return S_OK;
}

void WP0220Body::On_Destroy()
{
	PartObject::On_Destroy();
}

void WP0220Body::Priority_Update(Float timeDelta)
{

}

void WP0220Body::Update(Float timeDelta)
{
	m_Model->Update_ModelAnimation(timeDelta);

	if (m_IsSheathing == false)
	{
		TRANSFORM_FRAME rootVelocity = m_Model->Get_RootTransformVelocity(m_WeaponBoneIndex);

	}
}

void WP0220Body::Late_Update(Float timeDelta)
{
	m_PrevAttackPivot = m_AttackCollider->Get_Pivot();
	m_Transform->Update_WorldMatrix();
	Update_CombineWorldMatrix(*m_Transform->Get_WorldMatrixPtr());
	
	Matrix boneMatrix = m_Model->Get_BoneMatrix(m_WeaponBoneIndex);
	m_AttackCollider->Update(boneMatrix * m_CombinedWorldMatrix);
}

void WP0220Body::Fixed_Update(Float fixedDelta)
{

}

HRESULT WP0220Body::Render()
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

void WP0220Body::Submit_RenderGroup()
{
	if (m_IsActive == false) return;
	GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::NONBLEND, shared_from_this());
}

void WP0220Body::OnCollisionEnter(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{

}

void WP0220Body::OnCollisionStay(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
	if (!m_AttackCollider->Is_Active()) return;


	auto target = targetCollider->Get_Owner();
	if (target->Get_GameObjectType() != GAMEOBJECTTYPE::PART) return;
	if (target->Get_LayerMask().Get_LayerName() != L"Monster") return;

	const RuntimeObjectId targetID = target->Get_RuntimeObjectId();
	if (m_HitEntities.contains(targetID) == false)
	{
		m_HitEntities.insert(targetID);

		Entity::DAMAGE_INFO dmgInfo{};
		dmgInfo.attacker = Get_Owner();
		dmgInfo.damage = 175.f;
		dmgInfo.groggyWeight = 175.f;
		dmgInfo.attackType = ATK_TYPE::HEAVY;
		dmgInfo.blowAway = true;
		dmgInfo.hitPosition = targetCollider->ClosestPoint(ownCollider->Get_Pivot());
		// The blade's travel since last frame, so the sparks fly along the swing.
		const Vector3 swing = m_AttackCollider->Get_Pivot() - m_PrevAttackPivot;
		dmgInfo.hitRotation = swing.LengthSquared() > 0.000001f
			? Quaternion::FromToRotation(Vector3::UnitY, swing / swing.Length())
			: m_AttackCollider->Get_CurrentOrientation();
		dmgInfo.knockbackForce = 2.f;

		auto monster = static_pointer_cast<Monster>(static_pointer_cast<PartObject>(target)->Get_Owner());
		monster->TakeDamage(dmgInfo);
		if (m_IsLaunchSwing)
		{
			// The same height the machine takes from 2B's sixth light hit, which she follows up and this one does not.
			constexpr Float launchHeight = 8.5f;
			monster->Launch_Up(launchHeight);
		}
		monster->OnAttackHit(shared_from_this());

		auto attacker = static_pointer_cast<Pl0000>(dmgInfo.attacker.lock());
		attacker->OnAttackHit(monster);
		attacker->Trigger_GlobalLag(0.2f, 0.2f);

	}
}

void WP0220Body::OnCollisionExit(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{

}

void WP0220Body::Active_LightWeapon()
{
	m_AttackCollider->Set_Active(true);
	m_HitEntities.clear();
	if (const auto player = static_pointer_cast<Pl0000>(Get_Owner()))
		player->Swing_SwordWave(static_pointer_cast<Pl0000Parts>(shared_from_this()));
}

void WP0220Body::DeActive_LightWeapon()
{
	m_AttackCollider->Set_Active(false);
	m_HitEntities.clear();
}

void WP0220Body::Impact_Shockwave(const Vector3& offset)
{
	Matrix boneMatrix = m_Model->Get_BoneMatrix(m_WeaponBoneIndex);
	Matrix worldMatrix = boneMatrix * m_CombinedWorldMatrix; 

	Vector3 truePos = Vector3::Transform(offset, worldMatrix);

	Entity::DAMAGE_INFO dmgInfo{};
	dmgInfo.attacker = Get_Owner();
	dmgInfo.damage = 225.f;
	dmgInfo.groggyWeight = 225.f;
	dmgInfo.attackType = ATK_TYPE::HEAVY;
	dmgInfo.hitPosition = truePos;
	dmgInfo.hitRotation = Quaternion::Identity;
	dmgInfo.knockbackForce = 3.5f;

	Pl0000Shockwave::PLAYER_SHOCKWAVE_DESC desc{};
	desc.damageInfo = dmgInfo;
	desc.position = truePos;
	desc.radius = 3.f;
	GAME_INSTANCE->Instantiate<Pl0000Shockwave>(L"Pl0000Shockwave", Get_LevelIndex(), &desc);
}

void WP0220Body::Set_Sheathing()
{
	if (m_IsSheathing) return;
	m_AttackCollider->Set_Active(false);
	m_IsLaunchSwing = false;
	m_IsSheathing = true;
	m_SnapNextAnimation = true;
}

void WP0220Body::DrawWP0220()
{
	if (m_IsSheathing == false) return;
	m_AttackCollider->Set_Active(true);
	m_IsSheathing = false;
}

void WP0220Body::Set_Animation(uint32 animIndex, Float blendDuration, Bool isLoop)
{
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
	m_Model->Update_ModelAnimation(0.0001f);
}

HRESULT WP0220Body::Bind_ShaderResources()
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

HRESULT WP0220Body::Ready_Components()
{
	Shader::SHADER_DESC shaderDesc{ WeaponShaderTag,  VTXANIMMESH::Elements, VTXANIMMESH::numElements };
	m_Shader = Add_Component<Shader>(ETOI(LEVEL::STATIC), &shaderDesc);
	if (nullptr == m_Shader)
		return E_FAIL;

	Model::MODEL_DESC modelDesc{ L"wp0220" };
	m_Model = Add_Component<Model>(ETOI(LEVEL::STATIC), &modelDesc);
	if (nullptr == m_Model)
		return E_FAIL;
	if (FAILED(m_Model->Validate_MaterialBindings(m_Shader))) return E_FAIL;

	OBBCollider::OBB_COLLIDER_DESC colDesc{};
	colDesc.extents = Vector3{ 0.25f, 0.05f, 0.8f };
	colDesc.rotation = Vector3::Zero;
	colDesc.offset = Vector3{ -0.05f, 0.f, -1.f }; 
	m_AttackCollider = Add_Component<OBBCollider>(ETOI(LEVEL::STATIC), &colDesc);
	if (nullptr == m_AttackCollider)
		return E_FAIL;

	return S_OK;
}

HRESULT WP0220Body::Ready_AnimationNotify()
{
	using Notify = AnimationTracker::ANIMATION_NOTIFY;

	auto active = [this]() { Active_LightWeapon(); };
	auto deActive = [this]() { DeActive_LightWeapon(); };
	auto activeLaunch = [this]() { Active_LightWeapon(); m_IsLaunchSwing = true; };
	auto deActiveLaunch = [this]() { DeActive_LightWeapon(); m_IsLaunchSwing = false; };
	auto impactShockwave = [this]() { Impact_Shockwave(Vector3{0.f, 0.f, -2.f }); };
	auto impactCombo = [this]() { Impact_Shockwave(Vector3{0.f, 0.f, -2.f}); };

	m_Model->Add_AnimNotify(ETOI(WP0220_STATE::HEAVY_GROUND1), {
	Notify{ L"Sound_Reset", 0, []() { GAME_INSTANCE->Reset_SoundSequence(L"Wp0220_Combo1"); } },
	Notify{ L"wpf200_combo01_swing_01", 30, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_Combo1", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
	Notify{ L"wpf200_combo01_swing_02", 58, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_Combo1", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
		Notify{L"Swing1", 30, 100, active, deActive },
		Notify{L"Swing1", 60, active,  },
		});

	m_Model->Add_AnimNotify(ETOI(WP0220_STATE::HEAVY_GROUND2), {
	Notify{ L"Sound_Reset", 0, []() { GAME_INSTANCE->Reset_SoundSequence(L"Wp0220_Combo2"); } },
	Notify{ L"wpf200_combo02_swing_01", 56, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_Combo2", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
		Notify{L"Swing2", 65, 100, active, deActive },
		Notify{L"Swing2", 80, active  },
		});

	m_Model->Add_AnimNotify(ETOI(WP0220_STATE::HEAVY_GROUND3), {
	Notify{ L"Sound_Reset", 0, []() { GAME_INSTANCE->Reset_SoundSequence(L"Wp0220_Combo3"); } },
	Notify{ L"wpf200_combo03_swing_01", 31, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_Combo3", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
	Notify{ L"wpf200_combo03_swing_02", 58, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_Combo3", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
	Notify{ L"wpf200_combo03_swing_03", 87, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_Combo3", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
	Notify{ L"wpf200_combo03_impact", 107, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_Combo3", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
		Notify{L"Swing3", 50, 70, active, deActive },
		Notify{L"Swing3", 60, active,  },
		Notify{L"Impact", 111, impactShockwave}
		// 충격파 111 프레임일때 bone위치에 Instant
		});

	m_Model->Add_AnimNotify(ETOI(WP0220_STATE::HEAVY_COMBO), {
	Notify{ L"Sound_Reset", 0, []() { GAME_INSTANCE->Reset_SoundSequence(L"Wp0220_ComboExtra"); } },
	Notify{ L"wpf200_l_combo_wsh_2nd", 29, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_ComboExtra", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
	Notify{ L"wpf200_hammer_swing", 54, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_ComboExtra", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
	Notify{ L"wpf200_l_combo_inpact_1st", 80, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_ComboExtra", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
		Notify{L"Swing3", 40, 85, active, deActive },
		Notify{L"Swing3", 80, active },
		Notify{L"Impact", 80, impactCombo }
		});

	m_Model->Add_AnimNotify(ETOI(WP0220_STATE::HEAVY_GROUND_HOLD_UNFULL), {
	Notify{ L"Sound_Reset", 0, []() { GAME_INSTANCE->Reset_SoundSequence(L"Wp0220_ChargeUnfull"); } },
	Notify{ L"wpf200_charge0_swing", 16, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_ChargeUnfull", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
	Notify{ L"wpf200_charge0_impact", 36, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_ChargeUnfull", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
		Notify{L"Impact", 40, impactShockwave}
		// 충격파
		});

	m_Model->Add_AnimNotify(ETOI(WP0220_STATE::HEAVY_GROUND_HOLD_FULL), {
	Notify{ L"Sound_Reset", 0, []() { GAME_INSTANCE->Reset_SoundSequence(L"Wp0220_ChargeFull"); } },
	Notify{ L"wpf200_charge_hit_swing_01", 4, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_ChargeFull", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
	Notify{ L"wpf200_charge_hit_swing_02", 18, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_ChargeFull", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
	Notify{ L"wpf200_charge_hit_swing_03", 26, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_ChargeFull", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
	Notify{ L"wpf200_charge_hit_swing_04", 37, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_ChargeFull", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
		Notify{L"Swing1", 0, 50, active, deActive },
		Notify{L"Swing2", 20, active },
		Notify{L"Swing2", 30, active },
		Notify{L"Swing2", 40, active },
		});

	m_Model->Add_AnimNotify(ETOI(WP0220_STATE::HEAVY_AIR_DOWN_END), {
	Notify{ L"Sound_Reset", 0, []() { GAME_INSTANCE->Reset_SoundSequence(L"Wp0220_AirDownEnd"); } },
	Notify{ L"wpf200_hammer_impact", 0, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_AirDownEnd", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
		Notify{L"Impact", 10, impactShockwave}
		});

	// wpf200_0213 원본 SeTrack 사운드 이벤트
	m_Model->Add_AnimNotify(ETOI(WP0220_STATE::HEAVY_GROUND_HOLD_NO_CONTACT), {
	Notify{ L"Sound_Reset", 0, []() { GAME_INSTANCE->Reset_SoundSequence(L"Wp0220_ChargeNoContact"); } },
	Notify{ L"wpf200_charge1_swing_01", 3, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_ChargeNoContact", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
	Notify{ L"wpf200_upper_swing", 19, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_ChargeNoContact", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
	Notify{ L"wpf200_sword_fall", 84, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_ChargeNoContact", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
	Notify{ L"wpf200_Sword_lift_a", 183, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_ChargeNoContact", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
		});

	// wpf200_021a 원본 SeTrack 사운드 이벤트
	m_Model->Add_AnimNotify(ETOI(WP0220_STATE::HEAVY_GROUND_SPRINT), {
	Notify{ L"Sound_Reset", 0, []() { GAME_INSTANCE->Reset_SoundSequence(L"Wp0220_Sprint"); } },
	Notify{ L"wpf200_dash_atk_throw", 16, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_Sprint", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
	Notify{ L"wpf200_dash_atk_throw_wsh", 45, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_Sprint", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
	Notify{ L"wpf200_dash_atk_throw_sword", 47, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_Sprint", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
		});

	// wpf200_0225 원본 SeTrack 사운드 이벤트
	m_Model->Add_AnimNotify(ETOI(WP0220_STATE::HEAVY_AIR_DOWN_ENTER), {
	Notify{ L"Sound_Reset", 0, []() { GAME_INSTANCE->Reset_SoundSequence(L"Wp0220_AirDownEnter"); } },
	Notify{ L"wpf200_hammer_swing", 4, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_AirDownEnter", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
		});

	// A2's right click. Swings, sounds and hit windows from the original wpf200_0200~0203 / 0220 SeTrack and AttackTrack.
	m_Model->Add_AnimNotify(ETOI(WP0220_STATE::LIGHT_GROUND1), {
	Notify{ L"Sound_Reset", 0, []() { GAME_INSTANCE->Reset_SoundSequence(L"Wp0220_LightCombo1"); } },
	Notify{ L"wpf200_l_combo_wsh_1st", 9, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_LightCombo1", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
	Notify{ L"wpf200_l_combo_inpact_1st", 35, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_LightCombo1", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
	Notify{ L"wpf200_Sword_lift_b", 139, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_LightCombo1", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
		Notify{L"Swing1", 31, 40, active, deActive },
		});

	m_Model->Add_AnimNotify(ETOI(WP0220_STATE::LIGHT_GROUND2), {
	Notify{ L"Sound_Reset", 0, []() { GAME_INSTANCE->Reset_SoundSequence(L"Wp0220_LightCombo2"); } },
	Notify{ L"wpf200_l_combo_scrap", 1, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_LightCombo2", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
	Notify{ L"wpf200_l_combo_wsh_2nd", 27, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_LightCombo2", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
	Notify{ L"wpf200_l_combo_inpact_1st", 40, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_LightCombo2", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
	Notify{ L"wpf200_Sword_lift_b", 139, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_LightCombo2", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
		Notify{L"Swing2", 37, 46, active, deActive },
		});

	m_Model->Add_AnimNotify(ETOI(WP0220_STATE::LIGHT_GROUND3), {
	Notify{ L"Sound_Reset", 0, []() { GAME_INSTANCE->Reset_SoundSequence(L"Wp0220_LightCombo3"); } },
	Notify{ L"wpf200_l_combo_wsh_3rd_a", 4, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_LightCombo3", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
	Notify{ L"wpf200_l_combo_wsh_3rd_b", 45, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_LightCombo3", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
	Notify{ L"wpf200_l_combo_inpact_3rd", 66, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_LightCombo3", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
	Notify{ L"wpf200_Sword_lift_c", 213, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_LightCombo3", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
		Notify{L"Swing3", 29, 38, active, deActive },
		Notify{L"Swing3", 64, 70, active, deActive },
		});

	m_Model->Add_AnimNotify(ETOI(WP0220_STATE::LIGHT_GROUND4), {
	Notify{ L"Sound_Reset", 0, []() { GAME_INSTANCE->Reset_SoundSequence(L"Wp0220_LightCombo4"); } },
	Notify{ L"wpf200_l_combo_dash_wsh", 5, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_LightCombo4", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
	Notify{ L"wpf200_l_combo_wsh_4th", 40, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_LightCombo4", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
	Notify{ L"wpf200_l_combo_fall", 75, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_LightCombo4", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
	Notify{ L"wpf200_Sword_lift_a", 239, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_LightCombo4", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
		Notify{L"Swing4", 27, 40, active, deActive },
		Notify{L"Swing4", 53, 80, activeLaunch, deActiveLaunch },
		});

	m_Model->Add_AnimNotify(ETOI(WP0220_STATE::HEAVY_AIR_UP), {
	Notify{ L"Sound_Reset", 0, []() { GAME_INSTANCE->Reset_SoundSequence(L"Wp0220_AirUp"); } },
	Notify{ L"wpf200_upper_swing", 13, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_AirUp", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
	Notify{ L"wpf200_combo03_swing_01", 20, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_AirUp", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
	Notify{ L"wpf200_charge_hit_swing_04", 33, []() { GAME_INSTANCE->PlaySoundFX(L"Wp0220_AirUp", SOUNDCHANNEL::CHANNEL_12, 0.3f); } },
		Notify{L"Swing1", 30, 48, active, deActive },
		Notify{L"Swing1", 50, 68, active, deActive },
		});

	return S_OK;
}

Shared<WP0220Body> WP0220Body::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<WP0220Body>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : WP0220Body");
		return nullptr;
	}

	return prototype;
}

Shared<GameObject> WP0220Body::Clone(void* arg)
{
	auto instance = make_shared<WP0220Body>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : WP0220Body");
		return nullptr;
	}

	return instance;
}


