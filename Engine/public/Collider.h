#pragma once
#include "Component.h"

NS_BEGIN(Engine)

struct QUERY_SHAPE_DESC;

class ENGINE_DLL Collider abstract : public Component
{
	// 판정 후보에 지난 프레임 겹침 상대를 넣으려고 m_OverlapMembers를 읽는다. 게임플레이에는 열지 않는다.
	friend class CollisionManager;
public:
	typedef struct tagColliderDesc : public COMPONENT_DESC
	{
		Vector3 offset;
	} COLLIDER_DESC;
protected:
	explicit Collider();
	explicit Collider(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Collider(const Collider& rhs);
	virtual ~Collider() override;
	
public:
	virtual COLLIDER_TYPE Get_ColliderType() const PURE;
	virtual COMPONENT_TYPE Get_ComponentType() const override { return COMPONENT_TYPE::COLLIDER; }
	const Vector3& Get_Offset() const { return m_Offset; }
	virtual void Set_Offset(const Vector3& offset) { m_Offset = offset; }
	virtual Vector3 Get_Pivot() const PURE;
	virtual Vector3 ClosestPoint(const Vector3& point) PURE;

public:
	virtual HRESULT Initialize_Prototype() override;
	virtual HRESULT Initialize(void* arg) override;
	virtual void On_Destroy() override;
	virtual void On_Disable() override;
	virtual void Update(const Matrix& worldMatrix);
	/// 겹침 판정은 PhysX가 한다. 마지막 Update로 갱신된 월드 모양을 넘긴다.
	virtual void Fill_QueryShape(QUERY_SHAPE_DESC& outDesc) const PURE;

public:
	void Add_OverlapMember(const Shared<Collider>& collider) { m_OverlapMembers.insert(collider.get()); }
	void Release_OverlapMember(const Shared<Collider>& collider) { m_OverlapMembers.erase(collider.get()); }
	Bool Is_Overlapped(const Shared<Collider>& collider) const { return m_OverlapMembers.contains(collider.get()); }

	void Set_IsColliding(Bool isColliding) { m_IsColliding = isColliding; }
	Bool Get_IsColliding() const { return m_IsColliding; }

#ifdef _DEBUG
public:
	virtual HRESULT Render_Debug(const Shared<PrimitiveBatch<VertexPositionColor>>& batch, const Color& color);
#endif

protected:
	Vector3 m_Offset{};
	Bool m_IsColliding{ false };
	unordered_set<Collider*> m_OverlapMembers;

public:
	Shared<Component> Clone(void* arg = nullptr) override PURE;

};

NS_END