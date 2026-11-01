#pragma once
#include "Collider.h"

NS_BEGIN(Engine)

class ENGINE_DLL OBBCollider final : public Collider
{
public:
	typedef struct tagOBBColliderDesc : public COLLIDER_DESC
	{
		Vector3 extents = { 0.5f, 0.5f, 0.5f };
		Vector3 rotation = { 0.f, 0.f, 0.f }; // 라디안
	} OBB_COLLIDER_DESC;
public:
	explicit OBBCollider();
	explicit OBBCollider(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit OBBCollider(const OBBCollider& prototype);
	virtual ~OBBCollider() override = default;
public:
	COLLIDER_TYPE Get_ColliderType() const override { return COLLIDER_TYPE::OBB; }
	COMPONENT_TYPE Get_ComponentType() const override { return COMPONENT_TYPE::OBB_COLLIDER; }
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;

public:
	void Update(const Matrix& worldMatrix) override;
	void Fill_QueryShape(QUERY_SHAPE_DESC& outDesc) const override;
	const BoundingOrientedBox& Get_Bounding() const { return m_Transformed; }
	virtual Vector3 Get_Pivot() const override { return m_Transformed.Center; }
	Vector3 Get_CurrentExtends() const { return m_Transformed.Extents; }
	Quaternion Get_CurrentOrientation() const { return m_Transformed.Orientation; }
	Vector3 ClosestPoint(const Vector3& point) override;

	Vector3 Get_Extents() const { return m_Original.Extents; }
	void Set_Extents(Vector3 extents) { m_Original.Extents = extents; }
	void Set_Offset(const Vector3& offset) override {
		m_Offset = offset;
		m_Original.Center = offset;
	}

#ifdef _DEBUG
	HRESULT Render_Debug(const Shared<PrimitiveBatch<VertexPositionColor>>& batch, const Color& color) override;
#endif

private:
	BoundingOrientedBox m_Original = {};
	BoundingOrientedBox m_Transformed = {};

public:
	Shared<Component> Clone(void* arg = nullptr) override;
	static Shared<OBBCollider> CreatePrototype();
	static Shared<OBBCollider> Create(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context);
};

NS_END
