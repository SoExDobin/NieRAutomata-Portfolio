#include "Navigation.h"

#include "Game.h"
#include "GameObject.h"
#include "SpdLogger.h"
#include "Transform.h"
#include "Model.h"
#include <filesystem>
#include "String_Helper.h"

#ifdef _DEBUG
Bool Navigation::s_DebugRender = false;
#endif

Navigation::Navigation() : Component{} {}
Navigation::Navigation(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context) 
	: Component{ device, context } {}
Navigation::Navigation(const Navigation& rhs)
	: Component{ rhs }
	, m_CurrentCellIndex{ -1 }      
	, m_Cells{ rhs.m_Cells }
	, m_ModelTag{ rhs.m_ModelTag } {}

HRESULT Navigation::Initialize_Prototype()
{
	return Component::Initialize_Prototype();
}

HRESULT Navigation::Initialize(void* arg)
{
	if (FAILED(Component::Initialize(arg)))
	{
		LOG_ERROR(L"Failed to Initialize Navigation");
		return E_FAIL;
	}

	auto* desc = static_cast<NAVIGATION_DESC*>(arg);
	m_CurrentCellIndex = desc ? desc->startCellIndex : 0;
	if (m_CurrentCellIndex >= 0 && m_CurrentCellIndex < static_cast<int32>(m_Cells.size()))
	{
		
	}

	return S_OK;
}

void Navigation::On_Destroy()
{
	m_Cells.clear();
	Component::On_Destroy();
}

void Navigation::Set_NavCells(vector<NavCell>&& cells)
{
	m_Cells = std::move(cells);
	LOG_INFO(L"[Navigation] NavCells 등록: {} cells", m_Cells.size());
}

HRESULT Navigation::Build_FromMesh(
	const Float* vertices, int32 numVertices,
	const int32* triangles, int32 numTriangles,
	const NavigationBuilder::NAV_BUILD_PARAMS_DESC& params)
{
	auto buildResult = GAME_INSTANCE->Build_Navigation(vertices, numVertices, triangles, numTriangles, params);
	if (!buildResult.isSuccess)
	{
		LOG_ERROR(L"[Navigation] Build_FromMesh 실패");
		return E_FAIL;
	}
	m_Cells = std::move(buildResult.navCells);
	m_CurrentCellIndex = 0;
	return S_OK;
}

HRESULT Navigation::Load_FromBinary(const string& filePath)
{
	auto cells = GAME_INSTANCE->Import_Navigation(filePath);
	if (cells.empty())
	{
		LOG_ERROR(L"[Navigation] Load_FromBinary Failed : {}",
			Helper::To_wString(filePath));
		return E_FAIL;
	}
	m_Cells = std::move(cells);
	m_CurrentCellIndex = 0;
	// 파일 이름이 곧 원본 모델 태그다.
	m_ModelTag = std::filesystem::path(Helper::To_wString(filePath)).stem().wstring();

	LOG_INFO(L"[Navigation] Loaded {} cells for model {}", m_Cells.size(), m_ModelTag);
	return S_OK;
}

HRESULT Navigation::Post_Load()
{
	if (!m_ModelTag.empty() || !m_Cells.empty())
		return S_OK;

	const Shared<GameObject> owner = Get_Owner();
	const Shared<Model> model = owner ? owner->Get_Component<Model>() : nullptr;
	if (!model || model->Get_ModelTag().empty())
		return S_OK;

	const wstring& modelTag = model->Get_ModelTag();
	for (const auto& levelComponents : GAME_INSTANCE->Get_Prototype_Components())
	{
		const auto found = levelComponents.find(modelTag);
		if (found == levelComponents.end())
			continue;
		if (const auto source = dynamic_pointer_cast<Navigation>(found->second))
		{
			m_Cells = source->m_Cells;
			m_ModelTag = source->m_ModelTag;
			return S_OK;
		}
	}

	LOG_WARN(L"[Navigation] No baked navigation for model {}; left empty", modelTag);
	return S_OK;
}

void Navigation::Refresh_Space()
{
	Shared<Transform> transform = m_SpaceTransform.lock();
	if (!transform && !m_ModelTag.empty())
	{
		// 소유자가 그 모델을 그리면 소유자, 아니면(에이전트) 현재 레벨에서 그 모델을 가진 오브젝트.
		const auto HasSourceModel = [this](const Shared<GameObject>& object) {
			const Shared<Model> model = object ? object->Get_Component<Model>() : nullptr;
			return model && model->Get_ModelTag() == m_ModelTag;
		};
		if (const Shared<GameObject> owner = Get_Owner(); HasSourceModel(owner))
			transform = owner->Get_Transform();
		else
			for (const auto& [id, object] : GAME_INSTANCE->Get_GameObjects(GAME_INSTANCE->Get_CurrentLevelIndex()))
				if (object && !object->Is_Destroy() && HasSourceModel(object)) {
					transform = object->Get_Transform();
					break;
				}
		m_SpaceTransform = transform;
	}

	const Matrix worldMatrix = transform ? transform->Get_WorldMatrix() : Matrix::Identity;
	if (worldMatrix != m_WorldMatrix)
	{
		m_WorldMatrix = worldMatrix;
		m_InvWorldMatrix = worldMatrix.Invert();
	}
}

Vector3 Navigation::To_Local(const Vector3& worldPosition) const
{
	return Vector3::Transform(worldPosition, m_InvWorldMatrix);
}

Float Navigation::To_WorldHeight(const Vector3& localPosition, Float localHeight) const
{
	return Vector3::Transform(Vector3{ localPosition.x, localHeight, localPosition.z }, m_WorldMatrix).y;
}

