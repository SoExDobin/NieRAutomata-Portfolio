#pragma once
#include "Camera.h"
#include "Engine_Define.h"

#include "CameraManager.h"
#include "GraphicDevice.h"
#include "InputDevice.h"
#include "LevelManager.h"
#include "LightManager.h"
#include "ObjectManager.h"
#include "Pipeline.h"
#include "PrototypeManager.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "JobManager.h"
#include "TimeManager.h"
#include "FontManager.h"
#include "SoundManager.h"
#include "EventManager.h"
#include "CollisionManager.h"
#include "PhysicsManager.h"
#include "DebugRenderer.h"
#include "RenderTargetManager.h"

#include "GameObject.h"
#include "Component.h"
#include "Engine_Reflection.h"
#include "NavigationBuilder.h"

NS_BEGIN(Engine)
class LayerRegistry;
class TagRegistry;
class Registry;
class LevelSerializer;
class PrefabManager;

class ENGINE_DLL Game {
  DECLARE_SINGLETON(Game)
	friend class PrefabManager;
protected:
  explicit Game();
  ~Game();

private:
    void Shutdown();
    Bool m_ShuttingDown = false;

public:
    HRESULT Initialize_Engine(const ENGINE_DESC &engineDesc);
    /* Project settings. Load_EngineDesc runs before Initialize_Engine: it overwrites only the fields the file has
       (startLevel, viewportWidth, viewportHeight, windowTitle), so the caller fills the defaults first. */
    HRESULT Load_EngineDesc(const wstring& filePath, ENGINE_DESC& inOutDesc) const;
    // LayerSettings.json and TagSettings.json in the project setting folder.
    HRESULT Load_LayerAndTagSettings(const wstring& projectSettingPath) const;
    Float Begin_Frame(Bool accumulateFixedTime = true);
    void Update_Engine();
    void Update_Engine(Float timeDelta, Bool singleFixedStep = false);
    uint64_t Get_RuntimeFrameCount() const { return m_RuntimeFrameCount; }
    uint32 Get_LastRuntimeFixedStepCount() const { return m_LastRuntimeFixedStepCount; }
    Float Get_LastRuntimeDelta() const { return m_LastRuntimeDelta; }
    HRESULT Draw() const;
    HRESULT Draw_NoClearing() const;

    void Update_CameraPipeline();

