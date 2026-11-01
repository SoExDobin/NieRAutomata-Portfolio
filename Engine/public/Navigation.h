#pragma once
#include "Component.h"
#include "NavCell.h"
#include "NavigationBuilder.h"

NS_BEGIN(Engine)

class Shader;
class Transform;
class GameObject;

class ENGINE_DLL Navigation final : public Component
{
public:
	typedef struct tagNavigationDesc : public COMPONENT_DESC
	{
		int32 startCellIndex{ 0 };
	} NAVIGATION_DESC;

public:
	explicit Navigation();
	explicit Navigation(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Navigation(const Navigation& rhs);
	~Navigation() override = default;

public:
	COMPONENT_TYPE Get_ComponentType() const override { return COMPONENT_TYPE::NAVIGATION; }
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	void On_Destroy() override;
	
public: /* build nav */
	vector<NavCell> Get_NavCells() const { return m_Cells; }
	void Set_NavCells(vector<NavCell>&& cells);
	HRESULT Build_FromMesh(
		const Float* vertices, int32 numVertices,
		const int32* triangles, int32 numTriangles,
		const NavigationBuilder::NAV_BUILD_PARAMS_DESC& params = {}
	);
	/// .nnav 바이너리 파일에서 NavCell 로드 (이웃 정보 포함, SetUp_Neighbors 불필요)
	HRESULT Load_FromBinary(const string& filePath);
public:
	Bool Has_NeighborCell(const Vector3& position);
	void Compute_Height(const Shared<Transform>& transform);
	/// 현재 셀이 그 XZ를 실제로 덮을 때만 지면 높이를 준다.
	/// 덮지 않으면 셀 평면을 외삽하지 않고 -FLT_MAX("지면 없음")를 돌려준다.
	Float Get_HeightAtPoint(const Vector3& position) const;
	/// 실패하면 현재 셀 인덱스를 -1로 두어 호출자가 실패를 구분할 수 있게 한다.
	Bool Compute_CurrentCellByPosition(const Vector3& position);

public:
	uint32 Get_NumCells() const { return static_cast<uint32>(m_Cells.size()); }
	const wstring& Get_ModelTag() const { return m_ModelTag; }
	int32 Get_CurrentCellIndex() const { return m_CurrentCellIndex; }
	void Set_CurrentCellIndex(int32 cellIndex) { m_CurrentCellIndex = cellIndex; }

private:
	int32 m_CurrentCellIndex{ -1 };
	vector<NavCell> m_Cells;
	// 밖으로 나간 상태가 이어지는 동안 경고를 한 번만 남기기 위한 표식.
	Bool m_LoggedMiss{ false };

	// 셀은 모델 로컬 공간에 있다. m_ModelTag(= .nnav 파일 이름)로 그 모델을 그리는
	// GameObject를 찾아, 질의 때 월드 좌표를 그 로컬 공간으로 바꿔 판정한다.
	wstring m_ModelTag{};
	Weak<Transform> m_SpaceTransform{};
	Matrix m_WorldMatrix{ Matrix::Identity };
	Matrix m_InvWorldMatrix{ Matrix::Identity };

	/// 태그 없이 붙었으면 소유자 ModelTag의 .nnav 프로토타입에서 셀을 가져온다. 없으면 빈 채로 둔다.
	HRESULT Post_Load() override;
	void Refresh_Space();
	Vector3 To_Local(const Vector3& worldPosition) const;
	Float To_WorldHeight(const Vector3& localPosition, Float localHeight) const;

public:
	static Shared<Navigation> CreatePrototype();
	static Shared<Navigation> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<Component> Clone(void* arg = nullptr) override;

#ifdef _DEBUG
private:
	static Bool s_DebugRender;

public:
	HRESULT Render_Debug();
	static void Toggle_DebugRender() { s_DebugRender = !s_DebugRender; }
	static Bool Get_DebugRender() { return s_DebugRender; }
#endif
};

NS_END