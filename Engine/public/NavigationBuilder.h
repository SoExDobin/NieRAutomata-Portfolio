#pragma once
#include "EngineManager.h"
#include "NavCell.h"

struct rcPolyMesh;

NS_BEGIN(Engine)

class Model;

class NavigationBuilder final : public EngineManager
{
public:
	typedef struct tagNavBuildParamsDesc
	{
		Float cellSize				{ 0.2f };		// XZ 복셀 크기 (wu)			// ↓ 작으면: 정밀도↑, 셀 수↑, 느림 / ↑ 크면: 거칠고 수 적음
		Float cellHeight			{ 0.2f };		// Y축 복셀 높이 (wu)		// ↓ 작으면: Y 정밀↑, Compute_Height 정확 / ↑ 크면: 계단화
		Float moveableSlopeAngle	{ 45.f };		// 최대 경사각 (도)			// 작으면: 급경사 NavCell 제거 / ↑ 크면: 가파른 곳도 이동 가능
		Float agentHeight			{ 2.f };		// 캐릭터 키 (wu)			// 천장 낮은 곳 NavCell 제거
		Float agentRadius			{ 0.6f };		// 캐릭터 반경 (wu)			// 벽에서 NavCell 경계가 안쪽으로 침식 (Erode)
		Float agentMaxClimb			{ 0.8f };		// 오를 수 있는 단차 (wu)		// 값↑이면 높은 턱도 NavCell 연결
		
		int32 maxEdgeLen			{ 12 };			 
		int32 minRegionArea			{ 8 };			// 최소 리전 면적				// ↑ 크면: 작은 섬, 고립된 NavCell 제거
		int32 mergeRegionArea		{ 20 };

		Float detailSampleDist		{ 6.f };		// 디테일 샘플 간격			// 가장 중요! Compute_Height의 Y 정밀도 결정. ↓ 작으면 삼각형 거칠어짐
		Float detailSampleMaxError	{ 1.f };		// 윤곽 단순화 오차			// ↑ 크면: 엣지 직선화, NavCell 수↓ / ↓ 작으면: 원본 밀착, NavCell 수↑
		Float maxSimplificationError{ 1.3f };		
	} NAV_BUILD_PARAMS_DESC;

	struct NAV_BUILD_RESULT
	{
		vector<NavCell> navCells;
		Bool isSuccess{ false };
	};

private:
	NO_COPY(NavigationBuilder)
public:
	explicit NavigationBuilder();
	~NavigationBuilder() override = default;

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	HRESULT Begin() override;

public:
	/// 모델 로컬 공간에서 굽는다. 배치 위치는 런타임에 소유 GameObject가 정한다.
	vector<NavCellBinary> Bake_Navigation(Shared<Model> model, const rcConfig& config);
	HRESULT Export_Binary(const string& fileName, Shared<Model> model, const rcConfig& config);
	vector<NavCell> Import_Binary(const string& filePath);
private:
	static vector<NavCellBinary> Bake_Internal(Shared<Model> model, rcConfig config);

public:
	NAV_BUILD_RESULT Build(
		const Float* vertices, int32 numVertices,
		const int32* triangles, int32 numTriangles,
		const NAV_BUILD_PARAMS_DESC&params = {}
	);

private:
	/// rcPolyMesh에서 NavCell 추출 + Recast 이웃 직접 대입
	static void ExtractCells_FromPolyMesh(
		const rcPolyMesh& mesh,
		const rcConfig& cfg,
		_Out_ vector<NavCell>& outCells
	);

public:
	static Unique<NavigationBuilder> Create();

};

NS_END