#include "SphereCollider.h"
#include "Game.h"
#include "PhysicsManager.h"
#include "SpdLogger.h"

#ifdef _DEBUG
#include "DebugDraw.h"
#endif


SphereCollider::SphereCollider() : Collider() {}
SphereCollider::SphereCollider(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context) 
	: Collider{device, context} {}
SphereCollider::SphereCollider(const SphereCollider& rhs) 
	: Collider(rhs), m_Original(rhs.m_Original), m_Transformed(rhs.m_Transformed) {}

HRESULT SphereCollider::Initialize_Prototype()
{
	return Collider::Initialize_Prototype();
}
HRESULT SphereCollider::Initialize(void* arg)
{
	if (FAILED(Collider::Initialize(arg)))
	{
		LOG_ERROR(L"Failed to Initialize OBBCollider");
		return E_FAIL;
	}

	if (arg != nullptr)
	{
		auto desc = *static_cast<SPHERE_COLLIDER_DESC*>(arg);
		m_Original.Center = desc.offset;
		m_Original.Radius = desc.radius;
		m_Offset = desc.offset;
	}
	else
	{
		m_Original.Center = m_Offset;
		m_Original.Radius = 1.f;
	}
	m_Transformed = m_Original;

	return S_OK;
}
void SphereCollider::Update(const Matrix& worldMatrix)
{
	m_Original.Transform(m_Transformed, worldMatrix);
}
void SphereCollider::Fill_QueryShape(QUERY_SHAPE_DESC& outDesc) const
{
	outDesc.type = COLLIDER_TYPE::SPHERE;
	outDesc.center = m_Transformed.Center;
	outDesc.radius = m_Transformed.Radius;
}

Vector3 SphereCollider::ClosestPoint(const Vector3& point)
{
	Vector3 center = m_Transformed.Center;
	Vector3 toPoint = point - center;
	Float length = toPoint.Length();

	if (length <= m_Transformed.Radius)
		return point;

	toPoint.Normalize();
	return center + toPoint * m_Transformed.Radius;
}

#ifdef _DEBUG
HRESULT SphereCollider::Render_Debug(const Shared<PrimitiveBatch<VertexPositionColor>>& batch, const Color& color)
{
	DX::Draw(batch.get(), m_Transformed, color);
	return S_OK;
}
#endif

Shared<Component> SphereCollider::Clone(void* arg)
{
	auto clone = make_shared<SphereCollider>(*this);
	if (FAILED(clone->Initialize(arg))) return nullptr;
	
	GAME_INSTANCE->Add_Collider(clone);

	return clone;
}
Shared<SphereCollider> SphereCollider::CreatePrototype()
{
	auto instance = make_shared<SphereCollider>();
	if (FAILED(instance->Initialize_Prototype())) return nullptr;
	return instance;
}
Shared<SphereCollider> SphereCollider::Create(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context)
{
	auto instance = make_shared<SphereCollider>(device, context);
	if (FAILED(instance->Initialize_Prototype())) return nullptr;
	return instance;
}
