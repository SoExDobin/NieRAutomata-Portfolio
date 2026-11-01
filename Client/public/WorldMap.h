#pragma once
#include "GameObject.h"

NS_BEGIN(Engine)
	class Shader;
	class Model;
	class Navigation;
	class WorldCollider;
NS_END

NS_BEGIN(Client)

/// 월드 타일 하나. 어떤 타일인지는 저장된 Model의 ModelTag가 정하고,
/// 충돌(<태그>_COL)과 길찾기(<태그>.nnav)는 Post_Load에서 그 태그를 따라간다.
class CLIENT_DLL WorldMap final : public GameObject
{
public:
	explicit WorldMap();
	explicit WorldMap(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit WorldMap(const WorldMap& rhs);
	virtual ~WorldMap() override = default;

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	void On_Destroy() override;
	void On_Enable() override;
	void On_Disable() override;

public:
	void Priority_Update(Float timeDelta) override {}
	void Update(Float timeDelta) override {}
	void Late_Update(Float timeDelta) override {}
	void Fixed_Update(Float fixedDelta) override {}
	HRESULT Render() override;
	void Submit_RenderGroup() override;
	// Uses original water geometry regardless of render culling or LOD.
	enum class FOOTSTEP_WATER { NONE, SHALLOW, SEWER };
	Bool Has_Water() const;
	FOOTSTEP_WATER Get_FootstepWaterAt(const Vector3& footPosition) const;

protected:
	HRESULT Ready_Components();
	HRESULT Bind_ShaderResources();
	/// 이번 프레임에 그릴 모델을 고른다. 거리에 따라 원본 / LOD1 / LOD2다.
	const Shared<Model>& Select_Model();
	/// <ModelTag>_LOD를 잡고 그 안에 섞여 있는 LOD1·LOD2를 밴드별 인덱스로 나눈다. 최초 1회만 돈다.
	void Ready_LodBands();
	/// <ModelTag>_Props.json(타일 폴더, make_props.py 생성)의 소품을 인스턴스로 준비한다. 최초 1회만 돈다.
	void Ready_Props();

private:
	Shared<Shader> m_Shader{ nullptr };
	Shared<Model> m_Model{ nullptr };
	// <ModelTag>_LOD 프로토타입. 등록돼 있지 않으면 nullptr로 남고 LOD는 그냥 꺼진다.
	Shared<Model> m_LodModel{ nullptr };
	Bool m_LodResolved{ false };
	// 두 모델의 로컬 상자. m_LodResolved에서 한 번 구한다. m_LodBounds는 m_LodModel이 있을 때만 유효하다.
	BoundingBox m_ModelBounds{};
	BoundingBox m_LodBounds{};
	Bool m_HasModelBounds{ false };
	// 원본이나 LOD 모델에 DECAL·BLEND 재질 메시가 있으면 true. m_LodResolved에서 한 번 구하고, 그때부터 그 그룹에도 들어간다.
	Bool m_HasDecals{ false };
	Bool m_HasBlend{ false };
	/* _LOD.model 하나에 LOD1과 LOD2가 함께 들어 있고 구분은 메시 이름뿐이다. 두 레벨을 같이
	   그리면 같은 물체가 겹치므로, 해석할 때 밴드별 인덱스를 만들어 한 밴드만 그린다. */
	vector<uint32> m_LodBand0;   // 원본 모델에서 원경 링(nolowmap_far_)을 뺀 메시. 링이 없는 타일은 비워 두고 전체를 그린다.
	vector<uint32> m_LodBand1;   // LOD1 메시
	vector<uint32> m_LodBand2;   // LOD2 메시 + LOD2가 없는 그룹의 LOD1 메시
	/* 원본 모델에서 _LOD.model에 같은 물체가 없는 메시(원경 링 제외). 밴드 1·2에서도 원본에서 함께 그린다.
	   G11717은 본체 721개 중 37개만 LOD가 있어, 이것 없이는 밴드 1부터 지면·원경 집이 통째로 빠졌다. */
	vector<uint32> m_LodlessMeshes;
	// 이번 View에서 컬링을 통과한 메시. 할당을 재사용하려고 멤버로 둔다.
	vector<uint32> m_DrawMeshes;
	// renderMeshes 한 번 안의 묶음별 판정 대상 수와 판정 결과(0 = 아직, 1 = 보임, 2 = 안 보임). 할당 재사용.
	vector<uint32> m_ClusterCandidates;
	vector<uint8> m_ClusterVisibility;
	/* 원본·LOD 모델의 메시 월드 경계와 반 대각선 길이. 정적 타일이라 화면 패스와 정적 그림자 굽기가 같은 값을 쓴다.
	   매번 로컬 상자를 변환하던 메시 루프가 Debug 그림자 CPU 시간의 대부분이었다(2026-09-28 측정). */
	struct MESH_WORLD_BOUNDS
	{
		const Model* model{ nullptr };
		vector<BoundingBox> bounds;
		vector<Float> halfDiagonals;	// 경계가 없는 메시는 -1 (컬링하지 않음)
		/* 메시 중심의 XZ 범위를 MESH_CLUSTER_GRID×MESH_CLUSTER_GRID 칸으로 나눈 묶음. 묶음 상자는 칸 안 메시 상자의 합이라
		   묶음이 안 보이면 안의 메시도 안 보인다. 메시마다 판정하기 전에 묶음을 먼저 본다(2026-10-26, 판정 −42~47% 측정). */
		vector<uint32> clusterOf;			// 메시 → 칸, 경계가 없는 메시는 UINT32_MAX
		vector<BoundingBox> clusterBounds;
	};
	array<MESH_WORLD_BOUNDS, 2> m_MeshWorldBounds{};
	Matrix m_MeshBoundsWorldMatrix{};
	/// model의 메시 월드 경계. 타일 월드 행렬이 바뀔 때만 다시 만든다.
	const MESH_WORLD_BOUNDS& Get_MeshWorldBounds(const Shared<Model>& model, const Matrix& worldMatrix);
	uint32 m_LodBand{ 0 };       // 0 = 원본, 1 = LOD1, 2 = LOD2
	/* 히스테리시스는 같은 카메라로 이어질 때만 뜻이 있다. Editor는 View마다 메인 카메라를
	   바꿔 끼우므로(EditorManager.cpp:969, :982) 어느 카메라의 밴드인지 같이 들고 있어야
	   한 View가 다른 View의 밴드를 물려받지 않는다. 식별용이며 역참조하지 않는다. */
	const class Camera* m_LodBandCamera{ nullptr };
	Shared<Navigation> m_Navigation{ nullptr };
	Shared<WorldCollider> m_WorldCollider{ nullptr };

	/* 타일의 Layout 소품(가로등 등). 소품 모델마다 클론 하나가 그 소품의 타일 로컬 행렬 전부를 인스턴스로 들고
	   (Model::Set_Instances) 메시마다 draw 한 번에 그린다. 생성 데이터(_Props.json)라 씬에 저장되지 않는다.
	   그리는 셰이더는 타일 셰이더와 패스가 같은 인스턴스 맵 셰이더이고, 같은 타입 컴포넌트는 붙지 않으므로
	   _LOD처럼 멤버로 든다. 소품은 밴드 0(원본)에서만 그린다.
	   <소품>_LOD가 있으면 인스턴스마다 겉보기 크기로 원본 / LOD1 / LOD2를 골라, 메시마다 그 레벨의 인스턴스만 넣는다. */
	struct PROP_BATCH
	{
		Shared<Model> model;
		Shared<Model> lodModel;		// <소품>_LOD 클론. 없으면 늘 원본
		BoundingBox localBounds;	// 모든 인스턴스를 담는 타일 로컬 상자
		Float halfDiagonal{};		// 그 상자의 반 대각선 (그림자 최소 캐스터 판정)
		vector<INSTANCE_DATA> instances;
		vector<Vector4> spheres;	// 인스턴스마다 타일 로컬 중심(xyz)과 반지름(w)
		Float leaveLod0{};			// 겉보기 크기(반지름 / 거리)가 이보다 작으면 LOD1, leaveLod1보다 작으면 LOD2
		Float leaveLod1{};
		// 메시마다 그리는 레벨의 비트(1 = 원본, 2 = LOD1, 4 = LOD2). LOD가 없는 원본 메시는 모든 레벨에서 그린다.
		vector<uint8> modelLevels;
		vector<uint8> lodLevels;
	};
	vector<PROP_BATCH> m_Props;
	Shared<Shader> m_PropShader{ nullptr };
	/// 소품 인스턴스를 레벨별로 나눠 메시마다 올린다. full이면 모두 원본(그림자 굽기, F5 LOD 끔), cull이면 화면 밖과 가린(F6) 인스턴스를 뺀다.
	/// 같은 카메라·뷰투영·가림 깊이면 건너뛴다.
	void Update_PropLods(const Matrix& worldMatrix, Bool full, Bool cull);
	// 마지막으로 올린 판정. Render는 그룹(STATIC·DECAL·BLEND)과 View마다 불리므로 같은 View 안에서는 다시 나누지 않는다.
	const class Camera* m_PropLodCamera{ nullptr };
	Matrix m_PropLodViewProj{};
	Matrix m_PropLodOcclusion{};	// 판정에 쓴 가림 깊이의 VIEW×PROJ. 카메라가 멈추면 곧 같아져 다시 건너뛴다.
	Bool m_PropLodFull{ false };
	Bool m_PropLodCull{ false };
	Bool m_PropLodValid{ false };
	array<vector<INSTANCE_DATA>, 8> m_PropLevelLists;	// 레벨 비트 조합마다 모은 인스턴스(재사용)

public:
	static Shared<WorldMap> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;
};

NS_END
