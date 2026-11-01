#include "pch.h"
#include "Pl0000MonsterChecker.h"
#include "Pl0000.h"
#include <SpdLogger.h>
#include "SphereCollider.h"

Pl0000MonsterChecker::Pl0000MonsterChecker() : PartObject{} {}
Pl0000MonsterChecker::Pl0000MonsterChecker(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: PartObject{device, context} {}
Pl0000MonsterChecker::Pl0000MonsterChecker(const Pl0000MonsterChecker& rhs)
	: PartObject{rhs} {}

Shared<GameObject> Pl0000MonsterChecker::Get_ClosestTarget(Vector3 look, const Shared<GameObject>& exclude, Float maxDistance)
{
	auto player = static_pointer_cast<Pl0000>(Get_Parent());
	if (!player) return nullptr;
	Vector3 pos = player->Get_Transform()->Get_Position();
	look.y = 0.f;
	look.Normalize();

	Shared<GameObject> bestTarget{ nullptr };
	Float minDistance = FLT_MAX;

	if (m_CheckedMonsters.empty()) return nullptr;

	for (auto it = m_CheckedMonsters.begin(); it != m_CheckedMonsters.end(); )
	{
		if (it->expired())
		{
			it = m_CheckedMonsters.erase(it);
			continue;
		}

		auto targetMonster = it->lock();
		const auto entity = Cast<Entity>(targetMonster);
		if (targetMonster->Is_Destroy() || !targetMonster->Is_Active() || (entity && entity->Is_Dead()))
		{
			it = m_CheckedMonsters.erase(it);
			continue;
		}

		if (targetMonster == exclude)
		{
			++it;
			continue;
		}

		Vector3 targetPos = it->lock()->Get_Transform()->Get_Position();
		Vector3 toTarget = targetPos - pos;
		toTarget.y = 0.f;

		Float distance = toTarget.Length();
		if (distance > 0.001f)
			toTarget.Normalize();

		Float dot = look.Dot(toTarget);

		if (dot > 0.f && distance < minDistance && distance <= maxDistance)
		{
			minDistance = distance;
			bestTarget = targetMonster;
		}

		++it;
	}

	return bestTarget;
}

vector<Shared<GameObject>> Pl0000MonsterChecker::Get_MonstersAround(Float radius, Float height)
{
	vector<Shared<GameObject>> monsters;
	auto player = static_pointer_cast<Pl0000>(Get_Parent());
	if (!player) return monsters;
	const Vector3 pos = player->Get_Transform()->Get_Position();

	for (const auto& checked : m_CheckedMonsters)
	{
		auto monster = checked.lock();
		if (!monster || monster->Is_Destroy() || !monster->Is_Active()) continue;
		const auto entity = Cast<Entity>(monster);
		if (entity && entity->Is_Dead()) continue;

		Vector3 toMonster = monster->Get_Transform()->Get_Position() - pos;
		if (std::abs(toMonster.y) > height) continue;
		toMonster.y = 0.f;
		if (toMonster.Length() <= radius)
			monsters.push_back(monster);
	}

	return monsters;
}

HRESULT Pl0000MonsterChecker::Initialize_Prototype()
{
	m_LayerMask.Set_UseCustomMask(true);
	m_LayerMask.ClearMask();
	m_LayerMask.Set_Mask(L"MonsterPhysical");

	return PartObject::Initialize_Prototype();
}

HRESULT Pl0000MonsterChecker::Initialize(void* arg)
{
	if (FAILED(PartObject::Initialize(arg)))
	{
		LOG_ERROR(L"Failed To Initialize Pl0000MonsterChecker");
		return S_OK;
	}

	SphereCollider::SPHERE_COLLIDER_DESC colDesc{};
	colDesc.radius = m_CheckDistance;
	colDesc.offset = Vector3::UnitY;
	m_EvadeChecker = Add_Component<SphereCollider>(ETOI(LEVEL::STATIC), &colDesc);
	if (m_EvadeChecker == nullptr)
		return E_FAIL;

	return S_OK;
}

HRESULT Pl0000MonsterChecker::Begin()
{


	return S_OK;
}

void Pl0000MonsterChecker::On_Destroy()
{
	m_CheckedMonsters.clear();

	PartObject::On_Destroy();
}

void Pl0000MonsterChecker::Update(Float timeDelta)
{
	m_Transform->Update_WorldMatrix();
	Update_CombineWorldMatrix(*m_Transform->Get_WorldMatrixPtr());
	m_EvadeChecker->Update(m_CombinedWorldMatrix);
}

void Pl0000MonsterChecker::OnCollisionEnter(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
	if (!Get_Parent()) return;

	auto target = targetCollider->Get_Owner();

	if (target->Get_LayerMask().Get_LayerName() != L"MonsterPhysical") return;

	for (auto& monster : m_CheckedMonsters)
	{
		if (monster.expired()) continue;
		if (monster.lock() == target) return;
	}

	m_CheckedMonsters.push_back(target);
}

void Pl0000MonsterChecker::OnCollisionStay(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
	
	

}

void Pl0000MonsterChecker::OnCollisionExit(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
	if (!Get_Parent()) return;

	auto target = targetCollider->Get_Owner();

	if (target->Get_LayerMask().Get_LayerName() != L"MonsterPhysical") return;

	m_CheckedMonsters.remove_if([&target](const Weak<GameObject>& monster)
		{
			return monster.expired() || monster.lock() == target;
		});
}

Shared<Pl0000MonsterChecker> Pl0000MonsterChecker::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<Pl0000MonsterChecker>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Pl0000MonsterChecker");
		return nullptr;
	}

	return prototype;
}

Shared<GameObject> Pl0000MonsterChecker::Clone(void* arg)
{
	auto instance = make_shared<Pl0000MonsterChecker>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : Pl0000MonsterChecker");
		return nullptr;
	}

	return instance;
}



