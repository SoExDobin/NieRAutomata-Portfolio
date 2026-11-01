#include "CollisionManager.h"
#include "Collider.h"
#include "Game.h"
#include "GameObject.h"
#include "PhysicsManager.h"

CollisionManager::CollisionManager()
{
}

CollisionManager::~CollisionManager()
{
	m_Colliders.clear();
}

HRESULT CollisionManager::Initialize(void* arg)
{
	return EngineManager::Initialize(arg);
}

void CollisionManager::On_Destroy()
{
	m_Colliders.clear();
	m_QueryShapes.clear();
}
void CollisionManager::On_Disable() { EngineManager::On_Disable(); }
void CollisionManager::On_Enable() { EngineManager::On_Enable(); }
void CollisionManager::Set_Active(Bool isActive) { EngineManager::Set_Active(isActive); }

void CollisionManager::Add_Collider(const Shared<Collider>& collider)
{
	m_Colliders.push_back(collider);
}

void CollisionManager::Remove_Collider(const Shared<Collider>& collider, PhysicsManager* physics)
{
	if (nullptr == collider) return;

	if (const auto shape = m_QueryShapes.find(collider.get()); shape != m_QueryShapes.end())
	{
		if (physics)
			physics->Remove_QueryShape(shape->second);
		m_QueryShapes.erase(shape);
	}

	for (auto& col : m_Colliders)
	{
		if (col && col != collider)
			col->Release_OverlapMember(collider);
	}

	auto iter = ranges::find_if(m_Colliders.begin(), m_Colliders.end(), [collider](const Shared<Collider>& col) {
		return col == collider;
	});
	if (iter != m_Colliders.end())
	{
		m_Colliders.erase(iter);
	}
}

