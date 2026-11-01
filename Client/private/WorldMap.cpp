#include "pch.h"
#include "WorldMap.h"
#include "Shader.h"
#include "Model.h"
#include "Game.h"
#include "Navigation.h"
#include "WorldCollider.h"
#include "Camera.h"
#include <SpdLogger.h>

namespace
{
	/* 원작 ObjectParam의 DistRate를 그대로 쓴다(g11120·g11121·g11220 모두 0.70/0.35/0.10/0.03).
	   미터로 환산할 공식이 아직 없으므로 겉보기 크기 비율 s = 타일 AABB 반경 / 가장 가까운
	   면까지의 거리로 읽는다. 값이 내림차순인 것과 자연스럽게 맞는다. DistRate3(0.03)은
	   LOD3이 아니라 가시성 한계이고, 우리 타일 반경에서는 far plane 밖이라 쓰지 않는다. */
	constexpr Float LOD_RATE_LEAVE_LOD0{ 0.70f };
	constexpr Float LOD_RATE_LEAVE_LOD1{ 0.35f };
	// 경계에서 매 프레임 뒤집히지 않게 하는 폭.
	constexpr Float LOD_HYSTERESIS{ 0.10f };

	Bool Is_WaterMaterial(const MODEL_MATERIAL_SETTING& material)
	{
		return material.passName == "MAP_WaterBlend" && material.sourceShaderName.starts_with("Wtr00");
	}

	/* Layout 소품의 LOD. 소품 ObjectParam의 DistRate(bgd010 1.0/0.9/0.8/0.65, bgd137 0.7/0.2/0.1/0.03)를 타일처럼
	   겉보기 크기(반지름 / 가장 가까운 면까지 거리) 문턱으로 읽되, 소품은 반지름이 1~5 m라 그대로 쓰면 몇 m 앞에서 바뀐다.
	   원작 공식이 확인되지 않아 하나의 배율로 맞춘다: 0.05면 반지름 1 m 풀(1.0)은 20 m, 반지름 3 m 나무(0.7)는 86 m에서
	   LOD1로 내려간다(2026-10-14, 캡처로 조정·사용자 확인 대상). ObjectParam이 없는 컨테이너는 가장 흔한 값을 쓴다. */
	constexpr Float PROP_LOD_SIZE_SCALE{ 0.05f };
	constexpr Float DEFAULT_PROP_LOD_RATE0{ 0.7f };
	constexpr Float DEFAULT_PROP_LOD_RATE1{ 0.2f };
	// 타일 메시 묶음 격자의 한 변 칸 수. 2·4·8 시뮬레이션에서 8이 판정이 가장 적었다(Level 3, 2026-10-26).
	constexpr uint32 MESH_CLUSTER_GRID{ 8 };

	/// 이력 없이 겉보기 크기만으로 고르는 기본 판정.
	uint32 Band_From_Size(Float size)
	{
		if (size >= LOD_RATE_LEAVE_LOD0) return 0;
		if (size >= LOD_RATE_LEAVE_LOD1) return 1;
		return 2;
	}

	/// "LOD1_<이름>-LOD1.003"과 "g11021_build1-LOD1.002" 두 표기를 모두 받는다.
	uint32 Parse_LodLevel(const string& name)
	{
		const size_t suffix = name.rfind("-LOD");
		if (suffix != string::npos && suffix + 4 < name.size())
		{
			const Char digit = name[suffix + 4];
			if (digit >= '0' && digit <= '9')
				return static_cast<uint32>(digit - '0');
		}
		if (name.size() > 4 && name.compare(0, 3, "LOD") == 0 && name[4] == '_' &&
			name[3] >= '0' && name[3] <= '9')
			return static_cast<uint32>(name[3] - '0');
		return 0;
	}

	/// 레벨 표기와 Blender 사본 번호를 떼어 같은 물체끼리 묶을 이름을 만든다.
	string Strip_LodDecoration(const string& name)
	{
		string out = name;
		if (out.size() > 4 && out.compare(0, 3, "LOD") == 0 && out[4] == '_' &&
			out[3] >= '0' && out[3] <= '9')
			out.erase(0, 5);

		const size_t dash = out.rfind("-LOD");
		if (dash != string::npos)
		{
			size_t end = dash + 4;
			while (end < out.size() && out[end] >= '0' && out[end] <= '9')
				++end;
			out.erase(dash, end - dash);
		}

		const size_t dot = out.rfind('.');
		if (dot != string::npos && dot + 1 < out.size())
		{
			Bool allDigits = true;
			for (size_t i = dot + 1; i < out.size(); ++i)
				if (out[i] < '0' || out[i] > '9') { allDigits = false; break; }
			if (allDigits) out.erase(dot);
		}
		return out;
	}
}

