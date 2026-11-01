#include "NavCell.h"

#ifdef _DEBUG
#include "DebugDraw.h"
#endif

NavCell::NavCell(const Vector3& a, const Vector3& b, const Vector3& c, int32 index)
	: m_Index{ index }
{
	m_Points[0] = a;
	m_Points[1] = b;
	m_Points[2] = c;
	m_Neighbors[0] = -1;
	m_Neighbors[1] = -1;
	m_Neighbors[2] = -1;

	Rebuild_Normals();
}
void NavCell::Rebuild_Normals()
{
	for (int32 i = 0; i < 3; ++i)
	{
		const Vector3& p0 = m_Points[i];
		const Vector3& p1 = m_Points[(i + 1) % 3];

		Vector3 lineDir = p1 - p0;
		// 시계 방향 90 도 회전을 통한 내적을 통한 안 밖 판별
		Vector3 normal = Vector3{ -lineDir.z, 0.f, lineDir.x };
		normal.Normalize();

		// p2는 항상 삼각형 안쪽에 위치한 정점
		const Vector3& p2 = m_Points[(i + 2) % 3];
		Vector3 dirToP2 = p2 - p0;
		dirToP2.y = 0.f;

		// normal 방향이 p2(안쪽)를 향하고 있다면, normal을 뒤집어 바깥쪽을 구하게 함
		if (dirToP2.Dot(normal) > 0.f)
		{
			normal = -normal;
		}

		m_Normal[i] = normal;
	}
}

HRESULT NavCell::Initialize(const Shared<Navigation>& owner)
{
	if (FAILED(Initialize_Prototype()))
	{
		return E_FAIL;
	}

	if (FAILED(Initialize(nullptr)))
	{
		return E_FAIL;
	}

	m_Owner = owner;
	if (m_Owner.expired())
		return E_FAIL;

	return S_OK;
}

HRESULT NavCell::Initialize_Prototype()
{
	return Object::Initialize_Prototype();
}

HRESULT NavCell::Initialize(void* arg)
{
	return Object::Initialize(arg);
}

HRESULT NavCell::Begin()
{
	return Object::Begin();
}

Bool NavCell::IsIn(const Vector3& position, int32* outNeighborIndex) const
{
	for (int32 i = 0; i < 3; ++i)
	{
		Vector3 dir = position - m_Points[i];
		dir.y = 0.f;

		// 내적(Dot) 검사: 밖으로 나가는 방향(m_Normals)과 내 방향(dir)이 일치하는가?
		// 내적 값(Dot)이 양수라면 해당 선분을 넘어서 바깥으로 나갔음을 의미
		// (부동 소수점 오차 보정을 위해 약간의 epsilon 여유를 둠)
		if (dir.Dot(m_Normal[i]) > 0.001f)
		{
			if (outNeighborIndex)
			{
				*outNeighborIndex = m_Neighbors[i];
			}
			return false;
		}
	}

	return true;
}

Float NavCell::Compute_Height(Float pointX, Float pointZ) const
{
	const Vector3& A = m_Points[0];
	const Vector3& B = m_Points[1];
	const Vector3& C = m_Points[2];

	// 행렬식 
	Float determine = (B.z - C.z) * (A.x - C.x) + (C.x - B.x) * (A.z - C.z);
	if (fabsf(determine) < 0.0001f) // 세점이 일직선상이거나 점이 겹치는 경우 > 오류제어
		return A.y;

	// pointx,z가 현재 삼각형 내에서 어떤 위치 비율을 가지는지 무계중심 계산
	Float u = ((B.z - C.z) * (pointX - C.x) + (C.x - B.x) * (pointZ - C.z)) / determine;
	Float v = ((C.z - A.z) * (pointX - C.x) + (A.x - C.x) * (pointZ - C.z)) / determine;

	Float w = 1.f - u - v; // 삼각 형의 가중치 합은 항상 1이 되어야 함

	// 구해진 가중치 (u, v, w) 를 각 꼭지점 원래 높이  A.y, B.y, C.y에 곱하여 더합니다.
	return u * A.y + v * B.y + w * C.y;
}

Bool NavCell::Compare_Points(const Vector3& src, const Vector3& dst) const
{
	constexpr Float epsilon = 0.01f;

	auto isEqual = [epsilon](const Vector3& a, const Vector3& b) -> Bool
		{
			return (fabsf(a.x - b.x) < epsilon &&
				fabsf(a.y - b.y) < epsilon &&
				fabsf(a.z - b.z) < epsilon
				);
		};

	// 특정 선분을 맞다은 Edge 선분을 포함하고 있는가 를 판명

	if (isEqual(m_Points[0], src) && isEqual(m_Points[1], dst)) return true;
	if (isEqual(m_Points[1], src) && isEqual(m_Points[0], dst)) return true;

	if (isEqual(m_Points[1], src) && isEqual(m_Points[2], dst)) return true;
	if (isEqual(m_Points[2], src) && isEqual(m_Points[1], dst)) return true;

	if (isEqual(m_Points[2], src) && isEqual(m_Points[0], dst)) return true;
	if (isEqual(m_Points[0], src) && isEqual(m_Points[2], dst)) return true;

	return false;
}

Vector3 NavCell::Get_Center() const
{
	return Vector3(
		(m_Points[0].x + m_Points[1].x + m_Points[2].x) / 3.f,
		(m_Points[0].y + m_Points[1].y + m_Points[2].y) / 3.f,
		(m_Points[0].z + m_Points[1].z + m_Points[2].z) / 3.f
	);
}

void NavCell::Set_Neighbor(CELL_LINE line, int32 neighborIndex)
{
	m_Neighbors[static_cast<int32>(line)] = neighborIndex;
}


#ifdef _DEBUG
HRESULT NavCell::Render_Debug(const Shared<PrimitiveBatch<VertexPositionColor>>& batch, const Color& color)
{
	DX::DrawTriangle(batch.get(), m_Points[0], m_Points[1], m_Points[2], color);

	Vector3 edge0Center = (m_Points[0] + m_Points[1]) * 0.5f;
	DX::DrawRay(batch.get(), edge0Center, m_Normal[0], true, Colors::Yellow);
	Vector3 edge1Center = (m_Points[1] + m_Points[2]) * 0.5f;
	DX::DrawRay(batch.get(), edge1Center, m_Normal[1], true, Colors::Yellow);
	Vector3 edge2Center = (m_Points[2] + m_Points[0]) * 0.5f;
	DX::DrawRay(batch.get(), edge2Center, m_Normal[2], true, Colors::Yellow);

	return S_OK;
}
#endif

Shared<NavCell> NavCell::Create(const Vector3& a, const Vector3& b, const Vector3& c, int32 index, const Shared<Navigation>& owner)
{
	auto instance = make_shared<NavCell>(a, b, c, index);

	if (FAILED(instance->Initialize(owner)))
	{
		MSG_BOX("Failed to Create : NavCell");
		return nullptr;
	}

	return instance;
}