    void Clear_Resource(uint32 levIndex) const;
    void Clear_AllResource() const;

public: /* For Editor / Tool */
    const D3D11_VIEWPORT& Get_ViewportDesc() const { return m_GraphicDevice->Get_ViewportDesc(); }
    ComPtr<ID3D11Device> Get_Device() const { return m_GraphicDevice->Get_Device(); }
    ComPtr<ID3D11DeviceContext> Get_Context() const { return m_GraphicDevice->Get_Context(); }
    ComPtr<IDXGISwapChain1> Get_SwapChain() const { return m_GraphicDevice->Get_SwapChain(); }
    Shared<LayerRegistry> Get_LayerRegister() const { return m_LayerRegistry; }
    Shared<TagRegistry> Get_TagRegister() const { return m_TagRegistry; }

public: /* For Input Manager */
	void Update_Input() const;
    Bool Get_MouseLock() const { return m_InputDevice->Get_MouseLock(); }
    void Set_MouseLock(Bool isLock) const { return m_InputDevice->Set_MouseLock(isLock); }
    void Set_InputEnabled(Bool isEnabled) const { m_InputDevice->Set_InputEnabled(isEnabled); }
    Bool Get_InputEnabled() const { return m_InputDevice->Get_InputEnabled(); }

public: /* For GraphicDevice */
    HRESULT Clear_BackBufferView(const Shared<Float4>& clearColor) const;
    HRESULT Present() const;
    HRESULT OnResize(uint32 width, uint32 height, uint32 offScreenIndex = UINT_MAX);
    HRESULT Begin_RenderOffScreen(uint32 screenIndex) const;
    HRESULT Clear_RenderGroup() const;
    HRESULT End_RenderOffScreen() const;
    ComPtr<ID3D11ShaderResourceView> Get_OffScreenSRV(uint32 screenIndex) const;
    void Set_DepthStencilState(ID3D11DepthStencilState* state, UINT ref) const;

public:
    Byte Get_DIKeyState(uByte byKeyID) const;
    Byte Get_DIMouseState(DIMB mouseInput) const;
    Long Get_DIMouseMove(DIMM mouseState) const;

public: /* For TimeManager */
    HRESULT Add_Timer(const wstring &timerTag) const;
    void Set_TimeScale(Float timeScale) const;
    Float Get_FPS() const;
    Float Get_TotalTime() const;
    Float Compute_TimeDelta() const;
    Float Compute_UnscaledTimeDelta() const;
    Float Get_FixedDeltaTime() const;
    Float Compute_TimeDelta(const wstring &timerTag) const;

public: /* For LevelManager */
    Shared<Level> Get_CurrentLevel();
    void Update_Level() const;
    Bool LevelLoad_Finished() const { return m_LevelManager->Is_LoadFinished(); }
    uint32 Get_CurrentLevelIndex() const { return m_LevelManager->Get_CurrentLevelIndex(); }
    HRESULT Change_Level(uint32 levIndex, const Shared<class Level>& newLevel);

public: /* For PrototypeManager */
    const unordered_map<wstring, Shared<GameObject>>& Get_Prototypes(uint32 levIndex) const;
	const auto &Get_Prototype_Components() const { return m_PrototypeManager->Get_Components(); }
	HRESULT Refresh_ReflectionRegistry() const;
	HRESULT Register_ReflectionDescriptors(const ReflectionDescriptorBatch& descriptors) const;
	HRESULT Register_ReflectedPrototypes() const;
	string Find_RegisteredName(RuntimeTypeId runtimeTypeId) const;
	// The object's registered type (or its deepest registered base when its own class is not registered).
	HRESULT Find_ReflectedIdentity(const Object& object, RuntimeTypeId& outRuntimeTypeId,
		string& outRegisteredName, Shared<const vector<RuntimeTypeId>>& outTypeAncestors) const;
	RuntimeTypeId Find_RuntimeTypeId(std::string_view registeredName) const;
	HRESULT Find_ReflectedType(std::string_view registeredName, ReflectedTypeInfo& outInfo) const;
	HRESULT Find_ReflectedType(RuntimeTypeId runtimeTypeId, ReflectedTypeInfo& outInfo) const;
	vector<ReflectedTypeInfo> Get_ReflectedTypes() const;
	vector<ReflectedPropertyInfo> Get_ReflectedProperties(std::string_view registeredName) const;
	vector<ReflectedPropertyInfo> Get_ReflectedProperties(const Object& target) const;
	HRESULT Find_ReflectedEnum(std::string_view registeredName, ReflectedEnumInfo& outInfo) const;
	vector<ReflectedEnumInfo> Get_ReflectedEnums(std::string_view category = {}) const;
	HRESULT Read_ReflectedProperty(Object& target, std::string_view propertyName,
		ReflectionValue& outValue) const;
	HRESULT Write_ReflectedProperty(Object& target, std::string_view propertyName,
		const ReflectionValue& value) const;

private: /* For ObjectManager */
	HRESULT Add_GameObject(const Shared<GameObject>& GameObject, uint32 levIndex) const;

public: /* For ObjectManager */
	void Submit_RenderGroup() const;
	const unordered_map<RuntimeObjectId, Shared<GameObject>>& Get_GameObjects(uint32 levIndex) const;
	HRESULT Clear_AllGameObjects() const;
    Shared<GameObject> Find_ObjectByObjectTag(uint32 levIndex, const wstring& tag) const;
	Shared<GameObject> Find(ObjectGuid objectGuid) const;
	Shared<GameObject> Find(RuntimeObjectId runtimeObjectId) const;
	Bool Contains(uint32 levIndex, ObjectGuid objectGuid) const;
	HRESULT Find_Level(ObjectGuid objectGuid, uint32& outLevelIndex) const;
    HRESULT Destroy(ObjectGuid objectGuid) const;
	void Flush_DestroyedGameObjects() const;
	HRESULT Clear_GameObjects(uint32 levIndex) const;
    void Clearing_ObjectManager(uint32 levIndex) const;