WorldMap::WorldMap() : GameObject{} {}
WorldMap::WorldMap(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: GameObject{ device, context } {}
WorldMap::WorldMap(const WorldMap& rhs)
	: GameObject{ rhs } {}

HRESULT WorldMap::Initialize_Prototype()
{
	return GameObject::Initialize_Prototype();
}

HRESULT WorldMap::Initialize(void* arg)
{
	if (FAILED(GameObject::Initialize(arg)))
	{
		LOG_ERROR(L"Failed to Initialize WorldMap {}", m_ObjectName);
		return E_FAIL;
	}

	if (FAILED(Ready_Components()))
	{
		LOG_ERROR(L"Failed to Ready Components {}", m_ObjectName);
		return E_FAIL;
	}

	return S_OK;
}

void WorldMap::On_Destroy()
{
	GameObject::On_Destroy();
}

void WorldMap::On_Enable()
{
	GameObject::On_Enable();
}

void WorldMap::On_Disable()
{
	GameObject::On_Disable();
}

Bool WorldMap::Has_Water() const
{
	if (!m_Model || m_Model->Is_Skeletal()) return false;
	const auto settings = m_Model->Get_MaterialSettings();
	return settings && std::any_of(settings->materials.begin(), settings->materials.end(), Is_WaterMaterial);
}

WorldMap::FOOTSTEP_WATER WorldMap::Get_FootstepWaterAt(const Vector3& footPosition) const
{
	if (!Has_Water()) return FOOTSTEP_WATER::NONE;
	const auto settings = m_Model->Get_MaterialSettings();
	// The controller writes the owner's foot position. A dry bridge's support is above this band.
	constexpr Float shallowDepth = 0.35f;
	constexpr Float contactTolerance = 0.05f;
	const Matrix world = m_Transform->Get_WorldMatrix();
	if (fabsf(world.Determinant()) < 1e-8f) return FOOTSTEP_WATER::NONE;
	const Matrix inverse = world.Invert();
	const Vector3 start = Vector3::Transform(footPosition + Vector3::UnitY * shallowDepth, inverse);
	const Vector3 end = Vector3::Transform(footPosition - Vector3::UnitY * contactTolerance, inverse);
	for (uint32 i = 0; i < m_Model->Get_NumMeshes(); ++i)
	{
		string name;
		uint32 materialIndex{};
		if (FAILED(m_Model->Get_MeshMaterialInfo(i, name, materialIndex))) continue;
		const auto material = std::find_if(settings->materials.begin(), settings->materials.end(),
			[materialIndex](const auto& value) { return value.materialIndex == materialIndex; });
		if (material == settings->materials.end() || !Is_WaterMaterial(*material)) continue;
		Vector3 hit, normal;
		if (!m_Model->Intersect_MeshSegment(i, start, end, hit, normal)) continue;
		normal = Vector3::TransformNormal(normal, inverse.Transpose());
		normal.Normalize();
		// Wtr00 also includes falling water. Check world-space slope, not source (Z-up) normals.
		if (fabsf(normal.y) >= 0.7f)
		{
			// ManHole contains both the street mizu surface and the underground water material.
			const Bool sewer = m_Model->Get_ModelTag() == L"CityOfRuinManHole" &&
				material->sourceMaterialName == "pre_lightmap_tga3_";
			return sewer ? FOOTSTEP_WATER::SEWER : FOOTSTEP_WATER::SHALLOW;
		}
	}
	return FOOTSTEP_WATER::NONE;
}

HRESULT WorldMap::Render()
{
	/* STATIC 그룹(와 그림자 패스)은 불투명 메시만, DECAL·BLEND 그룹은 그 그룹 재질의 메시만 그린다.
	   데칼과 반투명(물)은 그림자를 드리우지 않는다. */
	const RENDERGROUP drawing = GAME_INSTANCE->Get_DrawingGroup();
	const RENDERGROUP meshGroup = (RENDERGROUP::DECAL == drawing || RENDERGROUP::BLEND == drawing) ? drawing : RENDERGROUP::NONBLEND;
	const Bool opaquePass = RENDERGROUP::NONBLEND == meshGroup;
#ifdef _DEBUG
	if (m_Navigation && !GAME_INSTANCE->Is_ShadowPass() && opaquePass)
		m_Navigation->Render_Debug();
#endif

	if (FAILED(Bind_ShaderResources()))
		return E_FAIL;

	/* 그림자 패스에서 타일은 레벨 정적 그림자를 구울 때만 그려진다(Renderer::Bake_StaticShadow). 레벨 전체를 한 번
	   굽는 것이라 카메라 거리로 고른 LOD가 아니라 원본(밴드 0)으로 드리우고, 카메라 밴드 판정도 건드리지 않는다. */
	const Bool shadowPass = GAME_INSTANCE->Is_ShadowPass();
	if (shadowPass && !m_LodResolved)
		Select_Model();
	const Shared<Model>& model = shadowPass ? m_Model : Select_Model();
	const uint32 band = shadowPass ? 0 : m_LodBand;
	/* 밴드0은 원본 모델 전체(원경 링 제외), 밴드1·2는 _LOD.model에서 그 레벨에 속한 메시와 LOD 없는 원본 메시를 그린다.
	   참조 대신 포인터를 쓰는 이유는 nullptr로 "전체"를 표현하면서 벡터 복사를 피하기 위해서다. */
	const vector<uint32>* meshOrder = nullptr;
	if (band == 0 && !m_LodBand0.empty()) meshOrder = &m_LodBand0;
	else if (band == 1) meshOrder = &m_LodBand1;
	else if (band == 2) meshOrder = &m_LodBand2;

	// 밴드 1·2에서는 LOD가 없는 원본 메시를 원본 모델에서 함께 그린다.
	const Bool drawLodless = 0 != band && !m_LodlessMeshes.empty();
	const uint32 numMeshes = meshOrder ?
		static_cast<uint32>(meshOrder->size()) : static_cast<uint32>(model->Get_NumMeshes());
	const Bool culling = GAME_INSTANCE->Get_FrustumCulling();
	const Matrix worldMatrix = m_Transform->Get_WorldMatrix();
	// 그림자 패스: 반 대각선이 그 캐스케이드 텍셀 SHADOW_MIN_CASTER_TEXELS개보다 작은 메시는 그림자 맵에 거의 남지 않아 건너뛴다.
	const Float minCasterExtent = shadowPass ? SHADOW_MIN_CASTER_TEXELS * GAME_INSTANCE->Get_ShadowTexelSize() : 0.f;

	/* 타일이 통째로 화면 밖이면 메시를 하나도 보지 않는다. LOD 없는 원본 메시는 LOD 상자 밖에 있을 수 있으므로
	   그것을 같이 그릴 때는 원본 상자로 잰다. */
	const BoundingBox* tileBounds = (0 != band && !drawLodless) ? &m_LodBounds : m_HasModelBounds ? &m_ModelBounds : nullptr;
	Bool tileVisible = true;
	if (culling && nullptr != tileBounds)
	{
		BoundingBox tileWorld{};
		tileBounds->Transform(tileWorld, worldMatrix);
		if (!GAME_INSTANCE->Is_Visible(tileWorld))
		{
			// 같은 타일이 DECAL·BLEND 그룹에서 한 번 더 오므로 컬링 수는 불투명 쪽에서만 센다.
			const size_t numCulled = numMeshes + (drawLodless ? m_LodlessMeshes.size() : 0);
			for (size_t i = 0; opaquePass && i < numCulled; ++i)
				GAME_INSTANCE->Add_CulledMesh();
			tileVisible = false;
		}
	}

	const auto renderMeshes = [&](const Shared<Model>& drawModel, const vector<uint32>* order, uint32 count) -> HRESULT
	{
		m_DrawMeshes.clear();
		const MESH_WORLD_BOUNDS& meshBounds = Get_MeshWorldBounds(drawModel, worldMatrix);
		/* 판정할 메시가 둘 이상인 묶음만 묶음 상자를 먼저 판정한다(처음 만날 때 한 번). 하나뿐인 묶음은 메시만 본다.
		   그리는 순서는 order 그대로 둔다. */
		const auto isJudged = [&](uint32 i) {
			return drawModel->Get_MeshRenderGroup(i) == meshGroup && meshBounds.halfDiagonals[i] >= minCasterExtent &&
				meshBounds.halfDiagonals[i] >= 0.f;
		};
		if (culling)
		{
			m_ClusterCandidates.assign(meshBounds.clusterBounds.size(), 0);
			m_ClusterVisibility.assign(meshBounds.clusterBounds.size(), 0);
			for (uint32 k = 0; k < count; ++k)
			{
				const uint32 i = order ? (*order)[k] : k;
				if (isJudged(i))
					++m_ClusterCandidates[meshBounds.clusterOf[i]];
			}
		}
		const auto clusterHidden = [&](uint32 i) {
			const uint32 cluster = meshBounds.clusterOf[i];
			if (m_ClusterCandidates[cluster] < 2)
				return false;
			if (0 == m_ClusterVisibility[cluster])
				m_ClusterVisibility[cluster] = GAME_INSTANCE->Is_Visible(meshBounds.clusterBounds[cluster]) ? 1 : 2;
			return 2 == m_ClusterVisibility[cluster];
		};
		for (uint32 k = 0; k < count; ++k)
		{
			const uint32 i = order ? (*order)[k] : k;
			if (drawModel->Get_MeshRenderGroup(i) != meshGroup)
				continue;

			// 경계가 없는 메시(halfDiagonal < 0)는 컬링하지 않는다.
			if ((culling || minCasterExtent > 0.f) && meshBounds.halfDiagonals[i] >= 0.f &&
				(meshBounds.halfDiagonals[i] < minCasterExtent ||
					(culling && (clusterHidden(i) || !GAME_INSTANCE->Is_Visible(meshBounds.bounds[i])))))
			{
				GAME_INSTANCE->Add_CulledMesh();
				continue;
			}

			m_DrawMeshes.push_back(i);
		}

		/* 반투명(물)은 G-buffer를 거치지 않으므로 방향광·그림자·장면 깊이를 직접 받고, 겹쳐도 맞게 섞이도록
		   카메라에서 먼 메시부터 그린다. */
		if (RENDERGROUP::BLEND == meshGroup && !m_DrawMeshes.empty())
		{
			if (FAILED(GAME_INSTANCE->Bind_ForwardLighting(m_Shader)))
				return E_FAIL;
			if (const auto camera = GAME_INSTANCE->Get_MainCamera(); camera && camera->Get_Transform())
			{
				const Vector3 eye = camera->Get_Transform()->Get_Position();
				std::ranges::sort(m_DrawMeshes, [&](uint32 lhs, uint32 rhs) {
					return Vector3::DistanceSquared(meshBounds.bounds[lhs].Center, eye) >
						Vector3::DistanceSquared(meshBounds.bounds[rhs].Center, eye);
				});
			}
		}

		// 같은 재질이 이어지는 메시의 재바인딩 생략은 Model이 한다.
		return drawModel->Render_Meshes(m_Shader, m_DrawMeshes);
	};

	if (tileVisible)
	{
		if (FAILED(renderMeshes(model, meshOrder, numMeshes)))
			return E_FAIL;
		if (drawLodless && FAILED(renderMeshes(m_Model, &m_LodlessMeshes, static_cast<uint32>(m_LodlessMeshes.size()))))
			return E_FAIL;
	}

	/* Layout 소품: 타일과 같은 그룹·그림자 규칙으로 소품 모델마다 인스턴스 draw. 컬링은 소품 배치 상자 단위이고,
	   그림자 패스에서는 그 상자의 반 대각선으로 최소 캐스터를 판정한다. 타일이 LOD 밴드(1·2)로 그려지는 View에서는
	   소품을 그리지 않는다(사용자 결정 2026-10-13). 정적 그림자 굽기는 밴드 0이라 소품도 드리운다. */
	if (m_Props.empty() || 0 != band)
		return S_OK;
	// 그림자 굽기와 F5(LOD) 끔은 모든 인스턴스를 원본으로 그린다.
	const Bool propFullDetail = shadowPass || !GAME_INSTANCE->Get_WorldLod();
	// 인스턴스 컬링은 화면 패스에서만, 프러스텀 컬링 토글을 따른다.
	Update_PropLods(worldMatrix, propFullDetail, culling && !shadowPass);
	if (FAILED(m_Transform->Bind_ShaderResource(m_PropShader, WorldMatrix)) ||
		FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_PropShader, ViewMatrix, D3DTS::VIEW)) ||
		FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_PropShader, ProjMatrix, D3DTS::PROJ)))
		return E_FAIL;
	Bool forwardBound = false;
	for (const PROP_BATCH& prop : m_Props)
	{
		BoundingBox propWorld{};
		prop.localBounds.Transform(propWorld, worldMatrix);
		if (prop.halfDiagonal < minCasterExtent || (culling && !GAME_INSTANCE->Is_Visible(propWorld)))
			continue;
		// 원본 모델과(LOD를 쓰는 View면) LOD 모델. 각 메시에는 Update_PropLods가 그 레벨의 인스턴스만 넣어 두었다.
		for (const auto& [drawModel, levels] : { std::pair{ prop.model, &prop.modelLevels },
			std::pair{ propFullDetail ? nullptr : prop.lodModel, &prop.lodLevels } })
		{
			if (!drawModel)
				continue;
			m_DrawMeshes.clear();
			for (uint32 i = 0; i < drawModel->Get_NumMeshes(); ++i)
				if (0 != (*levels)[i] && drawModel->Get_MeshRenderGroup(i) == meshGroup)
					m_DrawMeshes.push_back(i);
			if (m_DrawMeshes.empty())
				continue;
			if (RENDERGROUP::BLEND == meshGroup && !forwardBound)
			{
				if (FAILED(GAME_INSTANCE->Bind_ForwardLighting(m_PropShader)))
					return E_FAIL;
				forwardBound = true;
			}
			if (FAILED(drawModel->Render_Meshes(m_PropShader, m_DrawMeshes)))
				return E_FAIL;
		}
	}
	return S_OK;
}

