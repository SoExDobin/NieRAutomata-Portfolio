#include "pch.h"
#include "NavHelper.h"

#include "Game.h"
#include "PathManager.h"
#include "Model.h"
#include "GameObject.h"
#include "Transform.h"
#include "EditorManager.h"
#include "NavigationBuilder.h"
#include "String_Helper.h"
#include "Engine_RTTR_Metadata.h"
#include "SpdLogger.h"
#include "RenderTarget.h"

#include <filesystem>
#include <recastnavigation/Recast.h>

#ifdef _DEBUG
#include "DebugDraw.h"
#endif

NS_BEGIN(Editor)

NavHelper::NavHelper() : EditorObject()
{
}

NavHelper::~NavHelper()
{
}

HRESULT NavHelper::Initialize()
{
	m_Enable = false; // 초기에는 비활성

#ifdef _DEBUG
	auto pDevice = GAME_INSTANCE->Get_Device();
	auto pContext = GAME_INSTANCE->Get_Context();

	m_PreviewTarget = Engine::RenderTarget::Create(pDevice, pContext, 
		static_cast<uint32>(m_ViewportSize.x), static_cast<uint32>(m_ViewportSize.y), 
		DXGI_FORMAT_R8G8B8A8_UNORM, Vector4(0.12f, 0.12f, 0.15f, 1.f));

	D3D11_TEXTURE2D_DESC depthDesc;
	ZeroMemory(&depthDesc, sizeof(D3D11_TEXTURE2D_DESC));
	depthDesc.Width = static_cast<uint32>(m_ViewportSize.x);
	depthDesc.Height = static_cast<uint32>(m_ViewportSize.y);
	depthDesc.MipLevels = 1;
	depthDesc.ArraySize = 1;
	depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthDesc.SampleDesc.Count = 1;
	depthDesc.SampleDesc.Quality = 0;
	depthDesc.Usage = D3D11_USAGE_DEFAULT;
	depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	ComPtr<ID3D11Texture2D> depthTex;
	pDevice->CreateTexture2D(&depthDesc, nullptr, &depthTex);
	pDevice->CreateDepthStencilView(depthTex.Get(), nullptr, &m_DepthStencilView);

	D3D11_DEPTH_STENCIL_DESC dsDesc;
	ZeroMemory(&dsDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));
	dsDesc.DepthEnable = true;
	dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	dsDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	pDevice->CreateDepthStencilState(&dsDesc, &m_DepthStencilState);

	D3D11_RASTERIZER_DESC rsDesc;
	ZeroMemory(&rsDesc, sizeof(D3D11_RASTERIZER_DESC));
	rsDesc.FillMode = D3D11_FILL_SOLID;
	rsDesc.CullMode = D3D11_CULL_NONE; 
	rsDesc.DepthClipEnable = true;
	pDevice->CreateRasterizerState(&rsDesc, &m_RasterizerState);
#endif

	return EditorObject::Initialize();
}

void NavHelper::Update(Bool isResize)
{
	if (!m_Enable) return;
	EditorObject::Update(isResize);
}

void NavHelper::Render(Bool isResize)
{
	if (!m_Enable) return;

	ImGui::SetNextWindowSize(ImVec2(1200, 800), ImGuiCond_FirstUseEver);

	if (ImGui::Begin("NavMesh Builder", &m_Enable))
	{
		ImGui::Columns(2, "NavColumns", false);
		ImGui::SetColumnWidth(0, 360.0f);
		
		Render_ModelSelector();
		ImGui::Separator();
		Render_SourceSelector();
		ImGui::Separator();
		Render_ConfigPanel();
		ImGui::Separator();

		if (m_pSelectedModel)
		{
			if (ImGui::Button("Bake Preview", ImVec2(-1, 30)))
			{
				BakePreview();
			}
		}
		else
		{
			ImGui::TextDisabled("Select a model to bake.");
		}

		Render_PreviewStats();
		ImGui::Separator();
		Render_ExportPanel();
		
		ImGui::NextColumn();
		
		Render_Viewport();
		
		ImGui::Columns(1);
	}
	ImGui::End();
}

