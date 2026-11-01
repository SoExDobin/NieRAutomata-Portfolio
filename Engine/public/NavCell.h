#pragma once
#include "Object.h"

NS_BEGIN(Engine)

class Navigation;

class ENGINE_DLL NavCell final : public Object
{
public:
	enum class CELL_POINT { A = 0, B, C, END };
	enum class CELL_LINE { AB = 0, BC, CA, END };

public:
	NavCell() = default;
	NavCell(const Vector3& a, const Vector3& b, const Vector3& c, int32 index);
	~NavCell() override = default;

public:
	HRESULT Initialize(const Shared<Navigation>& owner);
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	HRESULT Begin() override;

public:
	Bool IsIn(const Vector3& position, _Out_ int32* outNeighborIndex) const;
	/// 셀을 다른 공간으로 옮긴다. 정점을 변환하고 내/외 판별용 노멀을 다시 만든다.
	/// 인덱스와 이웃 관계는 강체 변환에 영향받지 않으므로 그대로 둔다.
	Float Compute_Height(Float pointX, Float pointZ) const;
	Bool Compare_Points(const Vector3& src, const Vector3& dst) const;

public:
	Shared<Navigation> Get_OwnNavigation() const { return m_Owner.lock(); }

	const Vector3& Get_Point(CELL_POINT point) const { return m_Points[static_cast<int32>(point)]; }
	Vector3 Get_Center() const;

	int32 Get_Index() const { return m_Index; }
	void Set_Neighbor(CELL_LINE line, int32 neighborIndex);
	int32 Get_Neighbor(CELL_LINE line) const { return m_Neighbors[static_cast<int32>(line)]; }

#ifdef _DEBUG
public:
	HRESULT Render_Debug(const Shared<PrimitiveBatch<VertexPositionColor>>& batch, const Color& color);
#endif

private:
	void Rebuild_Normals();

private:
	Weak<Navigation> m_Owner{};
	int32 m_Index{ -1 };
	Vector3 m_Points[3]{};
	Vector3 m_Normal[3]{};
	Plane m_CellPlane{};
	int32 m_Neighbors[3]{ -1, -1, -1 };

public:
	static Shared<NavCell> Create(const Vector3& a, const Vector3& b, const Vector3& c, int32 index, const Shared<Navigation>& owner);
};

NS_END
