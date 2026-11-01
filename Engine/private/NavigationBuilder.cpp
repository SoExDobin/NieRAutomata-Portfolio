#include "NavigationBuilder.h"

#include <fstream>

#include "SpdLogger.h"
#include "Engine_ID.h"
#include <recastnavigation/Recast.h>
#include <recastnavigation/RecastAlloc.h>
#include "Model.h"
#include "String_Helper.h"

NavigationBuilder::NavigationBuilder() : EngineManager{} {}

HRESULT NavigationBuilder::Initialize_Prototype()
{
	return EngineManager::Initialize_Prototype();
}

HRESULT NavigationBuilder::Initialize(void* arg)
{
	return EngineManager::Initialize(arg);
}

HRESULT NavigationBuilder::Begin()
{
	return EngineManager::Begin();
}

vector<NavCellBinary> NavigationBuilder::Bake_Navigation(Shared<Model> model, const rcConfig& config)
{
	return Bake_Internal(model, config);
}

HRESULT NavigationBuilder::Export_Binary(const string& fileName, Shared<Model> model, const rcConfig& config)
{
	vector<NavCellBinary> bakedData = Bake_Internal(model, config);
	if (bakedData.empty())
	{
		MSG_BOX("[ NavMesh Builder Export ] There is no vertex or just failed");
		return E_FAIL;
	}

	NavMeshHeader header{};
	memcpy(header.magic, "NNAV", 4);
	header.numCells = static_cast<uint32>(bakedData.size());

	// AABB 계산
	Vector3 bmin{ FLT_MAX, FLT_MAX, FLT_MAX };
	Vector3 bmax{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
	for (auto& cell : bakedData)
	{
		for (int32 j = 0; j < 3; ++j)
		{
			bmin = Vector3::Min(bmin, cell.vertices[j]);
			bmax = Vector3::Max(bmax, cell.vertices[j]);
		}
	}
	header.bmin[0] = bmin.x; header.bmin[1] = bmin.y; header.bmin[2] = bmin.z;
	header.bmax[0] = bmax.x; header.bmax[1] = bmax.y; header.bmax[2] = bmax.z;

	std::ofstream fout(fileName, std::ios::binary);
	if (!fout.is_open()) return E_FAIL;

	fout.write(reinterpret_cast<Char*>(&header), sizeof(NavMeshHeader));
	fout.write(reinterpret_cast<Char*>(bakedData.data()), sizeof(NavCellBinary) * bakedData.size());
	fout.close();

	return S_OK;
}

vector<NavCellBinary> NavigationBuilder::Bake_Internal(Shared<Model> model, rcConfig config)
{
	vector<NavCellBinary> resultData;
	if (model == nullptr) return resultData;

	// 1. 모델에서 Raw 데이터 추출
	vector<Float> rawPos;
	vector<int32_t> rawIndices;
	model->Extract_RawMeshData(rawPos, rawIndices);

	int32 numVertices = static_cast<int32>(rawPos.size() / 3);
	int32 numTris = static_cast<int32>(rawIndices.size() / 3);

	if (numVertices == 0 || numTris == 0) return resultData;

	// 2. 모델 로컬 좌표 그대로 굽는다 (raw 위치에는 ModelSettings pre-transform이 이미 적용돼 있다)

	rcContext ctx;
	config.maxVertsPerPoly = 3; // NavCell 구조체(삼각형)를 위해 3으로 고정

	rcCalcBounds(rawPos.data(), numVertices, config.bmin, config.bmax);
	rcCalcGridSize(config.bmin, config.bmax, config.cs, &config.width, &config.height);

	rcHeightfield* solid = nullptr;
	rcCompactHeightfield* chf = nullptr;
	rcContourSet* contour = nullptr;
	rcPolyMesh* polyMesh = nullptr;
	uByte* triAreas = nullptr;
	Bool isBuildSuccess = false;

	// Recast 빌드 파이프라인
	solid = rcAllocHeightfield();
	if (solid && rcCreateHeightfield(&ctx, *solid, config.width, config.height, config.bmin, config.bmax, config.cs, config.ch))
	{
		triAreas = new unsigned char[numTris];
		memset(triAreas, 0, numTris);
		rcMarkWalkableTriangles(&ctx, config.walkableSlopeAngle, rawPos.data(), numVertices, rawIndices.data(), numTris, triAreas);

		if (!rcRasterizeTriangles(&ctx, rawPos.data(), numVertices, rawIndices.data(), triAreas, numTris, *solid, config.walkableClimb))
		{
			MSG_BOX("[ Failed NavMesh Builder Bake_Internal ] rcRasterizeTriangles");
			delete[] triAreas;
			rcFreeHeightField(solid);
			return resultData;
		}

		rcFilterLowHangingWalkableObstacles(&ctx, config.walkableClimb, *solid);
		rcFilterLedgeSpans(&ctx, config.walkableHeight, config.walkableClimb, *solid);
		rcFilterWalkableLowHeightSpans(&ctx, config.walkableHeight, *solid);

		chf = rcAllocCompactHeightfield();
		if (!chf || !rcBuildCompactHeightfield(&ctx, config.walkableHeight, config.walkableClimb, *solid, *chf))
		{
			MSG_BOX("[ Failed NavMesh Builder Bake_Internal ] rcAllocCompactHeightfield");
			rcFreeCompactHeightfield(chf);
			rcFreeHeightField(solid);
			delete[] triAreas;
			return resultData;
		}

		if (!rcErodeWalkableArea(&ctx, config.walkableRadius, *chf))
		{
			MSG_BOX("[ Failed NavMesh Builder Bake_Internal ] rcErodeWalkableArea");
			rcFreeCompactHeightfield(chf);
			rcFreeHeightField(solid);
			delete[] triAreas;
			return resultData;
		}

		if (!rcBuildDistanceField(&ctx, *chf) || !rcBuildRegions(&ctx, *chf, 0, config.minRegionArea, config.mergeRegionArea))
		{
			MSG_BOX("[ Failed NavMesh Builder Bake_Internal ] rcBuildDistanceField || rcBuildRegions");
			rcFreeCompactHeightfield(chf);
			rcFreeHeightField(solid);
			delete[] triAreas;
			return resultData;
		}

		contour = rcAllocContourSet();
		if (!contour || !rcBuildContours(&ctx, *chf, config.maxSimplificationError, config.maxEdgeLen, *contour))
		{
			MSG_BOX("[ Failed NavMesh Builder Bake_Internal ] rcAllocContourSet || rcBuildContours");
			rcFreeContourSet(contour);
			rcFreeCompactHeightfield(chf);
			rcFreeHeightField(solid);
			delete[] triAreas;
			return resultData; // ← 기존에 return 누락되어 contour null 역참조 발생했던 곳
		}

		polyMesh = rcAllocPolyMesh();
		if (polyMesh && rcBuildPolyMesh(&ctx, *contour, config.maxVertsPerPoly, *polyMesh))
		{
			isBuildSuccess = true;
		}
	}

	if (isBuildSuccess && polyMesh)
	{
		const int32 nvp = polyMesh->nvp; // 3으로 강제됨
		resultData.reserve(polyMesh->npolys);

		for (int i = 0; i < polyMesh->npolys; ++i)
		{
			NavCellBinary cell;
			const uShort* p = &polyMesh->polys[i * nvp * 2];

			// 정점 추출 (quantized → 모델 로컬 좌표 복원)
			for (int j = 0; j < 3; ++j)
			{
				if (p[j] == RC_MESH_NULL_IDX) continue;
				const uShort* v = &polyMesh->verts[p[j] * 3];

				cell.vertices[j].x = config.bmin[0] + v[0] * config.cs;
				cell.vertices[j].y = config.bmin[1] + v[1] * config.ch;
				cell.vertices[j].z = config.bmin[2] + v[2] * config.cs;
			}

			// 이웃 정보 추출 (Recast polys 배열에서 직접)
			for (int32 j = 0; j < 3; ++j)
			{
				unsigned short neighbor = p[nvp + j];
				if (neighbor == RC_MESH_NULL_IDX || neighbor >= polyMesh->npolys)
				{
					cell.neighborIndices[j] = -1;
				}
				else
				{
					cell.neighborIndices[j] = static_cast<int32_t>(neighbor);
				}
			}

			resultData.push_back(cell);
		}
	}

	// 메모리 누수 방지
	delete[] triAreas;
	if (solid)      rcFreeHeightField(solid);
	if (chf)        rcFreeCompactHeightfield(chf);
	if (contour)    rcFreeContourSet(contour);
	if (polyMesh)   rcFreePolyMesh(polyMesh);

	return resultData;
}

vector<NavCell> NavigationBuilder::Import_Binary(const string& filePath)
{
	vector<NavCell> cells;

	std::ifstream fin(filePath, std::ios::binary);
	if (!fin.is_open())
	{
		LOG_ERROR(L"[NavMeshBuilder] Import_Binary: 파일 열기 실패 ({})", Helper::To_wString(filePath));
		return cells;
	}

	NavMeshHeader header{};
	fin.read(reinterpret_cast<char*>(&header), sizeof(NavMeshHeader));
	if (fin.gcount() != static_cast<std::streamsize>(sizeof(NavMeshHeader)))
	{
		LOG_ERROR(L"[NavMeshBuilder] Import_Binary: 헤더가 잘렸습니다 ({})", Helper::To_wString(filePath));
		return cells;
	}

	// 매직 넘버 검증
	if (memcmp(header.magic, "NNAV", 4) != 0)
	{
		LOG_ERROR(L"[NavMeshBuilder] Import_Binary: 잘못된 매직 넘버");
		return cells;
	}

	// version 필드가 없으므로 파일 크기로 페이로드 정합성을 확인한다.
	fin.seekg(0, std::ios::end);
	const std::streamoff fileSize = fin.tellg();
	fin.seekg(sizeof(NavMeshHeader), std::ios::beg);

	const std::streamoff expectedSize =
		static_cast<std::streamoff>(sizeof(NavMeshHeader)) +
		static_cast<std::streamoff>(header.numCells) * static_cast<std::streamoff>(sizeof(NavCellBinary));
	if (fileSize != expectedSize)
	{
		LOG_ERROR(L"[NavMeshBuilder] Import_Binary: 크기 불일치 ({} != {}) ({}). 다시 bake하세요.",
			static_cast<long long>(fileSize), static_cast<long long>(expectedSize), Helper::To_wString(filePath));
		return cells;
	}

	cells.reserve(header.numCells);
	for (uint32 i = 0; i < header.numCells; ++i)
	{
		NavCellBinary bin{};
		fin.read(reinterpret_cast<char*>(&bin), sizeof(NavCellBinary));

		NavCell cell(bin.vertices[0], bin.vertices[1], bin.vertices[2], static_cast<int32>(i));

		// Recast에서 추출한 이웃을 직접 대입 (SetUp_Neighbors 불필요)
		cell.Set_Neighbor(NavCell::CELL_LINE::AB, bin.neighborIndices[0]);
		cell.Set_Neighbor(NavCell::CELL_LINE::BC, bin.neighborIndices[1]);
		cell.Set_Neighbor(NavCell::CELL_LINE::CA, bin.neighborIndices[2]);

		cells.push_back(std::move(cell));
	}

	LOG_INFO(L"[NavMeshBuilder] Import_Binary: {} cells 로드 완료", cells.size());
	return cells;
}

// ─── Build (런타임용) ── rcPolyMesh 기반, Recast 이웃 직접 사용 ───
NavigationBuilder::NAV_BUILD_RESULT NavigationBuilder::Build(const Float* vertices, int32 numVertices,
                                                             const int32* triangles, int32 numTriangles, 
                                                             const NAV_BUILD_PARAMS_DESC& params)
{
	NAV_BUILD_RESULT build{};
	rcContext ctx{};
	rcConfig cfg{};

	// 1. rcConfig 구성
	memset(&cfg, 0, sizeof(cfg));
	cfg.cs = params.cellSize;
	cfg.ch = params.cellHeight;
	cfg.walkableSlopeAngle = params.moveableSlopeAngle;
	cfg.walkableHeight = static_cast<int32>(ceilf(params.agentHeight / cfg.ch));
	cfg.walkableClimb = static_cast<int32>(floorf(params.agentMaxClimb / cfg.ch));
	cfg.walkableRadius = static_cast<int32>(ceilf(params.agentRadius / cfg.cs));
	cfg.maxEdgeLen = static_cast<int32>(params.maxEdgeLen / cfg.cs);
	cfg.minRegionArea = params.minRegionArea * params.minRegionArea;
	cfg.mergeRegionArea = params.mergeRegionArea * params.mergeRegionArea;
	cfg.maxVertsPerPoly = 3; // NavCell(삼각형) 구조 + Recast 이웃 1:1 매핑을 위해 3 고정
	cfg.detailSampleDist = params.detailSampleDist < 0.9f ? 0.f : cfg.cs * params.detailSampleDist;
	cfg.detailSampleMaxError = params.detailSampleMaxError;
	cfg.maxSimplificationError = params.maxSimplificationError;

	// 2. AABB & Grid
	rcCalcBounds(vertices, numVertices, cfg.bmin, cfg.bmax);
	rcCalcGridSize(cfg.bmin, cfg.bmax, cfg.cs, &cfg.width, &cfg.height);

#ifdef _DEBUG
	LOG_INFO(L"[ NavMeshBuilder ] Grid: {}x{}, Cells: {}", cfg.width, cfg.height, cfg.width * cfg.height);
#endif

	// 3. HeightField 
	rcHeightfield* solid = rcAllocHeightfield();
	if (!solid)
	{
		LOG_ERROR(L"[NavMeshBuilder] failed to ALLOC rcAllocHeightfield");
		return build;
	}

	if (!rcCreateHeightfield(&ctx, *solid, cfg.width, cfg.height, 
		cfg.bmin, cfg.bmax, cfg.cs, cfg.ch))
	{
		LOG_ERROR(L"[NavMeshBuilder] failed to CREATE rcCreateHeightfield");
		rcFreeHeightField(solid);
		return build;
	}

	// 4. Walkable
	uByte* triangleAreas = new uByte[numTriangles];
	memset(triangleAreas, 0, numTriangles);
	rcMarkWalkableTriangles(&ctx, cfg.walkableSlopeAngle, vertices, numVertices, triangles, numTriangles, triangleAreas);

	// 5. Rasterize
	if (!rcRasterizeTriangles(&ctx, vertices, numVertices,
		triangles, triangleAreas, numTriangles, *solid))
	{
		LOG_ERROR(L"[NavMeshBuilder] failed to RASTERIZE rcRasterizeTriangles");
		delete[] triangleAreas;
		rcFreeHeightField(solid);
		return build;
	}
	delete[] triangleAreas;

	// 6. Filtering
	rcFilterLowHangingWalkableObstacles(&ctx, cfg.walkableClimb, *solid);
	rcFilterLedgeSpans(&ctx, cfg.walkableHeight, cfg.walkableClimb, *solid);
	rcFilterWalkableLowHeightSpans(&ctx, cfg.walkableHeight, *solid);

	// 7. Compact
	rcCompactHeightfield* chf = rcAllocCompactHeightfield();
	if (!chf)
	{
		LOG_ERROR(L"[NavMeshBuilder] failed to ALLOC COMPACT rcAllocCompactHeightfield");
		rcFreeHeightField(solid);
		return build;
	}

	if (!rcBuildCompactHeightfield(&ctx, cfg.walkableHeight, cfg.walkableClimb, *solid, *chf))
	{
		LOG_ERROR(L"[NavMeshBuilder] failed to BUILD COMPACT rcBuildCompactHeightfield");
		rcFreeCompactHeightfield(chf);
		rcFreeHeightField(solid);
		return build;
	}
	rcFreeHeightField(solid);
	solid = nullptr;

	// 8. Agent Erode
	if (!rcErodeWalkableArea(&ctx, cfg.walkableRadius, *chf))
	{
		LOG_ERROR(L"[NavMeshBuilder] failed to AGENT ERODE WALKABLE rcErodeWalkableArea");
		rcFreeCompactHeightfield(chf);
		return build;
	}

	// 9. Distance Field
	if (!rcBuildDistanceField(&ctx, *chf))
	{
		LOG_ERROR(L"[NavMeshBuilder] failed to BUILD DISTANCE rcBuildDistanceField");
		rcFreeCompactHeightfield(chf);
		return build;
	}

	// 10. Region
	if (!rcBuildRegions(&ctx, *chf, cfg.borderSize, cfg.minRegionArea, cfg.mergeRegionArea))
	{
		LOG_ERROR(L"[NavMeshBuilder] failed to BUILD REGION rcBuildRegions");
		rcFreeCompactHeightfield(chf);
		return build;
	}

	// 11. Contour
	rcContourSet* contourSet = rcAllocContourSet();
	if (!contourSet)
	{
		LOG_ERROR(L"[NavMeshBuilder] failed to ALLOC CONTOUR rcAllocContourSet");
		rcFreeCompactHeightfield(chf);
		return build;
	}

	if (!rcBuildContours(&ctx, *chf, cfg.maxSimplificationError, cfg.maxEdgeLen, *contourSet))
	{
		LOG_ERROR(L"[NavMeshBuilder] failed to BUILD CONTOUR rcBuildContours");
		rcFreeContourSet(contourSet);
		rcFreeCompactHeightfield(chf);
		return build;
	}

	// 12. Polygon Mesh (maxVertsPerPoly=3 → 삼각형)
	rcPolyMesh* polyMesh = rcAllocPolyMesh();
	if (!polyMesh)
	{
		LOG_ERROR(L"[NavMeshBuilder] failed to ALLOC POLYGON MESH rcAllocPolyMesh");
		rcFreeContourSet(contourSet);
		rcFreeCompactHeightfield(chf);
		return build;
	}
	if (!rcBuildPolyMesh(&ctx, *contourSet, cfg.maxVertsPerPoly, *polyMesh))
	{
		LOG_ERROR(L"[NavMeshBuilder] failed to BUILD POLYGON MESH rcBuildPolyMesh");
		rcFreePolyMesh(polyMesh);
		rcFreeContourSet(contourSet);
		rcFreeCompactHeightfield(chf);
		return build;
	}
	rcFreeContourSet(contourSet);
	rcFreeCompactHeightfield(chf);

	// 13. rcPolyMesh에서 NavCell 추출 + Recast 이웃 직접 대입 (Detail/SetUp_Neighbors 불필요)
	ExtractCells_FromPolyMesh(*polyMesh, cfg, build.navCells);

#ifdef _DEBUG
	LOG_INFO(L"[NavMeshBuilder] NavMesh 빌드 완료: {} cells", build.navCells.size());
#endif

	rcFreePolyMesh(polyMesh);

	build.isSuccess = true;
	return build;
}

void NavigationBuilder::ExtractCells_FromPolyMesh(
	const rcPolyMesh& mesh,
	const rcConfig& cfg,
	vector<NavCell>& outCells)
{
	outCells.clear();
	outCells.reserve(mesh.npolys);

	const int32 nvp = mesh.nvp; // 3 (삼각형)

	for (int32 i = 0; i < mesh.npolys; ++i)
	{
		const uShort* p = &mesh.polys[i * nvp * 2];

		// 정점 추출 (quantized → world 좌표 복원)
		Vector3 verts[3];
		for (int32 j = 0; j < 3; ++j)
		{
			if (p[j] == RC_MESH_NULL_IDX) continue;
			const uShort* v = &mesh.verts[p[j] * 3];

			verts[j].x = cfg.bmin[0] + v[0] * cfg.cs;
			verts[j].y = cfg.bmin[1] + v[1] * cfg.ch;
			verts[j].z = cfg.bmin[2] + v[2] * cfg.cs;
		}

		NavCell cell(verts[0], verts[1], verts[2], i);

		// Recast 이웃 직접 대입 (polys 배열의 후반부)
		for (int32 j = 0; j < 3; ++j)
		{
			uShort neighbor = p[nvp + j];
			if (neighbor == RC_MESH_NULL_IDX || neighbor >= static_cast<uShort>(mesh.npolys))
			{
				cell.Set_Neighbor(static_cast<NavCell::CELL_LINE>(j), -1);
			}
			else
			{
				cell.Set_Neighbor(static_cast<NavCell::CELL_LINE>(j), static_cast<int32>(neighbor));
			}
		}

		outCells.push_back(std::move(cell));
	}
}

Unique<NavigationBuilder> NavigationBuilder::Create()
{
	auto instance = make_unique<NavigationBuilder>();

	if (FAILED(instance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : NavigationBuilder");
		return nullptr;
	}

	return instance;
}
