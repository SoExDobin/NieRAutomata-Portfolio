#pragma once
#include "Animation.h"
#include "AnimationTracker.h"
#include "Component.h"
#include "Engine_Reflection.h"

NS_BEGIN(Engine)
class Mesh;
class SkeletalMesh;
class Material;
class Bone;
class Animation;
class Shader;
class AnimationTracker;

struct MATERIAL_TEXTURE_SETTING
{
	string slotName{};
	string resourcePath{};
};

struct MODEL_MATERIAL_SETTING
{
	uint32 materialIndex{};
	string sourceMaterialName{};
	string sourceShaderName{};
	string sourceTechniqueName{};
	string passName{ "Default_Pass" };
	// "renderGroup" in the preset: NONBLEND (opaque), DECAL (G-buffer blend over the opaque pass) or BLEND.
	RENDERGROUP renderGroup{ RENDERGROUP::NONBLEND };
	vector<MATERIAL_TEXTURE_SETTING> textures{};
	MaterialParameters parameters{};
};

struct MODEL_MATERIAL_SETTINGS
{
	uint32 schemaVersion{ 1 };
	string modelTag{};
	string modelPath{};
	// Optional "shaderTag": the shader (vertex tag + variant, e.g. VTX_Animation_Mesh_Weapon) whose passes and
	// slots these materials use. Empty = the model's default vertex shader.
	string shaderTag{};
	vector<MODEL_MATERIAL_SETTING> materials{};
private:
	friend class Model;
	// Runtime-only preparation; never serialized into the authored document.
	vector<Shared<const Material>> preparedMaterials{};
	vector<string> textureSlots{};
};