// ─────────────────────────────────────────────────────────
// 모델 선택 (Drag & Drop)
// ─────────────────────────────────────────────────────────
void NavHelper::Render_ModelSelector()
{
	ImGui::Text("Target Model");

	string sTag = Helper::To_String(m_SelectedModelTag);
	string requiredType = Engine::Asset_Type_Key::Model;

	ImVec2 slotSize = ImVec2(ImGui::GetContentRegionAvail().x - 5.f, 25.f);
	ImVec4 slotColor = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
	ImVec4 textColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);

	if (const ImGuiPayload* payload = ImGui::GetDragDropPayload())
	{
		if (string(payload->DataType) == "ASSET_BROWSER_ITEM")
		{
			string payloadStr = static_cast<const Char*>(payload->Data);
			if (payloadStr.find(requiredType + "|") == 0) {
				slotColor = ImVec4(0.1f, 0.4f, 0.1f, 1.0f);
				textColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
			}
			else {
				slotColor = ImVec4(0.4f, 0.1f, 0.1f, 1.0f);
			}
		}
	}

	ImGui::PushStyleColor(ImGuiCol_Header, slotColor);
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(slotColor.x * 1.2f, slotColor.y * 1.2f, slotColor.z * 1.2f, slotColor.w));
	ImGui::PushStyleColor(ImGuiCol_Text, textColor);
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);

	string displayText = sTag.empty() ? "(Drag " + requiredType + " here)" : sTag;
	if (ImGui::Selectable((displayText + "##NavModelTag").c_str(), true, ImGuiSelectableFlags_None, slotSize)) { }

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_BROWSER_ITEM"))
		{
			string payloadStr = static_cast<const Char*>(payload->Data);
			size_t delimPos = payloadStr.find('|');
			if (delimPos != string::npos)
			{
				string droppedType = payloadStr.substr(0, delimPos);
				string droppedTag = payloadStr.substr(delimPos + 1);

				if (requiredType == droppedType)
				{
					wstring wDroppedTag = Helper::To_wString(droppedTag);
					if (m_SelectedModelTag != wDroppedTag)
					{
						m_SelectedModelTag = wDroppedTag;
						int32 levelIdx = GAME_INSTANCE->Get_ContainLevelByModelTag(m_SelectedModelTag);
						if (levelIdx != -1)
						{
							m_pSelectedModel = GAME_INSTANCE->Get_Model(levelIdx, m_SelectedModelTag.c_str());
							m_HasPreview = false;
							if (m_AutoBake && m_pSelectedModel) BakePreview();
						}
					}
				}
			}
		}
		ImGui::EndDragDropTarget();
	}
	ImGui::PopStyleVar();
	ImGui::PopStyleColor(3);
}

// ─────────────────────────────────────────────────────────
// 선택한 GameObject의 모델을 bake 소스로
// ─────────────────────────────────────────────────────────
void NavHelper::Render_SourceSelector()
{
	if (!ImGui::Button("Use Selected Object's Model", ImVec2(-1, 24)))
		return;

	const Shared<GameObject> selected = EDITOR->Get_SelectedObject();
	const Shared<Model> model = selected ? selected->Get_Component<Model>() : nullptr;
	if (!model)
	{
		MSG_BOX("Select a GameObject with a Model in the hierarchy first.");
		return;
	}

	m_pSelectedModel = model;
	m_SelectedModelTag = model->Get_ModelTag();
	m_HasPreview = false;
	if (m_AutoBake) BakePreview();
}

