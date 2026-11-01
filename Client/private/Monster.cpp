#include "pch.h"
#include "Monster.h"

#include <Game.h>
#include <Random_Helper.h>
#include <Transform.h>

#include "SparkEffect.h"
#include "SpdLogger.h"
#include "Collider.h"

Monster::Monster() : Entity{} {}
Monster::Monster(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: Entity{device, context} {}
Monster::Monster(const Monster& rhs)
	: Entity{rhs} {}

HRESULT Monster::Initialize_Prototype()
{
	m_LayerMask.Set_Layer(L"MonsterPhysical");
	m_TagMask.Set_Tag({ L"Monster" });

	return Entity::Initialize_Prototype();
}

HRESULT Monster::Initialize(void* arg)
{
	if (FAILED(Entity::Initialize(arg)))
	{
		LOG_ERROR(L"Failed to Initialize Monster {}", m_ObjectName);
		return E_FAIL;
	}

	const auto layers = GAME_INSTANCE->Get_LayerRegister();
	m_PlayerPhysicalLayerIndex = ETOI(layers->Get_LayerByName(L"PlayerPhysical"));
	m_MonsterPhysicalLayerIndex = ETOI(layers->Get_LayerByName(L"MonsterPhysical"));

	return S_OK;
}

void Monster::On_Destroy()
{
	m_HpBarUI.reset();
	Entity::On_Destroy();
}

void Monster::On_Enable()
{
	Entity::On_Enable();
}

void Monster::On_Disable()
{
	Entity::On_Disable();
}

void Monster::Priority_Update(Float timeDelta)
{

}

void Monster::Update(Float timeDelta)
{
	
}

void Monster::Late_Update(Float timeDelta)
{
}

void Monster::Fixed_Update(Float fixedDelta)
{
	
}

HRESULT Monster::Render()
{
	return S_OK;
}

void Monster::Submit_RenderGroup()
{
}

void Monster::OnCollisionEnter(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{

}

void Monster::OnCollisionStay(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
	// A static monster stands its ground; the player is pushed out of it instead (Pl0000::OnCollisionStay).
	if (Is_Static()) return;

	auto target = targetCollider->Get_Owner();

	if (!Cast<Entity>(target)) return;

	const uint32 targetLayer = target->Get_LayerMask().Get_Layer();
	if (targetLayer == m_PlayerPhysicalLayerIndex)
	{
		Vector3 pushDir = PushoutDelta(ownCollider, targetCollider, 1.0f);
		Apply_PushoutCorrection(pushDir);
	}
	if (targetLayer == m_MonsterPhysicalLayerIndex)
	{
		Vector3 pushDir = PushoutDelta(ownCollider, targetCollider, 0.5f);
		Apply_PushoutCorrection(pushDir);
	}
}

void Monster::OnCollisionExit(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{

}

void Monster::TakeDamage(const DAMAGE_INFO& dmgInfo)
{
	if (m_TargetPlayer.expired())
		m_TargetPlayer = dmgInfo.attacker;

	Entity::TakeDamage(dmgInfo);
}

Bool Monster::Is_TargetFront() const
{
	if (m_TargetPlayer.expired()) return false;
	
	Vector3 pos = m_Transform->Get_Look();
	Vector3 targetPos = m_TargetPlayer.lock()->Get_Transform()->Get_Position();
	Vector3 toTarget = targetPos - m_Transform->Get_Position();

	Float scalar = pos.Dot(toTarget);

	return scalar > 0.f;
}

Vector3 Monster::Get_DirectionToTarget() const
{
	if (m_TargetPlayer.expired()) return Vector3::Zero;

	Vector3 currentPos = m_Transform->Get_Position();
	Vector3 targetPos = m_TargetPlayer.lock()->Get_Transform()->Get_Position();
	Vector3 toTarget = targetPos - currentPos;
	toTarget.y = 0.f;

	if (toTarget.Length() < 0.001f) return Vector3::Zero;

	toTarget.Normalize();
	return toTarget;
}

const TRANSFORM_FRAME& Monster::Get_BodyModelTransform() const
{
	static const TRANSFORM_FRAME none{};
	return none;
}

Float Monster::Get_DistanceToTarget() const
{
	if (m_TargetPlayer.expired()) return 0;

	Vector3 currentPos = m_Transform->Get_Position();
	Vector3 targetPos = m_TargetPlayer.lock()->Get_Transform()->Get_Position();
	Vector3 toTarget = targetPos - currentPos;
	toTarget.y = 0.f;

	return toTarget.Length();
}

void Monster::Play_HitSFX(const DAMAGE_INFO& dmgInfo) const
{
	// 타격음 프리셋(SoundSettings.json)이 변형 중 하나를 랜덤으로 고른다. 채널을 멈추지 않아 연타 소리가 겹친다.
	const tChar* tag = nullptr;
	if (dmgInfo.attackType == ATK_TYPE::LIGHT)
		tag = L"SwordHit";
	else if (dmgInfo.attackType == ATK_TYPE::HEAVY)
		tag = L"BigSwordHit";
	else if (dmgInfo.attackType == ATK_TYPE::POD)
		tag = L"BulletHit";
	if (nullptr != tag)
	{
		const Vector3 position = m_Transform->Get_Position();
		GAME_INSTANCE->PlaySoundFX(tag, SOUNDCHANNEL::CHANNEL_20, 0.3f, &position);
	}
}

void Monster::DisplaySparkEffect(ATK_TYPE atkType, Vector3 position, Quaternion rotation) const
{
	SparkEffect::SPARK_EFFECT_DESC sparkDesc{};
	sparkDesc.atkType = atkType;
	sparkDesc.position = position;
	sparkDesc.rotation = rotation;
	GAME_INSTANCE->Instantiate<SparkEffect>(L"SparkEffect", Get_LevelIndex(), &sparkDesc);
}