    template <typename T> requires is_base_of_v<GameObject, T>
    Shared<T> Find(ObjectGuid objectGuid) const {
        return Cast<T>(Find(objectGuid));
    }

    template <typename T> requires is_base_of_v<GameObject, T>
    vector<Shared<T>> FindAll() const {
		const RuntimeTypeId runtimeTypeId = Make_RuntimeTypeId<T>();
        vector<Shared<T>> typedObjects;
        // The manager matched the exact runtime type, so the downcast is known.
        for (const Shared<GameObject>& object : FindAll_Internal(runtimeTypeId))
            typedObjects.push_back(std::static_pointer_cast<T>(object));
        return typedObjects;
    }

    template <typename T> requires is_base_of_v<Component, T>
    Shared<T> Add_Component(ObjectGuid ownerGuid, uint32 levIndex = 0, void* arg = nullptr) {
        const Shared<GameObject> owner = Find(ownerGuid);
        return owner ? owner->Add_Component<T>(levIndex, arg) : nullptr;
    }

public: /* For CameraManager */
    HRESULT Add_Camera(uint32 levIndex, const Shared<class Camera> &camera) const;
    vector<Shared<class Camera>> Get_Cameras(uint32 levIndex) const;
    HRESULT Set_MainCamera(const Shared<class Camera>& camera) const;
    Shared<class Camera> Get_MainCamera() const;

public: /* For JobManager */
    std::future<HRESULT> Submit_Job(std::function<HRESULT()> job) const;
    HRESULT Parallel_For(uint32 count, const std::function<HRESULT(uint32)>& job) const;

public: /* For ResourceManager */
    HRESULT Load_Shader(uint32 levIndex, const tChar* shaderFilePath, const D3D11_INPUT_ELEMENT_DESC* elements, uint32 numElements, const wstring& descriptionTag) const;
    // Shader_<VtxLayout>[_<Variant>].hlsl: input layout and tag from the file name.
    HRESULT Load_Shader(uint32 levIndex, const tChar* shaderFilePath) const;
    Shared<Shader> Get_Shader(uint32 levIndex, const tChar* shaderFilePath) const;
    // Registered shader tags of a level (vertex tag, or vertex tag + "_" + file variant), sorted.
    vector<wstring> Get_ShaderTags(uint32 levIndex) const;

    HRESULT Load_Texture(uint32 levIndex, const tChar* textureFilePath, uint32 numSRVs, const wstring& descriptionTag, Bool allowMissing = false) const;
    const Texture::TEXTURE_DESC* Get_TextureDesc(uint32 levIndex, const wstring& descriptionTag) const;
    vector<wstring> Get_TextureTags(uint32 levIndex) const;
    const ComPtr<ID3D11ShaderResourceView>& Get_Texture(uint32 levIndex, const tChar *textureFilePath) const;