Bool Navigation::Has_NeighborCell(const Vector3& position)
{
	Refresh_Space();
	const Vector3 local = To_Local(position);

	if (m_CurrentCellIndex < 0 || m_CurrentCellIndex >= static_cast<int32>(m_Cells.size()))
	{
		if (m_Cells.empty() || !Compute_CurrentCellByPosition(position))
			return false;
	}

	int32 neighborIndex{ -1 };
	if (m_Cells[m_CurrentCellIndex].IsIn(local, &neighborIndex))
	{
		m_LoggedMiss = false;
		return true;
	}

	constexpr int32 maxChain = 16;
	int32 chain = 0;

	while (neighborIndex != -1 && chain++ < maxChain)
	{
		int32 nextNeighbor = -1;
		if (m_Cells[neighborIndex].IsIn(local, &nextNeighbor))
		{
			m_CurrentCellIndex = neighborIndex;
			m_LoggedMiss = false;
			return true;
		}

		neighborIndex = nextNeighbor;
	}

	return false; // 이웃을 따라가도 없음 (NavMesh 밖)
}

void Navigation::Compute_Height(const Shared<Transform>& transform)
{
	Refresh_Space();
	const Vector3 position = transform->Get_Position();

	if (m_CurrentCellIndex < 0 || m_CurrentCellIndex >= static_cast<int32>(m_Cells.size()))
	{
		if (m_Cells.empty() || !Compute_CurrentCellByPosition(position))
			return;
	}

	transform->Set_Position(position.x, Get_HeightAtPoint(position), position.z);
}

Bool Navigation::Compute_CurrentCellByPosition(const Vector3& position)
{
	Refresh_Space();
	const Vector3 local = To_Local(position);

	int32 bestIndex = -1;
	Float minHeightDiff = FLT_MAX;

	for (int32 i = 0; i < m_Cells.size(); ++i)
	{
		int32 neighborIndex = -1;
		if (m_Cells[i].IsIn(local, &neighborIndex))
		{
			Float cellHeight = m_Cells[i].Compute_Height(local.x, local.z);
			Float heightDiff = fabsf(local.y - cellHeight);

			if (heightDiff < minHeightDiff)
			{
				minHeightDiff = heightDiff;
				bestIndex = i;
			}
		}
	}

	if (bestIndex != -1)
	{
		m_CurrentCellIndex = bestIndex;
		m_LoggedMiss = false;
		return true;
	}

	// 셀 0으로 되돌리면 호출자가 성공과 구분할 수 없고, 엉뚱한 셀의 평면이 지면으로 쓰인다.
	m_CurrentCellIndex = -1;

	// 밖으로 나간 동안 프레임마다 같은 줄을 찍지 않도록 상태 전이에서만 남긴다.
	if (!m_LoggedMiss)
	{
		m_LoggedMiss = true;
		LOG_WARN(L"[Navigation] No cell contains ({:.2f}, {:.2f}, {:.2f}) among {} cells of {}",
			position.x, position.y, position.z, m_Cells.size(), m_ModelTag);
	}
	return false;
}

Float Navigation::Get_HeightAtPoint(const Vector3& position) const
{
	if (m_CurrentCellIndex < 0 || m_CurrentCellIndex >= static_cast<int32>(m_Cells.size()))
		return -FLT_MAX;

	const Vector3 local = To_Local(position);

	// 셀 평면은 삼각형 밖에서도 값을 내므로, 덮지 않는 지점까지 답하면
	// 멀리 있는 셀의 외삽값이 지면으로 쓰인다. 덮을 때만 답한다.
	int32 neighborIndex{ -1 };
	if (!m_Cells[m_CurrentCellIndex].IsIn(local, &neighborIndex))
		return -FLT_MAX;

	return To_WorldHeight(local, m_Cells[m_CurrentCellIndex].Compute_Height(local.x, local.z));
}

Shared<Navigation> Navigation::CreatePrototype()
{
	auto nav = make_shared<Navigation>(GAME_INSTANCE->Get_Device(), GAME_INSTANCE->Get_Context());

	if (FAILED(nav->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Initialize Prototype : Navigation");
		return nullptr;
	}

	return nav;
}
Shared<Navigation> Navigation::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto nav = make_shared<Navigation>(device, context);
	if (FAILED(nav->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Navigation");
		return nullptr;
	}
	return nav;
}
Shared<Component> Navigation::Clone(void* arg)
{
	auto nav = make_shared<Navigation>(*this);
	if (FAILED(nav->Initialize(arg)))
	{
		MSG_BOX("Failed to Cloned : Navigation");
		return nullptr;
	}
	return nav;
}

#ifdef _DEBUG
HRESULT Navigation::Render_Debug()
{
	if (!s_DebugRender) return S_OK;

	DebugRenderer* debug = GAME_INSTANCE->Get_DebugRenderer();
	if (!debug) return E_FAIL;

	Refresh_Space();
	const auto& batch = debug->Begin_Lines(m_WorldMatrix, GAME_INSTANCE->Get_Transform(D3DTS::VIEW), GAME_INSTANCE->Get_Transform(D3DTS::PROJ));
	for (int32 i = 0; i < m_Cells.size(); ++i)
	{
		// 플레이어가 위치한 현재 셀은 붉은색, 나머지는 초록색 라인으로 긋습니다.
		XMVECTORF32 renderColor = (i == m_CurrentCellIndex) ? Colors::Red : Colors::Green;
		m_Cells[i].Render_Debug(batch, static_cast<Color>(renderColor));
	}
	debug->End_Lines();
	return S_OK;
}
#endif