void WorldMap::Update_PropLods(const Matrix& worldMatrix, Bool full, Bool cull)
{
	const auto camera = GAME_INSTANCE->Get_MainCamera();
	const Vector3 eye = camera && camera->Get_Transform() ? camera->Get_Transform()->Get_Position() : Vector3::Zero;
	// 컬링은 카메라가 제자리에서 돌기만 해도 바뀌므로 위치가 아니라 뷰·투영 행렬로 같은 판정인지 본다.
	const Matrix proj = GAME_INSTANCE->Get_Transform(D3DTS::PROJ);
	const Matrix viewProj = GAME_INSTANCE->Get_Transform(D3DTS::VIEW) * proj;
	// 인스턴스 컬링은 원근 View에서만 한다(직교 View는 소품 묶음 상자 컬링만).
	cull = cull && 0.f == proj._44;
	const Matrix occlusion = cull ? GAME_INSTANCE->Get_OcclusionViewProj() : Matrix{};
	if (m_PropLodValid && m_PropLodFull == full && m_PropLodCull == cull &&
		((full && !cull) || (m_PropLodCamera == camera.get() && m_PropLodViewProj == viewProj && m_PropLodOcclusion == occlusion)))
		return;
	m_PropLodValid = true;
	m_PropLodFull = full;
	m_PropLodCull = cull;
	m_PropLodCamera = camera.get();
	m_PropLodViewProj = viewProj;
	m_PropLodOcclusion = occlusion;

	// 타일은 이동·회전만 하므로 카메라를 타일 로컬로 옮겨 재면 거리가 같다.
	const Matrix worldInverse = worldMatrix.Invert();
	const Vector3 localEye = Vector3::Transform(eye, worldInverse);
	// 인스턴스 컬링용 절두체. 타일 로컬로 한 번 옮겨 두면 인스턴스 구를 옮기지 않고 바로 잰다.
	BoundingFrustum localFrustum{};
	if (cull)
	{
		BoundingFrustum::CreateFromMatrix(localFrustum, proj);
		localFrustum.Transform(localFrustum, GAME_INSTANCE->Get_InvTransform(D3DTS::VIEW) * worldInverse);
	}
	for (PROP_BATCH& prop : m_Props)
	{
		// 비트 1·2·4 = 원본·LOD1·LOD2에 든 인스턴스. 메시는 자기 비트 조합(0b001, 0b111 ...)의 합을 받는다.
		for (auto& list : m_PropLevelLists)
			list.clear();
		const Bool useLod = !full && nullptr != prop.lodModel;
		// 묶음 상자가 통째로 밖이면 인스턴스를 보지 않고, 통째로 안이면 하나씩 재지 않는다.
		const ContainmentType batchHit = cull ? localFrustum.Contains(prop.localBounds) : CONTAINS;
		const Bool testEach = INTERSECTS == batchHit;
		for (size_t i = 0; DISJOINT != batchHit && i < prop.instances.size(); ++i)
		{
			const Vector4& sphere = prop.spheres[i];
			if (testEach && DISJOINT == localFrustum.Contains(BoundingSphere{ Vector3{ sphere.x, sphere.y, sphere.z }, sphere.w }))
				continue;
			// 가림은 월드 상자로 잰다. 타일은 이동·회전만 하므로 구의 월드 중심에 반지름 상자를 씌운다.
			if (cull && GAME_INSTANCE->Is_Occluded(BoundingBox{
				Vector3::Transform(Vector3{ sphere.x, sphere.y, sphere.z }, worldMatrix), Vector3{ sphere.w, sphere.w, sphere.w } }))
				continue;
			uint32 level = 0;
			if (useLod)
			{
				const Float distance = Vector3::Distance(localEye, Vector3{ sphere.x, sphere.y, sphere.z }) - sphere.w;
				const Float size = distance > 0.f ? sphere.w / distance : FLT_MAX;
				level = size >= prop.leaveLod0 ? 0 : size >= prop.leaveLod1 ? 1 : 2;
			}
			m_PropLevelLists[size_t{ 1 } << level].push_back(prop.instances[i]);
		}
		const auto upload = [this](const Shared<Model>& model, const vector<uint8>& levels)
		{
			for (uint32 i = 0; model && i < model->Get_NumMeshes(); ++i)
			{
				const uint8 bits = levels[i];
				if (0 == bits)
					continue;
				// 한 레벨이면 그 목록을, 여러 레벨이면 합친 목록을 이번 판정 동안 한 번만 만든다.
				vector<INSTANCE_DATA>& merged = m_PropLevelLists[bits];
				if (bits != 0b001 && bits != 0b010 && bits != 0b100 && merged.empty())
					for (uint8 level : { 0b001, 0b010, 0b100 })
						if (bits & level)
							merged.insert(merged.end(), m_PropLevelLists[level].begin(), m_PropLevelLists[level].end());
				model->Set_Instances(i, merged);
			}
		};
		// LOD를 안 쓰는 판정(full)에서는 LOD 모델을 그리지 않으므로 그 버퍼는 그대로 둔다.
		upload(prop.model, prop.modelLevels);
		if (useLod)
			upload(prop.lodModel, prop.lodLevels);
	}
}