	// replacedTag: this level's stand-in for another stage's model (Level 4 em0013 for em0010); asking this level for
	// replacedTag gets this model, so a document saved with replacedTag loads here too.
	HRESULT Load_Model(uint32 levIndex, const tChar* modelFilePath, const wstring& descriptionTag,
		const Matrix& preTransformMatrix, const tChar* materialSettingsPath = nullptr,
		const wstring& replacedTag = {}) const;
	HRESULT Load_ModelAnimations(uint32 levIndex, const wstring& modelTag, const vector<wstring>& animationFilePaths) const;
	Shared<Model> Get_Model(uint32 levIndex, const tChar *modelFilePath) const;
    int32 Get_ContainLevelByModelTag(const wstring &modelTag) const;
    vector<Shared<Model>> Get_Models(uint32 levIndex) const;

public: /* For Renderer */
	void Add_RenderGroup(RENDERGROUP group, const Shared<class GameObject> &gameObject) const;
    // 프러스텀 컬링·월드 LOD 스위치. Release에서도 켜고 끌 수 있어야 FPS 차이를 보일 수 있다.
    Bool Toggle_FrustumCulling() const;
    Bool Get_FrustumCulling() const;
    Bool Toggle_WorldLod() const;
    Bool Get_WorldLod() const;
    Bool Toggle_Shadow() const;
    Bool Get_Shadow() const;
    Bool Toggle_SSAO() const;
    Bool Get_SSAO() const;
    Vector3 Get_SSAOParams() const;
    void Set_SSAOParams(const Vector3& params) const;
    void Set_ColorGrade(const Vector3& grade, const Color& tint) const;
    void Set_Fog(const Color& color, Float distance, Float opacity, const Color& sunColor) const;
    void Set_Bloom(Float intensity) const;
    // Ctrl+F5 안개, Ctrl+F6 블룸, Ctrl+F7 보정 켬/끔(레벨 값을 그 View에서 중립으로 본다).
    Bool Toggle_Fog() const;
    Bool Get_Fog() const;
    Bool Toggle_Bloom() const;
    Bool Get_Bloom() const;
    Bool Toggle_ColorGrade() const;
    Bool Get_ColorGrade() const;
    void Start_ScreenGlitch(Float strength, Float duration, Bool bands = true) const;
    Bool Toggle_Volumetric() const;
    Bool Get_Volumetric() const;
    Vector3 Get_VolumetricParams() const;
    void Set_VolumetricParams(const Vector3& params) const;
    Bool Toggle_CascadeDebug() const;
    Bool Get_CascadeDebug() const;
    Bool Toggle_Instancing() const;
    Bool Get_Instancing() const;
    Bool Toggle_Occlusion() const;
    Bool Get_Occlusion() const;
    // 화면 패스에서 이 View의 지난 깊이에 완전히 가린 월드 상자인가(F6 가림 컬링). Is_Visible도 함께 본다.
    Bool Is_Occluded(const BoundingBox& worldBounds) const;
    Matrix Get_OcclusionViewProj() const;
    Bool Is_ShadowPass() const;
    RENDERGROUP Get_DrawingGroup() const;
    HRESULT Bind_ForwardLighting(const Shared<class Shader>& shader) const;
    uint32 Get_ShadowCascade() const;
    Float Get_ShadowTexelSize() const;

public: /* For Pipeline */
    HRESULT Bind_CameraPosition(const Shared<class Shader> &shader,
                                const Char *constantName) const;
    HRESULT Bind_TransformMatrix(const Shared<class Shader> &shader,
                                 const Char *constantName, D3DTS transformState);
    HRESULT Bind_TransformMatrix_Inverse(const Shared<class Shader> &shader,
                                         const Char *constantName,
                                         D3DTS transformState);
    Matrix Get_Transform(D3DTS transformState) const;
    Matrix Get_InvTransform(D3DTS transformState) const;
    Vector4 Get_CamTransform() const;
    void Set_Transform(D3DTS transformState, Matrix transformStateMatrix);
    void Update_Pipeline() const;
    Bool Is_Visible(const BoundingBox& worldBounds) const;

public: /* For.LightManager */
    const LIGHT_DESC *Get_LightDesc(uint32 index) const;
    const LIGHT_DESC *Find_ActiveLight(LIGHT type) const;
    HRESULT Add_Light(const Shared<class Light>& light) const;
    HRESULT Remove_Light(const Shared<class Light>& light) const;
    HRESULT Render_Lights(const Shared<class Shader>& shader, const Shared<class VIBuffer_Rect>& buffer) const;

public: /* For LevelSerialize */
    HRESULT SerializeLevel(uint32 levIndex, const wstring& path) const;
    HRESULT DeSerializeLevel(const wstring& path) const;
	HRESULT Register_Prefab(PrefabGuid prefabGuid, const wstring& path) const;
	HRESULT Register_PrefabDocument(const wstring& path, PrefabGuid& outPrefabGuid) const;
	HRESULT Load_PrefabRepository(const wstring& directoryPath) const;
	HRESULT Unregister_Prefab(PrefabGuid prefabGuid) const;
	wstring Find_PrefabPath(PrefabGuid prefabGuid) const;
	vector<pair<PrefabGuid, wstring>> Get_PrefabDocuments() const;
	HRESULT SerializePrefabDocument(PrefabGuid prefabGuid,
		const Shared<GameObject>& selectedRoot, uint32 levIndex = UINT_MAX) const;
	HRESULT DeSerializePrefabDocument(PrefabGuid prefabGuid,
		Shared<GameObject>& outRoot, uint32 levIndex = UINT_MAX) const;
	HRESULT SerializeSubtreeSnapshot(PrefabGuid snapshotGuid,
		const Shared<GameObject>& selectedRoot, string& outSnapshot,
		uint32 levIndex = UINT_MAX) const;
	HRESULT DeSerializeSubtreeSnapshot(PrefabGuid snapshotGuid,
		const string& snapshot, Bool preserveObjectGuids,
		Shared<GameObject>& outRoot, uint32 levIndex = UINT_MAX) const;
	ObjectGuid Consume_RestoredObjectGuid() const;
	uint32 Consume_InitializingLevel() const;

public: /* For FontManager */
    HRESULT Add_Font(const wstring& fontTag, const tChar* fontFilePath);
    void Draw_Font(const wstring& fontTag, const tChar* text, const Vector2& position, const Color& color = XMVectorSet(1.f, 1.f, 1.f, 1.f),
        Float scale = 1.f, Float spacing = 0.f);
    // x: the width at scale 1; y: the middle of a capital letter below the drawn top-left. Zero for an unknown font.
    Vector2 Measure_Font(const wstring& fontTag, const tChar* text, Float spacing = 0.f) const;

public: /* For SoundManager */
    HRESULT LoadSoundFile(const wstring& path) const;
    HRESULT Load_Sound(const wstring& soundTag, const vector<wstring>& soundFilePaths, SOUND_ORDER order = SOUND_ORDER::SEQUENCE) const;
    Bool Is_SoundLoaded(const wstring& soundKey) const;
    Bool Is_SoundPlaying(SOUNDCHANNEL channel) const;
    Float Get_SoundLevel(SOUNDCHANNEL channel) const;
    void PlaySoundFX(const wstring& soundKey, SOUNDCHANNEL id, Float volume = -1.f, const Vector3* position = nullptr) const;
    void PlaySoundFXOnce(const wstring& soundKey, SOUNDCHANNEL id, Float volume = -1.f, const Vector3* position = nullptr) const;
    void PlaySoundLoopSection(const wstring& soundKey, SOUNDCHANNEL id, Float volume, uint32 loopStartMs, uint32 loopEndMs, Bool playIntro, Float fadeInSeconds = 0.f) const;
    void StopSound(SOUNDCHANNEL targetChannel = SOUNDCHANNEL::MAX_CHANNELS) const;
    void FadeSound(SOUNDCHANNEL targetChannel, Float gain, Float seconds, Bool stop = false) const;
    void Reset_SoundSequence(const wstring& soundKey) const;
    uint32 PlaySound3D(const wstring& soundKey, Float volume, const Vector3& position, Float nearDistance, Float farDistance, Bool loop) const;
    void StopLoopSound3D(uint32 handle) const;
    void MoveLoopSound3D(uint32 handle, const Vector3& position) const;

public: /* For EventManager*/ 
    HRESULT Add_Instance_Event(uint32 levIndex, const wstring& eventTag, const std::function<void()>& callback) const;
    HRESULT Add_Permanent_Event(uint32 levIndex, const wstring& eventTag, const std::function<void()>& callback) const;
    HRESULT Remove_Event(uint32 levIndex, const wstring& eventTag) const;

public: /* CollisionManager */
    void Add_Collider(const Shared<class Collider>& collider) const;
    void Remove_Collider(const Shared<class Collider>& collider) const;

public: /* PhysicsManager */
    /// 물리 씬이 준비됐는지.
    Bool Is_PhysicsReady() const;
    /// 월드 지오메트리를 정적 충돌로 올린다. 0이면 실패.
    uint32 Add_StaticCollision(const wstring& modelTag, const Matrix& worldMatrix) const;
    void   Remove_StaticCollision(uint32 handle) const;
    Bool   Raycast_Physics(const Vector3& origin, const Vector3& direction, Float distance,
        _Out_ Vector3& outHit, _Out_opt_ Vector3* outNormal = nullptr) const;
    Bool   Has_StaticCollision() const;
    /// 올라와 있는 정적 충돌 전체의 월드 경계. 없으면 false. 정적 그림자를 굽는 범위다.
    Bool   Get_StaticCollisionBounds(_Out_ BoundingBox& outBounds) const;