class ENGINE_DLL Model final : public Component
{
public:
	typedef struct tagModelDesc : public COMPONENT_DESC {
		tagModelDesc(const wstring& tag) : modelTag{ tag } {}
		wstring modelTag{};
	} MODEL_DESC;

public:
	explicit Model();
	explicit Model(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Model(const Model& rhs);
	~Model() override = default;

public:
	uint32 Get_NumMeshes() const { return m_NumMeshes; }
	uint32 Get_Num_Bones() const { return m_NumBones; }
	uint32 Get_NumAnimations() const { return m_NumAnimation; }
	Bool Is_Skeletal() const { return m_IsSkeletal; }

public:
	COMPONENT_TYPE Get_ComponentType() const override { return COMPONENT_TYPE::MODEL; }
	HRESULT Initialize_Prototype(const tChar* modelFilePath, const Matrix& preLocalTransformMatrix);
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	void On_Destroy() override;

public:
	void Update_ModelAnimation(Float timeDelta);
	void Set_Animation(uint32 index, Float blendDuration = 0.5f);
	HRESULT Load_Animations(const vector<wstring>& animationFilePaths);
	HRESULT Apply_AnimationPreset(const AnimationPresetSnapshot& preset);

public:
	Bool Is_Blending() const { return m_IsBlending; }
	int32 Get_AnimationIndexByName(const wstring& name);
	const wstring& Get_AnimationNameByIndex(uint32 index);
	void Set_AnimationIndex(uint32 index) { m_CurrentAnimIndex = index; }
	uint32 Get_AnimationIndex() const { return m_CurrentAnimIndex; }
	uint32 Get_NextAnimationIndex() const { return m_NextAnimIndex; }
	void Set_AnimLoop(Bool isLoop) { m_IsAnimLoop = isLoop; }
	Bool Is_AnimLoop() const { return m_IsAnimLoop; }
	Bool Is_AnimationFinished() const { return m_IsAnimEnd; }
	Float Get_AnimationProgress() const {
		if (m_Animations.empty())
			return 0.f;
		if (m_IsBlending)
			return m_Animations[m_NextAnimIndex]->Get_Progress();
		return m_Animations[m_CurrentAnimIndex]->Get_Progress();
	}
	// Moves the current clip to a point (0~1) and poses the bones there; the Model Viewer scrubs and weapons restart a clip with it.
	void Set_AnimationProgress(Float progress);

public: /* Animation Tracker */
	void Add_AnimNotify(uint32 animIndex, const AnimationTracker::ANIMATION_NOTIFY& notify);
	void Add_AnimNotify(uint32 animIndex, std::initializer_list<AnimationTracker::ANIMATION_NOTIFY> notifies);
	void Clear_AnimNotifies();
	Bool Is_NotifyActive(uint32 animIndex, const wstring & notifyTag) const;
	Bool Is_NotifyActive(const wstring & notifyName) const;

public: /* Mesh Info */
	HRESULT Get_MeshMaterialInfo(uint32 meshIndex, string& meshName, uint32& materialIndex) const;
	void Extract_RawMeshData(_Out_ vector<Float>& outPositions, _Out_ vector<int32>& outIndices) const;
	/* Bind-pose AABB in the owner's local space (pre-transform applied). False when there is no geometry. */
	Bool Compute_LocalBounds(_Out_ BoundingBox& outBounds) const;
	// 메시 하나의 로컬 AABB. 컬링처럼 메시 단위로 판정하는 호출부가 쓴다.
	Bool Get_MeshLocalBounds(uint32 meshIndex, _Out_ BoundingBox& outBounds) const;
	// Static mesh segment query in owner-local space (pre-transform already applied).
	Bool Intersect_MeshSegment(uint32 meshIndex, const Vector3& start, const Vector3& end,
		Vector3& outHit, Vector3& outNormal) const;
	/* Bind-space centre of the vertices each palette bone of a skeletal mesh dominates, indexed like the skinning
	   palette (zero for a bone that dominates none). A machine breaking into its bone-driven parts pivots each here. */
	Bool Get_MeshPartCenters(uint32 meshIndex, _Out_ vector<Vector3>& outCenters) const;
	/* AABB of the pose last built for skinning (bind pose if none); meshIndex -1 = every mesh.
	   Static models return their local bounds. Used to frame editor previews of animated models. */
	Bool Compute_PosedBounds(_Out_ BoundingBox& outBounds, int32 meshIndex = -1);

public: /* snap shot */
	vector<BONE_SNAPSHOT> Get_SnapShot_BoneMatrices();

public:
	int32 Get_BoneIndexByName(const string& boneName) const;
	Matrix Get_BoneMatrix(uint32 boneIndex) const; // 특정 부모 뼈대의 트랜스폼 가져오기
	// The bone's combined matrix (Get_BoneMatrix's space) in the bind pose the meshes are skinned in. Bone-space offsets of
	// hit volumes are read off it: Get_BoneMatrix already holds the first clip's first frame once clips are loaded.
	Matrix Get_BindBoneMatrix(uint32 boneIndex) const;
	/* 애니메이션이 만든 이번 포즈 위에 본들의 결합 행렬(Get_BoneMatrix와 같은 공간)을 덮어쓰고 자식 본을 따라 옮긴다.
	   본의 애니메이션 값은 그대로라 다음 Update_ModelAnimation은 클립 포즈에서 다시 시작한다. 천 시뮬레이션이 쓴다. */
	void Override_BoneMatrices(const vector<uint32>& boneIndices, const vector<Matrix>& combinedMatrices);
	const TRANSFORM_FRAME& Get_RootTransformVelocity(uint32 nodeIndex);
	void Set_LocalRootNode(uint32 nodeIndex);

	HRESULT Render(uint32 meshIndex);
	/* 정적 모델의 meshIndex 메시를 instances 수만큼 그리게 한다(VTXMESH_INSTANCE 셰이더로 Render). 처음 부를 때 그 메시를
	   이 Model만의 InstanceMesh 사본(버퍼 공유)으로 바꾸므로 프로토타입과 다른 클론은 그대로다. 스켈레탈 모델은 E_FAIL. */
	HRESULT Set_Instances(uint32 meshIndex, const vector<INSTANCE_DATA>& instances);
	HRESULT Bind_Material(const Shared<Shader>& shader, const Char* constantName, uint32 meshIndex, uint32 materialType, uint32 textureIndex = 0);
	HRESULT Bind_BoneMatrices(const Shared<Shader>& shader, const Char* constantName, uint32 meshIndex);
	HRESULT Validate_MaterialBindings(const Shared<Shader>& shader) const;
	HRESULT BindAndBeginMaterial(const Shared<Shader>& shader, uint32 meshIndex) const;
	/// 한 오브젝트의 메시들을 차례로 그린다. 정적 모델은 같은 재질이 이어지면 다시 바인딩하지 않는다.
	HRESULT Render_Meshes(const Shared<Shader>& shader, const vector<uint32>& meshIndices);

public:
	const wstring& Get_ModelTag() const { return m_ModelTag; }
	const string& Get_ModelResourcePath() const { return m_ModelResourcePath; }
	const wstring& Get_ModelFilePath() const { return m_ModelFilePath; }
	Shared<const MODEL_MATERIAL_SETTINGS> Get_MaterialSettings() const { return m_MaterialSettings; }
	// Render group of the mesh's material preset; NONBLEND without a preset.
	RENDERGROUP Get_MeshRenderGroup(uint32 meshIndex) const;
	vector<string> Get_MaterialNames() const;
	// Fails (state unchanged) when the tag is not loaded or its skeletal/static kind differs.
	HRESULT Set_ModelTag(const wstring& tag);
	const AnimationPresetSnapshot& Get_AnimationPreset() const { return m_AnimationPreset; }
	void Set_AnimationPreset(const AnimationPresetSnapshot& preset) { m_AnimationPreset = preset; }
	HRESULT Validate_MaterialSettings(const filesystem::path& settingsPath, const wstring& expectedModelTag) const;
	HRESULT Load_MaterialSettings(const filesystem::path& settingsPath, const wstring& expectedModelTag,
		const Shared<Shader>& shader = nullptr);
	HRESULT Prepare_MaterialSettings(const filesystem::path& settingsPath, const wstring& expectedModelTag,
		Shared<const MODEL_MATERIAL_SETTINGS>& prepared, const Shared<Shader>& shader = nullptr) const;
	HRESULT Apply_MaterialSettings(const Shared<const MODEL_MATERIAL_SETTINGS>& prepared);
	static wstring Make_MaterialSettingsFileName(const wstring& modelTag);

private:
	HRESULT Post_Load() override;
	HRESULT Ready_Bones(ifstream& in);
	HRESULT Ready_Meshes(ifstream& in, Bool isAnim);
	HRESULT Ready_Materials(ifstream& in, const std::string& directoryPath);
	HRESULT Ready_Animation(ifstream& in);
	HRESULT Read_AnimationData(ifstream& in, MODEL_ANIMATION& animationData,
		Bool hasStoredChannelCount, uint32 channelCount = 0) const;
	HRESULT Build_Animations(const vector<wstring>& animationFilePaths,
		const unordered_set<wstring>& existingNames,
		vector<Shared<Animation>>& outAnimations, vector<wstring>& outNames) const;
	Bool Validate_AnimationPreset(const AnimationPresetSnapshot& preset) const;
	Bool Validate_AnimationRequests(size_t clipCount) const;
	void Bind_AnimNotify(uint32 animIndex, const AnimationTracker::ANIMATION_NOTIFY& notify) const;
	void Restore_AnimationRequests();
	HRESULT Validate_MaterialBindings(const Shared<Shader>& shader,
		const Shared<const MODEL_MATERIAL_SETTINGS>& settings) const;
	// 본을 쓰는 경로 전용. 스켈레탈 모델이 아니거나 인덱스가 범위 밖이면 nullptr.
	Shared<SkeletalMesh> Get_SkeletalMesh(uint32 meshIndex) const;
	// 포즈가 바뀐 뒤 처음 불릴 때만 m_SkinMatrices를 다시 채운다. 포즈가 없으면 바인드 포즈를 만든다.
	void Update_SkinMatrices();

private:
	TRANSFORM_FRAME m_TransformFrame{};
	wstring	m_ModelTag{};
	wstring m_ResourceRootPath{};
	wstring m_ModelFilePath{};
	string m_ModelResourcePath{};
	Shared<const MODEL_MATERIAL_SETTINGS> m_MaterialSettings{};
	AnimationPresetSnapshot m_AnimationPreset{};
	Matrix	m_PreLocalTransformMatrix{};
	Bool	m_IsSkeletal{ false };
	// Header version of the loaded .model; Ready_Meshes reads VTXMESH_V2 below MODEL_VERSION_STATIC_COLOR.
	uint32	m_ModelVersion{ MODEL_VERSION };
	/* False until bone matrices were combined once; edit mode never runs the animation update. */
	Bool	m_HasPose{ false };
	/* 메시별 스키닝 행렬(오프셋 × 결합). 메시는 인스턴스끼리 공유하므로 인스턴스인 Model이 들고,
	   화면 패스와 그림자 패스가 한 프레임에 같은 포즈를 여러 번 그려도 한 번만 계산한다. */
	vector<vector<Matrix>> m_SkinMatrices;
	Bool	m_SkinDirty{ true };
	int32	m_RootLocalNode{ -1 };

private:
	Bool	m_IsAnimLoop{ false };
	Bool	m_IsPrevAnimLoop{ false };
	Bool	m_IsAnimEnd{ false };
	uint32	m_CurrentAnimIndex{};

private:
	Bool	m_IsBlending{ false };
	uint32	m_NextAnimIndex{};
	Float	m_BlendingElapsed{};
	Float	m_BlendingDuration{};

private:
	uint32 m_NumMeshes = {};
	vector<Shared<Mesh>> m_Meshes;
	uint32 m_NumMaterials = {};
	vector<Shared<Material>> m_Materials;
	uint32 m_NumBones = {};
	vector<Shared<Bone>> m_Bones;
	uint32 m_NumAnimation = {};
	// Each bone's combined matrix in the file's rest pose (the skin's bind pose), before any clip posed it.
	vector<Matrix> m_BindBoneMatrices;
	vector<Shared<Animation>> m_Animations;
	map<wstring, uint32> m_AnimationNames;
	/* .anim 경로 → 이 프로토타입 본으로 재매핑해 파싱한 클립 원본. 클론과 Set_ModelTag가 포인터를 공유해
	   같은 모델의 두 번째 인스턴스부터는 파일을 다시 읽지 않고 복제만 한다. 스켈레탈 프로토타입만 만든다. */
	struct ANIMATION_CLIP_CACHE;
	Shared<ANIMATION_CLIP_CACHE> m_ClipCache;
	Shared<AnimationTracker> m_Tracker{ nullptr };
	// Instance callbacks are deliberately not copied from resource prototypes.
	vector<pair<uint32, AnimationTracker::ANIMATION_NOTIFY>> m_AnimationNotifyDefinitions;
	uint32 m_PendingAnimIndex{ UINT32_MAX };

public:
	static Shared<Model> CreatePrototype();
	static Shared<Model> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context, const tChar* path, const Matrix& preLocalTransformMatrix = XMMatrixIdentity());
	Shared<Component> Clone(void* arg = nullptr) override;
};

NS_END