void CollisionManager::Update_Collision(PhysicsManager& physics)
{
	/* 판정 전에 참여하는 콜라이더의 도형을 모두 이번 프레임 모양으로 맞춘다.
	   비활성 콜라이더의 도형은 지난 자세로 남지만 아래 루프가 지금처럼 걸러낸다. */
	m_IndexByShape.clear();
	m_IndexByCollider.clear();
	for (size_t i = 0; i < m_Colliders.size(); ++i)
	{
		const auto& collider = m_Colliders[i];
		if (!collider || collider->Is_Destroy() || !collider->Is_Active()) continue;
		const auto owner = collider->Get_Owner();
		if (!owner || owner->Is_Destroy() || !owner->Is_Active()) continue;

		QUERY_SHAPE_DESC desc{};
		collider->Fill_QueryShape(desc);
		const LAYER_MASK& layerMask = owner->Get_LayerMask();
		const uint32 layerBit = layerMask.Get_Layer();
		const uint32 maskBits = layerMask.Is_GlobalMask() ?
			GAME_INSTANCE->Get_LayerRegister()->Get_GlobalMask(layerBit) : layerMask.Get_Mask();

		uint32& shape = m_QueryShapes[collider.get()];
		if (0 == shape)
			shape = physics.Add_QueryShape(desc, layerBit, maskBits);
		else
			physics.Update_QueryShape(shape, desc, layerBit, maskBits);

		if (0 != shape)
			m_IndexByShape.emplace(shape, i);
		m_IndexByCollider.emplace(collider.get(), i);
	}

	for (size_t i = 0; i < m_Colliders.size(); ++i)
	{
		auto srcCol = m_Colliders[i];
		if (!srcCol || srcCol->Is_Destroy() || !srcCol->Is_Active()) continue;
		auto srcOwner = srcCol->Get_Owner();
		if (!srcOwner || srcOwner->Is_Destroy() || !srcOwner->Is_Active()) continue;

		/* 상대 후보는 둘뿐이다. PhysX가 이번에 겹친다고 한 콜라이더(Enter/Stay)와 지난 프레임까지
		   겹쳐 있던 콜라이더(Exit). 나머지는 콜백이 없으므로 보지 않는다. 쌍은 한 번만 다루도록
		   뒤 순번(j > i)만 남기고, 콜백 순서가 예전과 같도록 순번대로 정렬한다. */
		m_HitIndices.clear();
		if (const auto shape = m_QueryShapes.find(srcCol.get()); shape != m_QueryShapes.end() && 0 != shape->second)
		{
			physics.Overlap_QueryShape(shape->second, m_Hits);
			for (const uint32 hit : m_Hits)
				if (const auto index = m_IndexByShape.find(hit); index != m_IndexByShape.end() && index->second > i)
					m_HitIndices.push_back(index->second);
		}
		std::sort(m_HitIndices.begin(), m_HitIndices.end());

		m_Candidates = m_HitIndices;
		for (const Collider* member : srcCol->m_OverlapMembers)
			if (const auto index = m_IndexByCollider.find(member); index != m_IndexByCollider.end() && index->second > i)
				m_Candidates.push_back(index->second);
		std::sort(m_Candidates.begin(), m_Candidates.end());
		m_Candidates.erase(std::unique(m_Candidates.begin(), m_Candidates.end()), m_Candidates.end());

		for (const size_t j : m_Candidates)
		{
			auto dstCol = m_Colliders[j];
			if (!dstCol || dstCol->Is_Destroy() || !dstCol->Is_Active()) continue;
			auto dstOwner = dstCol->Get_Owner();
			if (!dstOwner || dstOwner->Is_Destroy() || !dstOwner->Is_Active()) continue;

			// --- Layer Mask 필터링 ---
			uint32 srcLayerBit = srcOwner->Get_LayerMask().Get_Layer();
			uint32 dstLayerBit = dstOwner->Get_LayerMask().Get_Layer();
			
			uint32 srcMaskBits = srcOwner->Get_LayerMask().Is_GlobalMask() ?
				GAME_INSTANCE->Get_LayerRegister()->Get_GlobalMask(srcLayerBit)
				: srcOwner->Get_LayerMask().Get_Mask();
			uint32 dstMaskBits = dstOwner->Get_LayerMask().Is_GlobalMask() ?
				GAME_INSTANCE->Get_LayerRegister()->Get_GlobalMask(dstLayerBit)
				: dstOwner->Get_LayerMask().Get_Mask();

			if ((srcMaskBits & dstLayerBit) == 0 || (dstMaskBits & srcLayerBit) == 0)
				continue;
			// -------------------------

			if (std::binary_search(m_HitIndices.begin(), m_HitIndices.end(), j))
			{
				srcCol->Set_IsColliding(true);
				dstCol->Set_IsColliding(true);

				if (!srcCol->Is_Overlapped(dstCol))
				{
					srcCol->Add_OverlapMember(dstCol);
					srcOwner->OnCollisionEnter(srcCol, dstCol);

					dstCol->Add_OverlapMember(srcCol);
					dstOwner->OnCollisionEnter(dstCol, srcCol);
				}
				else
				{
					srcOwner->OnCollisionStay(srcCol, dstCol);
					dstOwner->OnCollisionStay(dstCol, srcCol);
				}
			}
			else
			{
				if (srcCol->Is_Overlapped(dstCol))
				{
					srcCol->Release_OverlapMember(dstCol);
					srcOwner->OnCollisionExit(srcCol, dstCol);

					dstCol->Release_OverlapMember(srcCol);
					dstOwner->OnCollisionExit(dstCol, srcCol);
				}
			}
		}
	}
}

#ifdef _DEBUG
void CollisionManager::Render_Debug() const
{
	if (!m_DebugMode)
		return;
	DebugRenderer* debug = GAME_INSTANCE->Get_DebugRenderer();
	if (!debug)
		return;

	const auto& batch = debug->Begin_Lines(Matrix::Identity, GAME_INSTANCE->Get_Transform(D3DTS::VIEW), GAME_INSTANCE->Get_Transform(D3DTS::PROJ));
	for (auto& collider : m_Colliders)
	{
		if (collider && collider->Is_Active() && !collider->Is_Destroy())
		{
			Bool colliding = collider->Get_IsColliding();
			FXMVECTOR color = colliding ? DirectX::Colors::Red : DirectX::Colors::Green;
			collider->Render_Debug(batch, color);
			collider->Set_IsColliding(false);
		}
	}
	debug->End_Lines();
}

#endif

Unique<CollisionManager> CollisionManager::Create()
{
	auto collisionManager = make_unique<CollisionManager>();

	if (collisionManager->Initialize_Prototype())
	{
		MSG_BOX("Failed to Create CollisionManager");
		return nullptr;
	}

	return collisionManager;
}