    /// 캐릭터 컨트롤러. 핸들로만 다루며 PhysX 타입은 노출하지 않는다.
    uint32  Create_CharacterController(const CHARACTER_CONTROLLER_DESC& desc, const Vector3& footPosition) const;
    uint32  Move_CharacterController(uint32 handle, const Vector3& displacement, Float timeDelta) const;
    void    Set_ControllerFootPosition(uint32 handle, const Vector3& footPosition) const;
    Vector3 Get_ControllerFootPosition(uint32 handle) const;
    void    Release_CharacterController(uint32 handle) const;

    /// 소프트바디(Jolt, 천·머리 같은 본 사슬)와 그 콜라이더. 핸들로만 다루며 Jolt 타입은 노출하지 않는다.
    uint32 Create_SoftBody(const SOFTBODY_DESC& desc, const vector<Matrix>& jointMatrices) const;
    void   Simulate_SoftBody(uint32 handle, const vector<Matrix>& jointMatrices, Float timeDelta, Bool reset,
        const SOFTBODY_TUNING& tuning, const vector<WIND_VOLUME>& winds) const;
    void   Get_SoftBodyPositions(uint32 handle, _Out_ vector<Vector3>& outPositions) const;
    void   Release_SoftBody(uint32 handle) const;
    uint32 Add_SoftBodyCollider(uint32 softBody, Float radius, Float halfHeight) const;
    void   Move_SoftBodyCollider(uint32 handle, const Vector3& from, const Vector3& to) const;
    void   Remove_SoftBodyCollider(uint32 handle) const;

public: /* NavigationBuilder */
    NavigationBuilder::NAV_BUILD_RESULT Build_Navigation(const Float* vertices, int32 numVertices, const int32* triangles, int32 numTriangles, const NavigationBuilder::NAV_BUILD_PARAMS_DESC& params);
    vector<NavCellBinary> Bake_Navigation(const Shared<Model>& model, const rcConfig& config) const;
    HRESULT Export_Navigation(const string& fileName, const Shared<Model>& model, const rcConfig& config) const;
    vector<NavCell> Import_Navigation(const string& filePath) const;

public:
    HRESULT Add_RenderTarget(const wstring& renderTargetTag, uint32 sizeX, uint32 sizeY, DXGI_FORMAT pixelFormat, const Color& color = Vector4::One) const;
    HRESULT Add_MultiRenderTarget(const wstring& multiRenderTargetTag, const wstring& renderTargetTag) const;
    HRESULT Begin_MultiRenderTarget(const wstring& multiRenderTargetTag) const;
    HRESULT End_MultiRenderTarget() const;
    HRESULT Bind_RenderTarget_ShaderResource(const Shared<class Shader>& shader, const Char* constantName, const wstring& renderTargetTag) const;
    HRESULT Copy_BoundRenderTarget(const wstring& renderTargetTag) const;

public: /* Prototype & Instantiate Facade */
    template <typename T>
    HRESULT Add_TypePrototype(uint32 levIndex, const Shared<T>& prototype) {
        static_assert(std::is_base_of_v<GameObject, T> || std::is_base_of_v<Component, T>);
        return Add_TypePrototype_Internal(levIndex, std::static_pointer_cast<Object>(prototype));
    }
    template <typename T>
    HRESULT Add_ResourceComponentPrototype(uint32 levIndex, const Shared<T>& prototype,
        const wstring& resourceTag) {
        static_assert(std::is_base_of_v<Component, T>);
        return Add_ResourceComponentPrototype_Internal(levIndex,
            std::static_pointer_cast<Component>(prototype), resourceTag);
    }
    template <typename T>
    Shared<T> Instantiate(const wstring& prototypeTag, uint32 levIndex, void* arg = nullptr) {
		static_assert(std::is_base_of_v<GameObject, T> || std::is_base_of_v<Component, T>);
		if constexpr (std::is_base_of_v<GameObject, T>)
			return std::static_pointer_cast<T>(Instantiate_GameObjectByTag(prototypeTag, levIndex, arg));
		else
			return std::static_pointer_cast<T>(Instantiate_ComponentByTag(prototypeTag, levIndex, arg));
    }
    template <typename T>
    Shared<T> Instantiate(uint32 levIndex = UINT_MAX, void* arg = nullptr) {
		static_assert(std::is_base_of_v<GameObject, T> || std::is_base_of_v<Component, T>);
		const RuntimeTypeId runtimeTypeId = Make_RuntimeTypeId<T>();
		if constexpr (std::is_base_of_v<GameObject, T>)
			return std::static_pointer_cast<T>(Instantiate_GameObjectByRuntimeTypeId(
				runtimeTypeId, levIndex, arg));
		else
			return std::static_pointer_cast<T>(Instantiate_ComponentByRuntimeTypeId(
				runtimeTypeId, levIndex, arg));
    }
    Shared<GameObject> Instantiate_GameObject(std::string_view registeredName,
        uint32 levIndex = UINT_MAX, void* arg = nullptr, ObjectGuid objectGuid = {}) const;
	Bool Can_InstantiateGameObject(std::string_view registeredName,
		uint32 levIndex = UINT_MAX) const;
	Bool Can_InstantiateComponent(std::string_view registeredName,
		uint32 levIndex = UINT_MAX) const;
    Shared<Object> Instantiate(const wstring& prototypeTag, uint32 levIndex = UINT_MAX, void* arg = nullptr) const {
		return Instantiate_ComponentByTag(prototypeTag, levIndex, arg);
    }
private: /* Internal Implementation (Non-Template) */
    HRESULT Add_TypePrototype_Internal(uint32 levIndex, const Shared<Object>& object) const;
	HRESULT Register_EngineComponentPrototypes() const;
    HRESULT Add_ResourceComponentPrototype_Internal(uint32 levIndex,
        const Shared<Component>& component, const wstring& resourceTag) const;
	Shared<GameObject> Instantiate_GameObjectByRuntimeTypeId(RuntimeTypeId runtimeTypeId,
		uint32 levIndex, void* arg = nullptr, ObjectGuid objectGuid = {}) const;
	Shared<Component> Instantiate_ComponentByRuntimeTypeId(RuntimeTypeId runtimeTypeId,
		uint32 levIndex, void* arg = nullptr) const;
	Shared<GameObject> Instantiate_GameObjectByTag(const wstring& prototypeTag,
		uint32 levIndex, void* arg = nullptr) const;
	Shared<Component> Instantiate_ComponentByTag(const wstring& prototypeTag,
		uint32 levIndex, void* arg = nullptr) const;
	Shared<GameObject> Instantiate_GameObjectPrototype(const Shared<GameObject>& prototype,
		uint32 levIndex, void* arg = nullptr, ObjectGuid objectGuid = {}) const;
	Shared<Component> Instantiate_ComponentPrototype(const Shared<Component>& prototype,
		void* arg = nullptr) const;
    vector<Shared<GameObject>> FindAll_Internal(RuntimeTypeId runtimeTypeId) const;

private:
    Shared<LayerRegistry> m_LayerRegistry = {nullptr};
    Shared<TagRegistry> m_TagRegistry = {nullptr};

