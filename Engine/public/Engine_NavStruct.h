#ifndef Engine_NavStruc_h__
#define Engine_NavStruct_h__

namespace Engine
{
	/// .nnav 헤더. 셀은 원본 모델의 로컬 공간(ModelSettings pre-transform 적용 후)에 있다.
	/// 파일 이름이 모델 태그이며, 런타임에 그 모델을 그리는 GameObject의 월드 행렬로 옮겨 쓴다.
	/// 디스크 이미지와 1:1이어야 하므로 패딩 없는 레이아웃을 static_assert로 고정한다.
	struct NavMeshHeader
	{
		char     magic[4]{ 'N','N','A','V' };   // 매직 넘버
		uint32_t numCells{ 0 };
		float    bmin[3]{};                      // 로컬 셀 AABB min (디버그/검증용)
		float    bmax[3]{};                      // 로컬 셀 AABB max
	};

	static_assert(sizeof(NavMeshHeader) == 32, "NNAV header layout must stay packed at 32 bytes");

	struct NavCellBinary
	{
		Vector3   vertices[3];
		int32_t   neighborIndices[3]{ -1, -1, -1 };  // Recast에서 직접 추출한 이웃
	};
}

#endif
