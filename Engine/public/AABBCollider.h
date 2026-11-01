#pragma once
#include "Collider.h"

NS_BEGIN(Engine)

class ENGINE_DLL AABBCollider final : public Collider
{
public:
	typedef struct tagAABBColliderDesc : public COLLIDER_DESC
	{
		Vector3 extents = { 0.5f, 0.5f, 0.5f };
	} AABB_COLLIDER_DESC;
public:
	explicit AABBCollider();
	explicit AABBCollider(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit AABBCollider(const AABBCollider& prototype);
	~AABBCollider() override = default;

public:
	COLLIDER_TYPE Get_ColliderType() const override { return COLLIDER_TYPE::AABB; }
	COMPONENT_TYPE Get_ComponentType() const override { return COMPONENT_TYPE::AABB_COLLIDER; }
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;

public:
	void Update(const Matrix& worldMatrix) override;
	void Fill_QueryShape(QUERY_SHAPE_DESC& outDesc) const override;
	const BoundingBox& Get_Bounding() const { return m_Transformed; }
	Vector3 Get_CurrentExtends() const { return m_Transformed.Extents; }
	Vector3 ClosestPoint(const Vector3& point) override;
	Vector3 Get_Extents() const { return m_Original.Extents; }
	void Set_Extents(Vector3 extents) { m_Original.Extents = extents; }
	void Set_Offset(const Vector3& offset) override {
		m_Offset = offset;
		m_Original.Center = offset;
	}
	virtual Vector3 Get_Pivot() const override { return m_Transformed.Center; }
	

#ifdef _DEBUG
	HRESULT Render_Debug(const Shared<PrimitiveBatch<VertexPositionColor>>& batch, const Color& color) override;
#endif

private:
	BoundingBox m_Original = {};
	BoundingBox m_Transformed = {};

public:
	Shared<Component> Clone(void* arg = nullptr) override;
	static Shared<AABBCollider> CreatePrototype();
	static Shared<AABBCollider> Create(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context);
};

NS_END