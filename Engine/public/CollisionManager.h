#pragma once
#include "EngineManager.h"

NS_BEGIN(Engine)

class Collider;
class PhysicsManager;

class CollisionManager final : public EngineManager
{
	NO_COPY(CollisionManager)
public:
	explicit CollisionManager();
	~CollisionManager() override;

public: 
	HRESULT Initialize(void* arg) override;
	void On_Destroy() override;
	void On_Disable() override;
	void On_Enable() override;
	void Set_Active(Bool isActive) override;

public: 
	void Add_Collider(const Shared<Collider>& collider);
	/// physics가 없으면(종료 중) PhysX 도형은 PhysicsManager가 한꺼번에 정리한다.
	void Remove_Collider(const Shared<Collider>& collider, PhysicsManager* physics);

public: 
	/// 겹침은 PhysX가 찾고, 레이어 필터와 Enter/Stay/Exit는 여기서 정한다.
	void Update_Collision(PhysicsManager& physics);

public: /* 디버그 렌더링 */
#ifdef _DEBUG
	void Render_Debug() const;
	Bool Toggle_DebugMode()
	{
		m_DebugMode = !m_DebugMode;
		return m_DebugMode;
	}
	Bool Get_DebugMode() const { return m_DebugMode; }
#endif

private:
	vector<Shared<Collider>> m_Colliders;
	// 콜라이더마다 PhysX 쿼리 도형 하나. 판정에 처음 참여할 때 만든다.
	unordered_map<const Collider*, uint32> m_QueryShapes;
	// 이번 프레임 판정. 매 프레임 다시 채우며 할당만 재사용한다.
	unordered_map<uint32, size_t> m_IndexByShape;
	unordered_map<const Collider*, size_t> m_IndexByCollider;
	vector<uint32> m_Hits;
	vector<size_t> m_HitIndices;
	vector<size_t> m_Candidates;
#ifdef _DEBUG
	Bool m_DebugMode = { false };
#endif
public:
	static Unique<CollisionManager> Create();
};

NS_END