    Unique<GraphicDevice> m_GraphicDevice = {nullptr};
    Unique<TimeManager> m_TimeManager = {nullptr};
    Unique<JobManager> m_JobManager = {nullptr};
    Unique<InputDevice> m_InputDevice = {nullptr};
    Unique<Pipeline> m_Pipeline = {nullptr};
    Unique<LevelManager> m_LevelManager = {nullptr};
    Unique<PrototypeManager> m_PrototypeManager = {nullptr};
    Unique<Registry> m_Registry = {nullptr};
    Unique<ObjectManager> m_ObjectManager = {nullptr};
    Unique<CameraManager> m_CameraManager = {nullptr};
    Unique<ResourceManager> m_ResourceManager = {nullptr};
    Unique<Renderer> m_Renderer = {nullptr};
    Unique<LightManager> m_LightManager = {nullptr};
    Unique<LevelSerializer> m_LevelSerializer = {nullptr};
	Unique<PrefabManager> m_PrefabManager = {nullptr};
    Unique<FontManager> m_FontManager = { nullptr };
    Unique<SoundManager> m_SoundManager = { nullptr };
    Unique<EventManager> m_EventManager = { nullptr };
    Unique<CollisionManager> m_CollisionManager = { nullptr };
    Unique<PhysicsManager> m_PhysicsManager = { nullptr };
#ifdef _DEBUG
    Unique<DebugRenderer> m_DebugRenderer = { nullptr };
#endif
    Unique<RenderTargetManager> m_RenderTargetManager = { nullptr };
    Unique<NavigationBuilder> m_NavigationBuilder = { nullptr };