const WorldMap::MESH_WORLD_BOUNDS& WorldMap::Get_MeshWorldBounds(const Shared<Model>& model, const Matrix& worldMatrix)
{
	// 타일을 옮기면(Editor 기즈모 등) 두 모델의 캐시를 모두 버린다.
	if (worldMatrix != m_MeshBoundsWorldMatrix)
	{
		for (MESH_WORLD_BOUNDS& cache : m_MeshWorldBounds)
			cache.model = nullptr;
		m_MeshBoundsWorldMatrix = worldMatrix;
	}

	MESH_WORLD_BOUNDS* slot = nullptr;
	for (MESH_WORLD_BOUNDS& cache : m_MeshWorldBounds)
	{
		if (cache.model == model.get())
			return cache;
		if (nullptr == slot && nullptr == cache.model)
			slot = &cache;
	}
	if (nullptr == slot)
		slot = &m_MeshWorldBounds[0];

	const uint32 numMeshes = model->Get_NumMeshes();
	slot->model = model.get();
	slot->bounds.assign(numMeshes, BoundingBox{});
	slot->halfDiagonals.assign(numMeshes, -1.f);
	for (uint32 i = 0; i < numMeshes; ++i)
	{
		BoundingBox local{};
		if (!model->Get_MeshLocalBounds(i, local))
			continue;
		local.Transform(slot->bounds[i], worldMatrix);
		slot->halfDiagonals[i] = Vector3{ slot->bounds[i].Extents }.Length();
	}

	// 메시 묶음: 경계가 있는 메시 중심의 XZ 범위를 격자로 나누고, 칸마다 안 메시 상자를 합친다.
	Vector3 low{ FLT_MAX, FLT_MAX, FLT_MAX }, high{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
	for (uint32 i = 0; i < numMeshes; ++i)
		if (slot->halfDiagonals[i] >= 0.f)
		{
			low = Vector3::Min(low, slot->bounds[i].Center);
			high = Vector3::Max(high, slot->bounds[i].Center);
		}
	const auto cell = [](Float value, Float from, Float to) {
		return to > from ? (std::min)(MESH_CLUSTER_GRID - 1, static_cast<uint32>((value - from) / (to - from) * MESH_CLUSTER_GRID)) : 0u;
	};
	slot->clusterOf.assign(numMeshes, UINT32_MAX);
	slot->clusterBounds.assign(MESH_CLUSTER_GRID * MESH_CLUSTER_GRID, BoundingBox{});
	vector<Bool> filled(slot->clusterBounds.size(), false);
	for (uint32 i = 0; i < numMeshes; ++i)
	{
		if (slot->halfDiagonals[i] < 0.f)
			continue;
		const Vector3 center = slot->bounds[i].Center;
		const uint32 cluster = cell(center.x, low.x, high.x) * MESH_CLUSTER_GRID + cell(center.z, low.z, high.z);
		slot->clusterOf[i] = cluster;
		if (filled[cluster])
			BoundingBox::CreateMerged(slot->clusterBounds[cluster], slot->clusterBounds[cluster], slot->bounds[i]);
		else
			slot->clusterBounds[cluster] = slot->bounds[i];
		filled[cluster] = true;
	}
	return *slot;
}

void WorldMap::Ready_LodBands()
{
	m_LodBand0.clear();
	m_LodBand1.clear();
	m_LodBand2.clear();
	m_LodlessMeshes.clear();

	/* G11816의 nolowmap_far_01은 놀이공원 전체를 덮는 전경 링이라 우리가 따로 로드한 이웃 타일 위에
	   저해상도 사본이 겹친다. 이웃 타일을 실제로 그리므로 링은 그리지 않는다. */
	if (m_Model)
	{
		const uint32 numModelMeshes = m_Model->Get_NumMeshes();
		for (uint32 i = 0; i < numModelMeshes; ++i)
		{
			string meshName{};
			uint32 materialIndex{};
			if (SUCCEEDED(m_Model->Get_MeshMaterialInfo(i, meshName, materialIndex)) &&
				meshName.starts_with("nolowmap_far_"))
				continue;
			m_LodBand0.push_back(i);
		}
		if (m_LodBand0.size() == numModelMeshes)
			m_LodBand0.clear();
	}

	if (nullptr == m_LodModel)
		return;

	const uint32 numMeshes = m_LodModel->Get_NumMeshes();
	vector<uint32> levels(numMeshes, 0);
	vector<string> groups(numMeshes);
	std::set<string> hasLod2;

	for (uint32 i = 0; i < numMeshes; ++i)
	{
		string meshName{};
		uint32 materialIndex{};
		if (FAILED(m_LodModel->Get_MeshMaterialInfo(i, meshName, materialIndex)))
			continue;

		levels[i] = Parse_LodLevel(meshName);
		groups[i] = Strip_LodDecoration(meshName);
		if (levels[i] == 2)
			hasLod2.insert(groups[i]);
	}

	for (uint32 i = 0; i < numMeshes; ++i)
	{
		if (levels[i] == 2)
		{
			m_LodBand2.push_back(i);
		}
		else if (levels[i] == 1)
		{
			m_LodBand1.push_back(i);
			// 더 거친 판이 없는 물체는 먼 밴드에서도 LOD1을 그대로 쓴다.
			if (!hasLod2.contains(groups[i]))
				m_LodBand2.push_back(i);
		}
		else
		{
			// 레벨 표기가 없는 메시는 어느 밴드에서도 빠지지 않게 둘 다에 넣는다.
			m_LodBand1.push_back(i);
			m_LodBand2.push_back(i);
		}
	}

	/* 원작은 물체마다 LOD를 바꾸므로 LOD가 없는 물체는 멀어져도 원본으로 남는다. 같은 물체 판정은 위의
	   그룹 이름을 쓴다(LOD2만 있는 그룹은 현재 타일에 없어 밴드를 나누지 않는다). 원경 링은 밴드0과 같이 뺀다. */
	if (nullptr == m_Model)
		return;
	const std::set<string> lodGroups(groups.begin(), groups.end());
	const uint32 numCandidates = m_LodBand0.empty() ? m_Model->Get_NumMeshes() : static_cast<uint32>(m_LodBand0.size());
	for (uint32 k = 0; k < numCandidates; ++k)
	{
		const uint32 i = m_LodBand0.empty() ? k : m_LodBand0[k];
		string meshName{};
		uint32 materialIndex{};
		// 재질별로 합친 메시(merge_tile_meshes.py)는 원래 이름이 없어 짝을 못 찾으므로 이름에 찍힌 판정(_lodless_)을 쓴다.
		if (SUCCEEDED(m_Model->Get_MeshMaterialInfo(i, meshName, materialIndex)) &&
			(meshName.starts_with("merged_") ? string::npos != meshName.find("_lodless_")
				: !lodGroups.contains(Strip_LodDecoration(meshName))))
			m_LodlessMeshes.push_back(i);
	}
}

void WorldMap::Ready_Props()
{
	m_Props.clear();
	if (!m_Model || m_Model->Get_ModelTag().empty())
		return;
	const filesystem::path path = filesystem::path(m_Model->Get_ModelFilePath()).parent_path() /
		(m_Model->Get_ModelTag() + L"_Props.json");
	if (!filesystem::exists(path))
		return;
	nlohmann::json document;
	try
	{
		ifstream file(path);
		file >> document;
	}
	catch (const std::exception&)
	{
		LOG_WARN(L"[WorldMap] Unreadable prop list {}", path.wstring());
		return;
	}

	m_PropShader = GAME_INSTANCE->Get_Shader(ETOI(LEVEL::STATIC), MapInstanceShaderTag);
	if (!m_PropShader)
		return;
	for (const auto& entry : document.value("props", nlohmann::json::array()))
	{
		const wstring tag = Helper::To_wString(entry.value("model", string{}));
		const int32 levIndex = GAME_INSTANCE->Get_ContainLevelByModelTag(tag);
		const Shared<Model> prototype = levIndex != -1 ? GAME_INSTANCE->Get_Model(static_cast<uint32>(levIndex), tag.c_str()) : nullptr;
		Model::MODEL_DESC desc{ tag };
		const Shared<Model> model = prototype ? static_pointer_cast<Model>(prototype->Clone(&desc)) : nullptr;
		BoundingBox modelBounds{};
		if (!model || FAILED(model->Validate_MaterialBindings(m_PropShader)) || !model->Compute_LocalBounds(modelBounds))
		{
			LOG_WARN(L"[WorldMap] {} prop {} is not loaded or fails the map materials; skipped", m_Model->Get_ModelTag(), tag);
			continue;
		}

		PROP_BATCH batch{ model };
		batch.modelLevels.assign(model->Get_NumMeshes(), 0b001);

		/* <소품>_LOD: 타일 LOD와 같은 이름 규칙(LOD1_<이름>-LOD1)으로 메시마다 레벨을 정한다. LOD2가 없는 물체의 LOD1은
		   LOD2 거리에서도 쓰고, LOD3 이상은 쓰지 않는다. LOD가 없는 원본 메시는 모든 레벨에서 원본으로 그린다. */
		const wstring lodTag = tag + L"_LOD";
		const int32 lodLevel = GAME_INSTANCE->Get_ContainLevelByModelTag(lodTag);
		const Shared<Model> lodPrototype = lodLevel != -1 ? GAME_INSTANCE->Get_Model(static_cast<uint32>(lodLevel), lodTag.c_str()) : nullptr;
		Model::MODEL_DESC lodDesc{ lodTag };
		Shared<Model> lodModel = lodPrototype ? static_pointer_cast<Model>(lodPrototype->Clone(&lodDesc)) : nullptr;
		BoundingBox lodBounds{};
		if (lodModel && (FAILED(lodModel->Validate_MaterialBindings(m_PropShader)) || !lodModel->Compute_LocalBounds(lodBounds)))
		{
			LOG_WARN(L"[WorldMap] {} prop LOD {} fails the map materials; LOD off", m_Model->Get_ModelTag(), lodTag);
			lodModel = nullptr;
		}
		if (lodModel)
		{
			const uint32 numLodMeshes = lodModel->Get_NumMeshes();
			vector<uint32> levels(numLodMeshes, 0);
			vector<string> groups(numLodMeshes);
			std::set<string> hasLod2;
			for (uint32 i = 0; i < numLodMeshes; ++i)
			{
				string meshName{};
				uint32 materialIndex{};
				if (FAILED(lodModel->Get_MeshMaterialInfo(i, meshName, materialIndex)))
					continue;
				levels[i] = Parse_LodLevel(meshName);
				groups[i] = Strip_LodDecoration(meshName);
				if (2 == levels[i])
					hasLod2.insert(groups[i]);
			}
			batch.lodLevels.assign(numLodMeshes, 0);
			for (uint32 i = 0; i < numLodMeshes; ++i)
			{
				if (1 == levels[i]) batch.lodLevels[i] = hasLod2.contains(groups[i]) ? 0b010 : 0b110;
				else if (2 == levels[i]) batch.lodLevels[i] = 0b100;
			}
			const std::set<string> lodGroups(groups.begin(), groups.end());
			for (uint32 i = 0; i < model->Get_NumMeshes(); ++i)
			{
				string meshName{};
				uint32 materialIndex{};
				if (SUCCEEDED(model->Get_MeshMaterialInfo(i, meshName, materialIndex)) &&
					!lodGroups.contains(Strip_LodDecoration(meshName)))
					batch.modelLevels[i] = 0b111;
			}
			BoundingBox::CreateMerged(modelBounds, modelBounds, lodBounds);
			batch.lodModel = lodModel;
		}

		// 원작 ObjectParam DistRate0·1(make_props.py가 옮김)에 PROP_LOD_SIZE_SCALE을 곱한 겉보기 크기에서 레벨을 내린다.
		const auto rates = entry.value("lodRates", vector<Float>{});
		batch.leaveLod0 = (rates.size() > 0 ? rates[0] : DEFAULT_PROP_LOD_RATE0) * PROP_LOD_SIZE_SCALE;
		batch.leaveLod1 = (rates.size() > 1 ? rates[1] : DEFAULT_PROP_LOD_RATE1) * PROP_LOD_SIZE_SCALE;
		// 행 우선 16개 = 엔진(행 벡터) 행렬. 상자는 인스턴스마다 옮긴 모델 상자를 합친다.
		const Float modelRadius = Vector3{ modelBounds.Extents }.Length();
		for (const auto& values : entry.value("matrices", nlohmann::json::array()))
		{
			if (!values.is_array() || values.size() != 16)
				continue;
			INSTANCE_DATA instance{};
			for (uint32 i = 0; i < 16; ++i)
				instance.world.m[i / 4][i % 4] = values[i].get<Float>();
			BoundingBox placed{};
			modelBounds.Transform(placed, instance.world);
			if (batch.instances.empty())
				batch.localBounds = placed;
			else
				BoundingBox::CreateMerged(batch.localBounds, batch.localBounds, placed);
			const Float scale = std::max({ instance.world.Right().Length(), instance.world.Up().Length(), instance.world.Backward().Length() });
			batch.spheres.emplace_back(placed.Center.x, placed.Center.y, placed.Center.z, modelRadius * scale);
			batch.instances.push_back(instance);
		}
		if (batch.instances.empty())
			continue;

		Bool ready = true;
		for (uint32 i = 0; i < model->Get_NumMeshes(); ++i)
			ready = ready && SUCCEEDED(model->Set_Instances(i, batch.instances));
		for (uint32 i = 0; batch.lodModel && i < batch.lodModel->Get_NumMeshes(); ++i)
			ready = ready && SUCCEEDED(batch.lodModel->Set_Instances(i, {}));
		if (!ready)
			continue;
		batch.halfDiagonal = Vector3{ batch.localBounds.Extents }.Length();
		LOG_INFO(L"[WorldMap] {} props {} x{}{}", m_Model->Get_ModelTag(), tag, batch.instances.size(), batch.lodModel ? L" (LOD)" : L"");
		m_Props.push_back(std::move(batch));
	}
}

const Shared<Model>& WorldMap::Select_Model()
{
	/* Editor는 정지 상태에서 Update를 돌리지 않으면서 렌더는 계속하므로 판정이 Update가 아닌
	   렌더 경로에 있다. SkyBox::Follow_Camera가 같은 이유로 여기에 있다.
	   Render는 View마다 불리고, Editor는 View마다 메인 카메라를 바꿔 끼운다
	   (EditorManager.cpp:969, :982). 따라서 밴드는 그 View의 카메라로 판정해야 하고,
	   히스테리시스는 같은 카메라로 이어질 때만 적용한다. */
	if (!m_LodResolved)
	{
		m_LodResolved = true;
		// 정적 타일의 상자는 로드 뒤 바뀌지 않으므로 LOD 모델을 잡을 때 한 번만 구한다.
		m_HasModelBounds = m_Model && m_Model->Compute_LocalBounds(m_ModelBounds);

		if (m_Model && !m_Model->Get_ModelTag().empty())
		{
			// 충돌이 <ModelTag>_COL을 따라가는 것과 같은 규칙이다.
			const wstring lodTag = m_Model->Get_ModelTag() + L"_LOD";
			const int32 levIndex = GAME_INSTANCE->Get_ContainLevelByModelTag(lodTag);
			if (levIndex != -1)
			{
				/* 정적 월드 모델은 인스턴스 상태가 없고 메시·머티리얼은 어차피 프로토타입과
				   공유되므로 클론하지 않고 프로토타입을 그대로 그린다. */
				m_LodModel = GAME_INSTANCE->Get_Model(static_cast<uint32>(levIndex), lodTag.c_str());
				if (m_LodModel && FAILED(m_LodModel->Validate_MaterialBindings(m_Shader)))
				{
					LOG_WARN(L"[WorldMap] LOD model {} failed material validation; LOD disabled", lodTag);
					m_LodModel = nullptr;
				}
				// 지오메트리가 없는 LOD 모델은 거리도 잴 수 없고 그릴 것도 없다.
				if (m_LodModel && !m_LodModel->Compute_LocalBounds(m_LodBounds))
					m_LodModel = nullptr;
			}

		}

		Ready_LodBands();
		Ready_Props();

		const auto hasGroup = [](const Shared<Model>& model, RENDERGROUP group) {
			for (uint32 i = 0; model && i < model->Get_NumMeshes(); ++i)
				if (group == model->Get_MeshRenderGroup(i))
					return true;
			return false;
		};
		m_HasDecals = hasGroup(m_Model, RENDERGROUP::DECAL) || hasGroup(m_LodModel, RENDERGROUP::DECAL);
		m_HasBlend = hasGroup(m_Model, RENDERGROUP::BLEND) || hasGroup(m_LodModel, RENDERGROUP::BLEND);
		for (const PROP_BATCH& prop : m_Props)
		{
			m_HasDecals = m_HasDecals || hasGroup(prop.model, RENDERGROUP::DECAL);
			m_HasBlend = m_HasBlend || hasGroup(prop.model, RENDERGROUP::BLEND);
		}
	}

	if (nullptr == m_LodModel || m_LodBand1.empty() || !GAME_INSTANCE->Get_WorldLod())
	{
		m_LodBand = 0;
		return m_Model;
	}

	const auto camera = GAME_INSTANCE->Get_MainCamera();
	/* 원본 모델 상자는 타일 밖 물체까지 품는다. G11319의 g11420_dummybuild는 214 떨어진
	   이웃 타일 위에, G11121의 enkei 판은 수천 밖에 있어 카메라가 이웃 타일로 가도 상자 안에
	   남아 LOD0에 머문다. 바꿔 그릴 대상인 LOD 모델의 상자로 잰다. */
	if (camera && camera->Get_Transform())
	{
		BoundingBox tileWorld{};
		m_LodBounds.Transform(tileWorld, m_Transform->Get_WorldMatrix());

		const Vector3 center{ tileWorld.Center };
		const Vector3 extents{ tileWorld.Extents };
		const Float radius = extents.Length();

		/* 중심까지가 아니라 AABB에서 가장 가까운 점까지를 잰다. 타일이 200~600으로 커서
		   중심 거리로 재면 바로 옆에 서 있어도 멀다고 판정된다. 상자 안에 있으면 0이 되어
		   항상 LOD0이고, 멀어지면 중심 거리에서 반경을 뺀 값에 수렴한다. */
		const Vector3 cameraPosition = camera->Get_Transform()->Get_Position();
		const Vector3 boxMin = center - extents;
		const Vector3 boxMax = center + extents;
		const Vector3 nearestPoint{
			std::clamp(cameraPosition.x, boxMin.x, boxMax.x),
			std::clamp(cameraPosition.y, boxMin.y, boxMax.y),
			std::clamp(cameraPosition.z, boxMin.z, boxMax.z) };
		const Float distance = Vector3::Distance(nearestPoint, cameraPosition);

		// 겉보기 크기. 가까울수록 커진다.
		const Float size = distance > 0.f ? radius / distance : FLT_MAX;

		if (camera.get() != m_LodBandCamera)
		{
			/* 다른 View다. 이전 밴드를 이어받으면 한 번에 한 단계씩만 움직이는 탓에
			   다른 카메라가 밀어놓은 밴드에서 못 빠져나온다. 이 카메라 기준으로 새로 판정한다. */
			m_LodBand = Band_From_Size(size);
			m_LodBandCamera = camera.get();
		}
		else
		{
			switch (m_LodBand)
			{
			case 0:
				if (size < LOD_RATE_LEAVE_LOD0 * (1.f - LOD_HYSTERESIS)) m_LodBand = 1;
				break;
			case 1:
				if (size < LOD_RATE_LEAVE_LOD1 * (1.f - LOD_HYSTERESIS)) m_LodBand = 2;
				else if (size > LOD_RATE_LEAVE_LOD0 * (1.f + LOD_HYSTERESIS)) m_LodBand = 0;
				break;
			default:
				if (size > LOD_RATE_LEAVE_LOD1 * (1.f + LOD_HYSTERESIS)) m_LodBand = 1;
				break;
			}
		}

		// LOD2가 아예 없는 타일은 밴드2로 내려가도 볼 게 없으므로 밴드1에 머문다.
		if (m_LodBand == 2 && m_LodBand2.empty())
			m_LodBand = 1;
	}

	if (m_LodBand != 0)
	{
		GAME_INSTANCE->Add_LodTile();
		return m_LodModel;
	}

	return m_Model;
}

void WorldMap::Submit_RenderGroup()
{
	// 월드 타일은 움직이지 않으므로 STATIC: 화면은 NONBLEND와 같이 G-buffer에 그리고, 그림자는 Renderer가 구워 둔 깊이를 재사용한다.
	GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::STATIC, shared_from_this());
	// 도로 무늬·균열 같은 데칼 메시는 모든 타일의 불투명 메시 뒤에 섞어야 하므로 DECAL 그룹에서 한 번 더 그린다.
	if (m_HasDecals)
		GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::DECAL, shared_from_this());
	// 물은 조명·합성이 끝난 화면 위에 포워드로 섞으므로 BLEND 그룹에서 한 번 더 그린다.
	if (m_HasBlend)
		GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::BLEND, shared_from_this());
}

