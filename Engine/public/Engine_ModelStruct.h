#ifndef Engine_ModelStruc_h__
#define Engine_ModelStruct_h__

namespace Engine // For Converting
{
	constexpr char MODEL_MAGIC[4] = { 'N','M','D','L' };
	constexpr uint32 MODEL_VERSION = 3;
	// First version whose static vertices are VTXMESH with texcoord1 and color; older files store VTXMESH_V2.
	constexpr uint32 MODEL_VERSION_STATIC_COLOR = 3;

	// Static vertex of MODEL_VERSION 1-2 files (44 bytes). Model reads it and fills the new VTXMESH members.
	struct VTXMESH_V2
	{
		Vector3 position;
		Vector3 normal;
		Vector3 tangent;
		Vector2 texcoord;
	};
	static_assert(sizeof(VTXMESH_V2) == 44);
	constexpr char ANIMATION_MAGIC[4] = { 'N','A','N','M' };
	constexpr uint32 ANIMATION_VERSION = 1;
	constexpr uint32 MODEL_BONE_MAX = 512;

	typedef struct MatTexEntry
	{
		uint32_t typeIndex;
		std::string path;
	} MODEL_ENTRY;

	typedef struct ModelFileHeader
	{
		Char   magic[4];       // "NMDL"포멧
		uint32 version;        // 1
		Bool   isAnim;
		uint32 numBones;
		uint32 numMeshes;
		uint32 numMaterials;
		uint32 numAnimations;
	} MODEL_HEADER;

	typedef struct AnimationFileHeader
	{
		Char magic[4];       // "NANM"
		uint32 version;
		uint32 numChannels;
	} ANIMATION_HEADER;

	typedef struct BoneData
	{
		std::string name;
		int32_t parentIndex{ -1 };
		Matrix transform{};
	} MODEL_BONE;

	typedef struct MeshData
	{
		std::string name;
		int32 materialIndex{ 0 };
		std::vector<uint32> boneIndices;// 해당 메쉬가 사용하는 전역 본 인덱스 배열
		std::vector<Matrix> offsetMatrices; // 
		std::vector<VTXMESH> vertices;       // Non-Anim Vertices
		std::vector<VTXANIMMESH> animVertices; // Anim Vertices
		std::vector<uint32> indices;
	} MODEL_MESH;

	typedef struct MaterialData {
		std::string name;
		uint32 textureTypeMax;
		std::string directoryPath;
		std::vector<MODEL_ENTRY> textures;
	} MODEL_MATERIAL;

	typedef struct ChannelData
	{
		std::string name;
		int32 boneIndex;
		uint32 numKeyFrames;
		std::vector<KEYFRAME> keyFrames;
	} MODEL_CHANNEL;

	typedef struct AnimationData
	{
		std::string name;
		Float duration;
		Float tickPerSecond;
		uint32 numChannel;
		Vector3 rootTotalTranslation;
		Vector4 rootTotalRotation;
		std::vector<MODEL_CHANNEL> channels;
	} MODEL_ANIMATION;

}

namespace Engine
{
	typedef struct boneSnapShot
	{
		Matrix matrices[MODEL_BONE_MAX];
		uint32 numBones{ 0 };
	} BONE_SNAPSHOT;
}

#endif

/** Assimp aiTextureType Summary (0 ~ 27) */
// 0: NONE							- 텍스처 없음 (더미 수치)
// 1: DIFFUSE						- 기본 색상 (고전 방식 알베도)
// 2: SPECULAR						- 반사광 강도 및 하이라이트 영역
// 3: AMBIENT						- 환경광 반사 수치 (그림자 보정용)
// 4: EMISSIVE						- 자기 발광 (빛이 직접 나오는 효과)
// 5: HEIGHT						- 높이 맵 (범프 매핑용 그레이스케일)
// 6: NORMALS						- 노멀 맵 (탄젠트 공간 표면 굴곡 상세 표현)
// 7: SHININESS						- 광택 지수 (Phong 반사 지수 결정)
// 8: OPACITY						- 불투명도 (투명도 조절용)
// 9: DISPLACEMENT					- 정점 변위 (실제 정점 위치 이동용)
// 10: LIGHTMAP						- 구워진 조명 데이터 또는 AO
// 11: REFLECTION					- 완전 거울 반사 효과 (환경 매핑용)
// 12: BASE_COLOR					- PBR 알베도 (금속/비금속 공통 기본 컬러)
// 13: NORMAL_CAMERA				- PBR 표준 노멀 맵
// 14: EMISSION_COLOR				- PBR 발광 컬러 값
// 15: METALNESS					- PBR 금속성 수치 (Metalness Workflow)
// 16: DIFFUSE_ROUGHNESS			- PBR 표면 거칠기 (Roughness Workflow)
// 17: AMBIENT_OCCLUSION			- 구석진 곳의 환경광 차폐도 (AO)
// 18: UNKNOWN						- 정의되지 않은 미확인 타입
// 19: SHEEN						- 천/벨벳 등의 미세 섬유 광택 효과
// 20: CLEARCOAT					- 표면 위의 투명 코팅층 효과 (자동차 도색 등)
// 21: TRANSMISSION					- 유리/액체 등 투과 및 굴절 효과
// 22: MAYA_BASE					- Maya 엔진 전용 베이스 컬러
// 23: MAYA_SPECULAR				- Maya 엔진 전용 스펙큘러
// 24: MAYA_SPECULAR_COLOR			- Maya 엔진 전용 스펙큘러 컬러
// 25: MAYA_SPECULAR_ROUGHNESS		- Maya 엔진 전용 스펙큘러 거칠기
// 26: ANISOTROPY					- 비등방성 반사 효과 (머리카락, 브러시드 메탈)
// 27: GLTF_METALLIC_ROUGHNESS		- glTF 전용 패킹 (Blue: Metal, Green: Rough)