    uint64_t m_RuntimeFrameCount{};
    uint32 m_LastRuntimeFixedStepCount{};
    Float m_LastRuntimeDelta{};


public:
    // 직전 프레임 렌더 통계(draw·삼각형·묶인 인스턴스·컬링·가림·LOD 타일). Release Launcher F1 표시도 읽는다.
    void Add_CulledMesh() const;
    uint32 Get_FrameCulledCount() const;
    uint32 Get_FrameOccludedCount() const;
    void Add_LodTile() const;
    uint32 Get_FrameLodTileCount() const;
    void Add_DrawStats(uint32 triangleCount) const;
    void Add_InstancedProps(uint32 instanceCount) const;
    uint32 Get_FrameDrawCount() const;
    uint64_t Get_FrameTriangleCount() const;
    uint32 Get_FrameInstanceCount() const;

#ifdef _DEBUG /* For Debug Function */
public:
    void Fail_NextViewResize_Debug(Bool offscreen) const;
    // Debug 빌드의 디버그 그리기(선, 반투명 면). Engine 디버그 그리기와 Editor 도구가 같이 쓴다.
    DebugRenderer* Get_DebugRenderer() const { return m_DebugRenderer.get(); }
    void Render_CollisionDebug() const;
    Bool Toggle_RenderDebug() const;
    Bool Get_RenderDebug() const;
    Bool Toggle_RenderTargetDebug() const;
    Bool Get_RenderTargetDebug() const;
    uint32 Get_FrameCascadeDraws(uint32 cascade) const;
    Float Get_FrameCascadeMs(uint32 cascade) const;
    uint32 Get_FrameShadowBakes() const;
    HRESULT Ready_RenderTarget_Debug(const wstring& renderTargetTag, Float x, Float y, Float sizeX, Float sizeY) const;
	HRESULT Render_RenderTarget_Debug(const Shared<class VIBuffer_Rect>& buffer, const Shared<class Shader>& shader, const wstring& multiRenderTargetTag) const;

#endif

};

NS_END