HRESULT WorldMap::Ready_Components()
{
	// 타일 프리셋(ProjectSetting/Material/CityOfRuin*.json)의 shaderTag와 같은 맵 셰이더다. 본체와 _LOD가 함께 쓴다.
	Shader::SHADER_DESC shaderDesc{ MapShaderTag,  VTXMESH::Elements, VTXMESH::numElements };
	m_Shader = Add_Component<Shader>(ETOI(LEVEL::STATIC), &shaderDesc);
	if (nullptr == m_Shader)
		return E_FAIL;

	// 기본 모델은 없다. 모델은 씬에 저장된 ModelTag로 정해진다(레벨마다 있는 타일이 달라 고정 기본값을 둘 수 없다).
	Model::MODEL_DESC modelDesc{ L"" };
	m_Model = Add_Component<Model>(ETOI(LEVEL::STATIC), &modelDesc);
	if (nullptr == m_Model)
		return E_FAIL;
	if (FAILED(m_Model->Validate_MaterialBindings(m_Shader))) return E_FAIL;

	// 태그를 비워 두면 Post_Load에서 최종 ModelTag를 따라간다. 길찾기 데이터 전용이다.
	Navigation::NAVIGATION_DESC navDesc{};
	m_Navigation = Add_Component<Navigation>(ETOI(LEVEL::STATIC), &navDesc);
	if (nullptr == m_Navigation)
		LOG_ERROR(L"Failed to attach Navigation to WorldMap {}", m_ObjectName);

	// 충돌은 원작 COL(<ModelTag>_COL)이 정의한다. 시각 메시는 충돌로 쓰지 않는다.
	WorldCollider::WORLD_COLLIDER_DESC colliderDesc{};
	m_WorldCollider = Add_Component<WorldCollider>(ETOI(LEVEL::STATIC), &colliderDesc);
	if (nullptr == m_WorldCollider)
		LOG_ERROR(L"Failed to attach WorldCollider to WorldMap {}", m_ObjectName);

	return S_OK;
}

HRESULT WorldMap::Bind_ShaderResources()
{
	if (FAILED(m_Transform->Bind_ShaderResource(m_Shader, WorldMatrix)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ViewMatrix, D3DTS::VIEW)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ProjMatrix, D3DTS::PROJ)))
		return E_FAIL;

	return S_OK;
}

Shared<WorldMap> WorldMap::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<WorldMap>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		LOG_ERROR(L"Failed to Created : WorldMap");
		MSG_BOX("Failed to Created : WorldMap");
	}
	return prototype;
}

Shared<GameObject> WorldMap::Clone(void* arg)
{
	auto instance = make_shared<WorldMap>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		LOG_ERROR(L"Failed to Cloned : WorldMap");
		MSG_BOX("Failed to Cloned : WorldMap");
	}
	return instance;
}