// ─────────────────────────────────────────────────────────
// rcConfig 파라미터 UI
// ─────────────────────────────────────────────────────────
void NavHelper::Render_ConfigPanel()
{
	if (ImGui::CollapsingHeader("Build Parameters", ImGuiTreeNodeFlags_DefaultOpen))
	{
		bool changed = false;

		ImGui::Checkbox("Auto-Bake", &m_AutoBake);
		ImGui::Separator();

		ImGui::DragFloat("Cell Size (cs)", &m_CellSize, 0.01f, 0.05f, 2.f);
		if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
		ImGui::DragFloat("Cell Height (ch)", &m_CellHeight, 0.01f, 0.05f, 2.f);
		if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
		ImGui::DragFloat("Slope Angle", &m_SlopeAngle, 0.5f, 0.f, 85.f);
		if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
		ImGui::DragFloat("Agent Height", &m_AgentHeight, 0.1f, 0.1f, 10.f);
		if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
		ImGui::DragFloat("Agent Radius", &m_AgentRadius, 0.05f, 0.f, 5.f);
		if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
		ImGui::DragFloat("Agent Max Climb", &m_AgentMaxClimb, 0.05f, 0.f, 5.f);
		if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
		ImGui::DragInt("Max Edge Length", &m_MaxEdgeLen, 1, 1, 100);
		if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
		ImGui::DragInt("Min Region Area", &m_MinRegionArea, 1, 1, 200);
		if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
		ImGui::DragInt("Merge Region Area", &m_MergeRegionArea, 1, 1, 400);
		if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
		ImGui::DragFloat("Detail Sample Dist", &m_DetailSampleDist, 0.1f, 0.f, 64.f);
		if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
		ImGui::DragFloat("Detail Max Error", &m_DetailSampleMaxError, 0.1f, 0.f, 16.f);
		if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
		ImGui::DragFloat("Max Simplification Error", &m_MaxSimplError, 0.1f, 0.f, 8.f);
		if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

		if (changed && m_AutoBake && m_pSelectedModel) BakePreview();
	}
}

// ─────────────────────────────────────────────────────────
// 시스템 뷰포트
// ─────────────────────────────────────────────────────────
void NavHelper::Render_Viewport()
{
#ifdef _DEBUG
	ImGui::Text("Preview Viewport (Right Drag: Orbit / Middle Drag: Pan / Scroll: Zoom)");

	ImVec2 vMin = ImGui::GetWindowContentRegionMin();
	ImVec2 vMax = ImGui::GetWindowContentRegionMax();
	vMin.x += ImGui::GetWindowPos().x; vMin.y += ImGui::GetWindowPos().y;
	vMax.x += ImGui::GetWindowPos().x; vMax.y += ImGui::GetWindowPos().y;

	ImVec2 availableSpace = ImGui::GetContentRegionAvail();
	if (availableSpace.x < 100) availableSpace.x = 100;
	if (availableSpace.y < 100) availableSpace.y = 100;

	// Camera Input Handling
	if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
	{
		ImVec2 mousePos = ImGui::GetMousePos();
		bool inViewport = (mousePos.x >= vMin.x && mousePos.x <= vMax.x && 
						   mousePos.y >= vMin.y && mousePos.y <= vMax.y);

		if (inViewport || ImGui::IsMouseDragging(ImGuiMouseButton_Right) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
		{
			if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
				ImVec2 delta = ImGui::GetIO().MouseDelta;
				m_CamYaw += delta.x * 0.01f;
				m_CamPitch += delta.y * 0.01f;
				if (m_CamPitch > 1.5f) m_CamPitch = 1.5f;
				if (m_CamPitch < -1.5f) m_CamPitch = -1.5f;
			}
			if (ImGui::GetIO().MouseWheel != 0.f) {
				m_CamRadius -= ImGui::GetIO().MouseWheel * 5.0f;
				if (m_CamRadius < 1.f) m_CamRadius = 1.f;
			}
			if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
				ImVec2 delta = ImGui::GetIO().MouseDelta;
				Vector3 camPos = m_CamTarget + Vector3(
					m_CamRadius * cosf(m_CamPitch) * cosf(m_CamYaw),
					m_CamRadius * sinf(m_CamPitch),
					m_CamRadius * cosf(m_CamPitch) * sinf(m_CamYaw)
				);
				Vector3 forward = m_CamTarget - camPos; forward.Normalize();
				Vector3 right = Vector3::Up.Cross(forward); right.Normalize();
				Vector3 up = forward.Cross(right); up.Normalize();
				
				m_CamTarget -= right * delta.x * 0.05f * (m_CamRadius / 20.f);
				m_CamTarget += up * delta.y * 0.05f * (m_CamRadius / 20.f);
			}
		}
	}

	// Render Pass
	auto pContext = GAME_INSTANCE->Get_Context();

	ID3D11RenderTargetView* oldRTV = nullptr;
	ID3D11DepthStencilView* oldDSV = nullptr;
	pContext->OMGetRenderTargets(1, &oldRTV, &oldDSV);

	ID3D11RenderTargetView* rtv = m_PreviewTarget->Get_RenderTargetView().Get();
	pContext->OMSetRenderTargets(1, &rtv, m_DepthStencilView.Get());

	m_PreviewTarget->Clear_RenderTarget();
	pContext->ClearDepthStencilView(m_DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	D3D11_VIEWPORT vp;
	vp.Width = m_ViewportSize.x;
	vp.Height = m_ViewportSize.y;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	pContext->RSSetViewports(1, &vp);

	pContext->OMSetDepthStencilState(m_DepthStencilState.Get(), 0);
	pContext->RSSetState(m_RasterizerState.Get());

	Vector3 camPos = m_CamTarget + Vector3(
		m_CamRadius * cosf(m_CamPitch) * cosf(m_CamYaw),
		m_CamRadius * sinf(m_CamPitch),
		m_CamRadius * cosf(m_CamPitch) * sinf(m_CamYaw)
	);
	Matrix view = XMMatrixLookAtLH(camPos, m_CamTarget, Vector3::Up);
	Matrix proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, availableSpace.x / availableSpace.y, 0.1f, 10000.0f);

	DebugRenderer* debug = GAME_INSTANCE->Get_DebugRenderer();
	const auto& batch = debug->Begin_Lines(Matrix::Identity, view, proj);
	if (m_HasPreview && m_ShowPreviewWire)
		Render_PreviewDebug(batch);
	for (int i = -30; i <= 30; i+=2) {
		batch->DrawLine(DirectX::VertexPositionColor(Vector3((float)i, 0, -30.f), Vector4(0.3f,0.3f,0.3f,1.f)),
						DirectX::VertexPositionColor(Vector3((float)i, 0,  30.f), Vector4(0.3f,0.3f,0.3f,1.f)));
		batch->DrawLine(DirectX::VertexPositionColor(Vector3(-30.f, 0, (float)i), Vector4(0.3f,0.3f,0.3f,1.f)),
						DirectX::VertexPositionColor(Vector3( 30.f, 0, (float)i), Vector4(0.3f,0.3f,0.3f,1.f)));
	}
	debug->End_Lines();

	pContext->OMSetRenderTargets(1, &oldRTV, oldDSV);
	if (oldRTV) oldRTV->Release();
	if (oldDSV) oldDSV->Release();

	if (m_PreviewTarget->Get_ShaderResourceView()) {
		ImGui::Image(ImTextureID(static_cast<void*>(m_PreviewTarget->Get_ShaderResourceView().Get())), availableSpace);
	}
