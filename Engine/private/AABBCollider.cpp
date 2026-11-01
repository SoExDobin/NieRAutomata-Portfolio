#include "AABBCollider.h"
#include "Game.h"
#include "PhysicsManager.h"
#include "SpdLogger.h"

#ifdef _DEBUG
#include "DebugDraw.h"
#endif



AABBCollider::AABBCollider() : Collider{} {}
AABBCollider::AABBCollider(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context) 
	: Collider{ device, context } {}
AABBCollider::AABBCollider(const AABBCollider& rhs) 
	: Collider(rhs), m_Original(rhs.m_Original), m_Transformed(rhs.m_Transformed) {}

HRESULT AABBCollider::Initialize_Prototype()
{
	return Collider::Initialize_Prototype();
}
HRESULT AABBCollider::Initialize(void* arg)
{
	if (FAILED(Collider::Initialize(arg)))
	{
		LOG_ERROR(L"Failed to Initialize OBBCollider");
		return E_FAIL;
	}

	if (arg != nullptr)
	{
		auto desc= *static_cast<AABB_COLLIDER_DESC*>(arg);
		m_Original.Center = desc.offset;
		m_Original.Extents = desc.extents;
		m_Offset = desc.offset;
	}
	else
	{
		m_Original.Center = m_Offset;
		m_Original.Extents = Vector3(0.5f, 0.5f, 0.5f);
	}

	m_Transformed = m_Original;
	return S_OK;
}
void AABBCollider::Update(const Matrix& worldMatrix)
{
	Matrix transformMatrix = worldMatrix;

	// AABB는 회전을 거부해야 하므로 Right, Up, Look 벡터를 축 상에 맞춥니다.
	transformMatrix.Right(Vector3::Right * transformMatrix.Right().Length());
	transformMatrix.Up(Vector3::Up * transformMatrix.Up().Length());
	transformMatrix.Backward(Vector3::Backward * transformMatrix.Backward().Length());
	m_Original.Transform(m_Transformed, transformMatrix);
}
void AABBCollider::Fill_QueryShape(QUERY_SHAPE_DESC& outDesc) const
{
	outDesc.type = COLLIDER_TYPE::AABB;
	outDesc.center = m_Transformed.Center;
	outDesc.extents = m_Transformed.Extents;
}

Vector3 AABBCollider::ClosestPoint(const Vector3& point)
{
	Vector3 center = m_Transformed.Center;
	Vector3 extends = m_Transformed.Extents;

	Vector3 closest = point;
	closest.x = clamp(closest.x, center.x - extends.x, center.x + extends.x);
	closest.y = clamp(closest.y, center.y - extends.y, center.y + extends.y);
	closest.z = clamp(closest.z, center.z - extends.z, center.z + extends.z);

	return closest;
}
#ifdef _DEBUG
HRESULT AABBCollider::Render_Debug(const Shared<PrimitiveBatch<VertexPositionColor>>& batch, const Color& color)
{
	DX::Draw(batch.get(), m_Transformed, color);
	return S_OK;
}
#endif
Shared<Component> AABBCollider::Clone(void* arg)
{
	auto clone = make_shared<AABBCollider>(*this);
	if (FAILED(clone->Initialize(arg))) return nullptr;
	
	GAME_INSTANCE->Add_Collider(clone);

	return clone;
}
Shared<AABBCollider> AABBCollider::CreatePrototype()
{
	auto instance = make_shared<AABBCollider>();
	if (FAILED(instance->Initialize_Prototype())) return nullptr;
	return instance;
}
Shared<AABBCollider> AABBCollider::Create(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context)
{
	auto instance = make_shared<AABBCollider>(device, context);
	if (FAILED(instance->Initialize_Prototype())) return nullptr;
	return instance;
}