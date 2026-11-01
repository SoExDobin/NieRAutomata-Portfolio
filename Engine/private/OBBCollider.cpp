#include "OBBCollider.h"
#include "Game.h"
#include "PhysicsManager.h"
#include "SpdLogger.h"

#ifdef _DEBUG
#include "DebugDraw.h"
#endif

OBBCollider::OBBCollider() : Collider{} {}
OBBCollider::OBBCollider(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: Collider{ device, context } {
}
OBBCollider::OBBCollider(const OBBCollider& rhs)
	: Collider(rhs), m_Original(rhs.m_Original), m_Transformed(rhs.m_Transformed) {
}

HRESULT OBBCollider::Initialize_Prototype()
{
	return Collider::Initialize_Prototype();
}
HRESULT OBBCollider::Initialize(void* arg)
{
	if (FAILED(Collider::Initialize(arg)))
	{
		LOG_ERROR(L"Failed to Initialize OBBCollider");
		return E_FAIL;
	}

	if (arg != nullptr)
	{
		auto desc = *static_cast<OBB_COLLIDER_DESC*>(arg);
		m_Original.Center = desc.offset;
		m_Original.Extents = desc.extents;
		m_Offset = desc.offset;

		Quaternion rotation = Quaternion::CreateFromYawPitchRoll(desc.rotation.y, desc.rotation.x, desc.rotation.z);
		m_Original.Orientation = rotation;
		m_Offset = desc.offset;
	}
	else
	{
		m_Original.Center = m_Offset;
		m_Original.Extents = Vector3(0.5f, 0.5f, 0.5f);
		m_Original.Orientation = Quaternion::Identity;
	}

	m_Transformed = m_Original;
	return S_OK;
}
void OBBCollider::Update(const Matrix& worldMatrix)
{
	m_Original.Transform(m_Transformed, worldMatrix);
}
void OBBCollider::Fill_QueryShape(QUERY_SHAPE_DESC& outDesc) const
{
	outDesc.type = COLLIDER_TYPE::OBB;
	outDesc.center = m_Transformed.Center;
	outDesc.extents = m_Transformed.Extents;
	outDesc.orientation = Quaternion{ m_Transformed.Orientation };
}

Vector3 OBBCollider::ClosestPoint(const Vector3& point)
{
	Vector3 center = m_Transformed.Center;
	Vector3 extents = m_Transformed.Extents;
	Quaternion orientation = m_Transformed.Orientation;

	Quaternion invQuat{};
	orientation.Inverse(invQuat);

	Vector3 localPoint = Vector3::Transform(point - center, invQuat);

	localPoint.x = clamp(localPoint.x, -extents.x, extents.x);
	localPoint.y = clamp(localPoint.y, -extents.y, extents.y);
	localPoint.z = clamp(localPoint.z, -extents.z, extents.z);

	return center + Vector3::Transform(localPoint, orientation);
}

#ifdef _DEBUG
HRESULT OBBCollider::Render_Debug(const Shared<PrimitiveBatch<VertexPositionColor>>& batch, const Color& color)
{
	DX::Draw(batch.get(), m_Transformed, color);
	return S_OK;
}
#endif

Shared<Component> OBBCollider::Clone(void* arg)
{
	auto clone = make_shared<OBBCollider>(*this);
	if (FAILED(clone->Initialize(arg))) return nullptr;

	GAME_INSTANCE->Add_Collider(clone);

	return clone;
}
Shared<OBBCollider> OBBCollider::CreatePrototype()
{
	auto instance = make_shared<OBBCollider>();
	if (FAILED(instance->Initialize_Prototype())) return nullptr;
	return instance;
}
Shared<OBBCollider> OBBCollider::Create(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context)
{
	auto instance = make_shared<OBBCollider>(device, context);
	if (FAILED(instance->Initialize_Prototype())) return nullptr;
	return instance;
}