#endif
}

// ─────────────────────────────────────────────────────────
// 프리뷰 통계 표시
// ─────────────────────────────────────────────────────────
void NavHelper::Render_PreviewStats()
{
	if (!m_HasPreview) return;

	ImGui::Spacing();
	ImGui::TextColored(ImVec4(0.4f, 1.f, 0.4f, 1.f), "Preview: %d cells", static_cast<int>(m_PreviewCells.size()));

	// 이웃 통계
	int32 totalNeighbors = 0;
	int32 boundaryEdges = 0;
	for (auto &cell : m_PreviewCells)
	{
		for (int32 j = 0; j < 3; ++j)
		{
			if (cell.neighborIndices[j] >= 0)
				++totalNeighbors;
			else
				++boundaryEdges;
		}
	}
	ImGui::Text("  Neighbors: %d  |  Boundary edges: %d", totalNeighbors, boundaryEdges);

#ifdef _DEBUG
	ImGui::Checkbox("Show Wireframe##NavPreview", &m_ShowPreviewWire);
#endif
}

// ─────────────────────────────────────────────────────────
// Export 패널 (파일 이름 + Export 버튼)
// ─────────────────────────────────────────────────────────
void NavHelper::Render_ExportPanel()
{
	if (!m_HasPreview)
	{
		ImGui::TextDisabled("Bake first to export.");
		return;
	}

	ImGui::Text("Export");
	ImGui::TextDisabled("ProjectSetting/NavData/%s.nnav", Helper::To_String(m_SelectedModelTag).c_str());

	if (ImGui::Button("Export .nnav", ImVec2(-1, 30)))
	{
		SaveBinary();
	}
}

