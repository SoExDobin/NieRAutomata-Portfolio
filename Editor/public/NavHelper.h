#pragma once
#include "EditorObject.h"

NS_BEGIN(Engine)
class Model;
class GameObject;
NS_END

NS_BEGIN(Editor)

class NavHelper final : public EditorObject
{
public:
	NavHelper();
	~NavHelper() override;
	HRESULT Initialize() override;

public:
	void Update(Bool isResize) override;
	void Render(Bool isResize) override;

private:
	// ── rcConfig 파라미터 (UI 바인딩) ──
	Float	m_CellSize{ 0.4f };
	Float	m_CellHeight{ 0.15f };
	Float	m_SlopeAngle{ 45.f };
	Float	m_AgentHeight{ 2.f };
	Float	m_AgentRadius{ 0.6f };
	Float	m_AgentMaxClimb{ 0.8f };
	int32   m_MaxEdgeLen{ 12 };
	int32   m_MinRegionArea{ 8 };
	int32   m_MergeRegionArea{ 20 };
	Float	m_DetailSampleDist{ 6.f };
	Float	m_DetailSampleMaxError{ 1.f };
	Float	m_MaxSimplError{ 1.3f };

	// ── 타겟 모델 ──
	bool m_AutoBake{ true };
	wstring m_SelectedModelTag{};
	Shared<Engine::Model> m_pSelectedModel{ nullptr };

	// ── 프리뷰 데이터 ──
	vector<NavCellBinary> m_PreviewCells;
	Bool m_HasPreview{ false };

	// ── 디버그 렌더링 ──
#ifdef _DEBUG
	Bool m_ShowPreviewWire{ true };

	// ── 시스템 뷰포트 (독립 렌더링) ──
	Shared<Engine::RenderTarget> m_PreviewTarget{ nullptr };
	ComPtr<ID3D11DepthStencilView> m_DepthStencilView{ nullptr };
	ComPtr<ID3D11DepthStencilState> m_DepthStencilState{ nullptr };
	ComPtr<ID3D11RasterizerState> m_RasterizerState{ nullptr };

	// ── 프리뷰 카메라 (ArcBall) ──
	Vector3 m_CamTarget{ Vector3::Zero };
	float m_CamRadius{ 50.f };
	float m_CamPitch{ XMConvertToRadians(45.f) };
	float m_CamYaw{ XMConvertToRadians(-45.f) };
	
	// ── 시스템 창 크기 ──
	ImVec2 m_ViewportSize{ 800.f, 600.f };
#endif

	// ── 내부 함수 ──
	rcConfig Build_RcConfig() const;
	void BakePreview();
	void SaveBinary();

	void Render_ModelSelector();
	void Render_SourceSelector();
	void Render_ConfigPanel();
	void Render_Viewport();
	void Render_PreviewStats();
	void Render_ExportPanel();

#ifdef _DEBUG
	void Render_PreviewDebug(const Shared<PrimitiveBatch<DirectX::VertexPositionColor>>& batch);
#endif

public:
	static Shared<NavHelper> Create();
};

NS_END