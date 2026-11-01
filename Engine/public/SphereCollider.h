#pragma once
#include "Collider.h"

NS_BEGIN(Engine)

class ENGINE_DLL SphereCollider final : public Collider
{
public:
	typedef struct tagSphereColliderDesc : public COLLIDER_DESC
	{
		Float radius{ 1.f };
	} SPHERE_COLLIDER_DESC;
public:
	explicit SphereCollider();
	explicit SphereCollider(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit SphereCollider(const SphereCollider& prototype);
	~SphereCollider() override = default;

public:
	COLLIDER_TYPE Get_ColliderType() const override { return COLLIDER_TYPE::SPHERE; }
	COMPONENT_TYPE Get_ComponentType() const override { return COMPONENT_TYPE::SPHERE_COLLIDER; }
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;

public:
	void Update(const Matrix& worldMatrix) override;
	void Fill_QueryShape(QUERY_SHAPE_DESC& outDesc) const override;
	const BoundingSphere& Get_Bounding() const { return m_Transformed; }
	Float Get_CurrentRadius() const { return m_Transformed.Radius; }
	Vector3 Get_Pivot() const override { return m_Transformed.Center; }
	Vector3 ClosestPoint(const Vector3& point) override;

	Float Get_Radius() const { return m_Original.Radius; }
	void Set_Radius(Float radius) { m_Original.Radius = radius; }
	void Set_Offset(const Vector3& offset) override {
		m_Offset = offset;
		m_Original.Center = offset;
	}

#ifdef _DEBUG
	HRESULT Render_Debug(const Shared<PrimitiveBatch<VertexPositionColor>>& batch, const Color& color) override;
#endif

private:
	BoundingSphere m_Original = {};
	BoundingSphere m_Transformed = {};

public:
	Shared<Component> Clone(void* arg = nullptr) override;
	static Shared<SphereCollider> CreatePrototype();
	static Shared<SphereCollider> Create(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context);
};

NS_END