// ─────────────────────────────────────────────────────────
// rcConfig 생성
// ─────────────────────────────────────────────────────────
rcConfig NavHelper::Build_RcConfig() const
{
	rcConfig cfg{};
	memset(&cfg, 0, sizeof(cfg));

	cfg.cs = m_CellSize;
	cfg.ch = m_CellHeight;
	cfg.walkableSlopeAngle = m_SlopeAngle;
	cfg.walkableHeight = static_cast<int>(ceilf(m_AgentHeight / cfg.ch));
	cfg.walkableClimb = static_cast<int>(floorf(m_AgentMaxClimb / cfg.ch));
	cfg.walkableRadius = static_cast<int>(ceilf(m_AgentRadius / cfg.cs));
	cfg.maxEdgeLen = static_cast<int>(m_MaxEdgeLen / cfg.cs);
	cfg.minRegionArea = m_MinRegionArea * m_MinRegionArea;
	cfg.mergeRegionArea = m_MergeRegionArea * m_MergeRegionArea;
	cfg.maxVertsPerPoly = 3; // NavCell(삼각형) 구조를 위해 3 고정
	cfg.detailSampleDist = (m_DetailSampleDist < 0.9f) ? 0.f : cfg.cs * m_DetailSampleDist;
	cfg.detailSampleMaxError = m_DetailSampleMaxError;
	cfg.maxSimplificationError = m_MaxSimplError;

	return cfg;
}

// ─────────────────────────────────────────────────────────
// Bake Preview
// ─────────────────────────────────────────────────────────
void NavHelper::BakePreview()
{
	if (!m_pSelectedModel)
	{
		MSG_BOX("No model selected for NavMesh baking.");
		return;
	}

	rcConfig cfg = Build_RcConfig();

	// 모델 로컬 공간에서 굽는다. 배치는 런타임에 그 모델을 그리는 GameObject를 따른다.
	m_PreviewCells = GAME_INSTANCE->Bake_Navigation(m_pSelectedModel, cfg);
	m_HasPreview = !m_PreviewCells.empty();

	if (m_HasPreview)
	{
		LOG_INFO(L"[NavHelper] Bake Preview: {} cells", m_PreviewCells.size());
	}
	else
	{
		LOG_WARN(L"[NavHelper] Bake Preview returned 0 cells.");
	}
}

// ─────────────────────────────────────────────────────────
// Save Binary (.nnav)
// ─────────────────────────────────────────────────────────
void NavHelper::SaveBinary()
{
	if (!m_pSelectedModel || m_PreviewCells.empty())
	{
		MSG_BOX("Nothing to export. Bake first.");
		return;
	}

	// NavData 디렉토리 생성
	wstring navDataDir = PATH.GetNavDataDir();
	std::filesystem::create_directories(navDataDir);

	// 파일 이름은 모델 태그로 고정한다. Client가 같은 이름으로 Navigation prototype을 등록한다.
	const string fileName = Helper::To_String(navDataDir + m_SelectedModelTag) + ".nnav";

	if (SUCCEEDED(GAME_INSTANCE->Export_Navigation(fileName, m_pSelectedModel, Build_RcConfig())))
	{
		string msg = "NavMesh exported: " + fileName;
		LOG_INFO(L"[NavHelper] {}", Helper::To_wString(msg));
	}
	else
	{
		MSG_BOX("NavMesh export failed!");
	}
}

// ─────────────────────────────────────────────────────────
// Debug Draw
// ─────────────────────────────────────────────────────────
#ifdef _DEBUG

void NavHelper::Render_PreviewDebug(const Shared<PrimitiveBatch<DirectX::VertexPositionColor>>& batch)
{
	for (auto &cell : m_PreviewCells)
	{
		Color colorConnected = static_cast<Color>(Colors::Green);
		Color colorBoundary = static_cast<Color>(Colors::Yellow);

		for (int32 j = 0; j < 3; ++j)
		{
			const Vector3 &p0 = cell.vertices[j];
			const Vector3 &p1 = cell.vertices[(j + 1) % 3];
			Color edgeColor = (cell.neighborIndices[j] >= 0) ? colorConnected : colorBoundary;

			batch->DrawLine(
				DirectX::VertexPositionColor(p0, edgeColor),
				DirectX::VertexPositionColor(p1, edgeColor)
			);
		}
	}
}

#endif

// ─────────────────────────────────────────────────────────
// Factory
// ─────────────────────────────────────────────────────────
Shared<NavHelper> NavHelper::Create()
{
	auto instance = make_shared<NavHelper>();
	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Create NavHelper");
		return nullptr;
	}
	return instance;
}

NS_END
