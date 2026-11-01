#pragma once
#include "EditorCamera.h"
#include "Engine_Reflection.h"

NS_BEGIN(Engine)
class Camera;
class GameObject;
class Object;
NS_END

NS_BEGIN(Editor)

class Inspector;
class EditorView;
class MenuBar;
class AssetBrowser;
class LogConsole;
class Hierarchy;
class ModelViewer;
class AnimationPresetEditor;
class NavHelper;

class EditorCamera;

class EditorManager {
  DECLARE_SINGLETON(EditorManager)
public:
    typedef struct tagResizeInfo
    {
        uint32 width{};
        uint32 height{};
        uint32 screenIndex{};
    } RESIZE_INFO;
public:
  EditorManager();
  ~EditorManager();

public:
  HRESULT Initialize();
  void Update(Bool IsResetView);
  HRESULT Render(Bool IsResetView);

public:
  EDITOR_STATE Get_State() const { return m_State; }
  void Set_State(EDITOR_STATE state);
  void Request_SingleStep();

public: /* Frame-safe structural mutations */
  void Queue_Destroy(ObjectGuid targetGuid, uint32 levelIndex);
  void Queue_MoveToRoot(ObjectGuid targetGuid, uint32 levelIndex);
  void Queue_Reparent(ObjectGuid targetGuid, ObjectGuid parentGuid, uint32 levelIndex,
	  size_t targetIndex = numeric_limits<size_t>::max());
  void Queue_Reorder(ObjectGuid targetGuid, size_t targetIndex, uint32 levelIndex);
  void Queue_Spawn(const wstring& prototypeTag, uint32 levelIndex, ObjectGuid parentGuid = {},
	  size_t targetIndex = numeric_limits<size_t>::max(),
	  optional<Vector3> spawnPosition = nullopt);
	/* Instantiates the Prefab document named prefabName (its file stem) as a new root. openForEdit makes it that
	   Prefab's edit copy instead: it keeps the document's name, Inspector applies it back to the document, and the
	   level is not saved while it exists. Only the level the document was saved in opens it, because another level
	   may load stand-in models (ModelSettings "replaces") whose tags would be written into the document. */
	void Queue_SpawnPrefab(const wstring& prefabName, uint32 levelIndex,
	  optional<Vector3> spawnPosition = nullopt, Bool openForEdit = false);
	void Queue_Duplicate(ObjectGuid targetGuid, uint32 levelIndex);
	void Queue_Rename(ObjectGuid targetGuid, uint32 levelIndex, const wstring& newName);
	void Queue_PropertyWrite(const Shared<Engine::GameObject>& owner, Engine::Object& target,
		std::string_view propertyName, const ReflectionValue& before,
		const ReflectionValue& after, Bool beginGesture);
	void Queue_Undo();
	void Queue_Redo();
	void Clear_History();
	Bool Can_Undo() const { return !m_UndoHistory.empty(); }
	Bool Can_Redo() const { return !m_RedoHistory.empty(); }
	Bool Can_EditHierarchy(const Shared<Engine::GameObject>& object) const;
	Bool Can_EditChildren(const Shared<Engine::GameObject>& object) const;

public: /* Prefab edit copies (Asset Browser double-click) */
	// The Prefab a live root is the edit copy of, or an invalid guid.
	PrefabGuid Get_PrefabEdit(const Shared<Engine::GameObject>& root) const;
	// Writes the edit copy back over its Prefab document (backed up first).
	void Apply_PrefabEdit(const Shared<Engine::GameObject>& root) const;
	// Backs the document up and removes it; refused while its edit copy is open.
	void Delete_Prefab(PrefabGuid prefabGuid) const;
	// False, with a warning, while an edit copy is in the level: Apply or Close it first.
	Bool Can_SaveLevel() const;

public:
    Shared<EditorCamera> Get_EditorCamera() const { return m_EditorCamera; }
    /* View/Proj the Scene View image was rendered with this frame. The shared pipeline is
       rebound to the in-game camera afterwards, so gizmo and picking must use these. */
    const Matrix& Get_SceneViewMatrix() const { return m_SceneViewMatrix; }
    const Matrix& Get_SceneProjMatrix() const { return m_SceneProjMatrix; }
    Shared<Engine::Camera> Get_InGameCamera() const { return m_InGameCamera; }
    void Set_InGameCamera(const Shared<Engine::Camera> &camera) {
		m_InGameCamera = camera;
    }

    Shared<ModelViewer> Get_ModelViewer() const { return m_ModelViewer; }
	Shared<AnimationPresetEditor> Get_AnimationPresetEditor() const { return m_AnimationPresetEditor; }
    Shared<NavHelper> Get_NavHelper() const { return m_NavHelper; }
    Shared<MenuBar> Get_MenuBar() const { return m_MenuBar; }

public:
    const map<uint32, RESIZE_INFO>& Get_ResizeRequests() const { return m_ResizeRequests; }
    void RequestResize(Float width, Float height, uint32 screenIndex);
    void Clear_ResizeRequest(uint32 screenIndex) { m_ResizeRequests.erase(screenIndex); }

public: /* Selected Object (Hierarchy <-> Inspector 공유) */
  Shared<GameObject> Get_SelectedObject() const {
    return m_SelectedObject.lock();
  }
  void Set_SelectedObject(const Shared<GameObject>& obj) {
    m_SelectedObject = obj;
  }
  void Clear_SelectedObject() { m_SelectedObject.reset(); }
  /* Moves the editor camera so the object and its children fill the Scene View. */
  void Focus_Object(const Shared<GameObject>& obj);
  
public: /* Reset Request */
    void Request_Reset(uint32 startLevel) {
		Clear_History();
		m_PendingMutations.clear();
		m_PrefabEdits.clear();
		Clear_SelectedObject();
        m_IsResetRequested = true;
        m_EngineDesc.startLevel = startLevel;
    }
    Bool Is_ResetRequested() const { return m_IsResetRequested; }
    void Clear_ResetRequest() { m_IsResetRequested = false; }

    ENGINE_DESC& Get_EngineDesc() { return m_EngineDesc; }
    void Set_EngineDesc(const ENGINE_DESC& desc)
    {
        m_EngineDesc = desc;}

public: /* Auto Load Settings */
    Bool Is_AutoLoadEnabled() const { return m_IsAutoLoad; }
    void Set_AutoLoadEnabled(Bool isEnabled) { m_IsAutoLoad = isEnabled; }

    Bool Is_AutoLoadRequested() const { return m_IsAutoLoadRequested; }
    void Request_AutoLoad(Bool isRequested) { m_IsAutoLoadRequested = isRequested; }

private:
    enum class MUTATION_TYPE
    {
        DESTROY,
        MOVE_TO_ROOT,
        REPARENT,
		REORDER,
        SPAWN,
		DUPLICATE,
		PROPERTY_WRITE,
		RENAME,
		UNDO,
		REDO,
    };

    struct MUTATION_COMMAND
    {
        MUTATION_TYPE type{};
        ObjectGuid targetGuid{};
        ObjectGuid parentGuid{};
        uint32 levelIndex{};
        wstring prototypeTag;
		size_t targetIndex{ numeric_limits<size_t>::max() };
		optional<Vector3> spawnPosition;
		string targetRegisteredName;
		string propertyName;
		ReflectionValue beforeValue;
		ReflectionValue afterValue;
		Bool beginGesture{ true };
		wstring newName;
		Bool isPrefab{ false };		// SPAWN: prototypeTag names a Prefab document
		Bool openForEdit{ false };
    };

	enum class HISTORY_TYPE
	{
		ADD_SUBTREE,
		REMOVE_SUBTREE,
		MOVE,
		PROPERTY,
		RENAME,
	};

	struct HIERARCHY_PLACEMENT
	{
		ObjectGuid parentGuid{};
		size_t childIndex{ numeric_limits<size_t>::max() };
	};

	struct HISTORY_ENTRY
	{
		HISTORY_TYPE type{};
		ObjectGuid targetGuid{};
		uint32 levelIndex{};
		HIERARCHY_PLACEMENT before{};
		HIERARCHY_PLACEMENT after{};
		PrefabGuid snapshotGuid{};
		string snapshot;
		string targetRegisteredName;
		string propertyName;
		ReflectionValue beforeValue;
		ReflectionValue afterValue;
		wstring beforeName;
		wstring afterName;
	};

    void Flush_PendingMutations();
    Bool Apply_Mutation(const MUTATION_COMMAND& command);
	Bool Apply_Undo();
	Bool Apply_Redo();
	Bool Apply_History(const HISTORY_ENTRY& entry, Bool undo);
	Bool Capture_Subtree(const Shared<Engine::GameObject>& root,
		uint32 levelIndex, HISTORY_ENTRY& entry) const;
	Bool Restore_Subtree(const HISTORY_ENTRY& entry,
		Shared<Engine::GameObject>& outRoot) const;
	Bool Place_Object(const Shared<Engine::GameObject>& object,
		const HIERARCHY_PLACEMENT& placement) const;
	Shared<Engine::Object> Resolve_PropertyTarget(ObjectGuid ownerGuid,
		std::string_view targetRegisteredName) const;
	HIERARCHY_PLACEMENT Get_Placement(const Shared<Engine::GameObject>& object) const;
	void Record_History(HISTORY_ENTRY entry);

private:
    map<uint32, RESIZE_INFO> m_ResizeRequests;

    EDITOR_STATE m_State = EDITOR_STATE::STOP;
    Bool m_SingleStepRequested{ false };
    vector<MUTATION_COMMAND> m_PendingMutations;
	vector<HISTORY_ENTRY> m_UndoHistory;
	vector<HISTORY_ENTRY> m_RedoHistory;
	// Edit-copy root -> its Prefab. Entries outlive a Close so Undo brings the copy back as an edit copy.
	vector<pair<ObjectGuid, PrefabGuid>> m_PrefabEdits;
    Shared<EditorCamera> m_EditorCamera{nullptr};
    Matrix m_SceneViewMatrix{};
    Matrix m_SceneProjMatrix{};
    Shared<Engine::Camera> m_InGameCamera{nullptr};
    Weak<Engine::GameObject> m_SelectedObject = {};

    Bool m_IsResetRequested{ false };
    ENGINE_DESC m_EngineDesc = {};

    Bool m_IsAutoLoad{ true };
    Bool m_IsAutoLoadRequested{ false };

private:
    Shared<Inspector> m_Inspector = {nullptr};
    Shared<EditorView> m_EditorView = {nullptr};
    Shared<MenuBar> m_MenuBar = {nullptr};
    Shared<AssetBrowser> m_AssetBrowser = {nullptr};
    Shared<LogConsole> m_LogConsole = {nullptr};
    Shared<Hierarchy> m_Hierarchy = {nullptr};
    Shared<ModelViewer> m_ModelViewer = {nullptr};
	Shared<AnimationPresetEditor> m_AnimationPresetEditor = {nullptr};
    Shared<NavHelper> m_NavHelper = {nullptr};
};

NS_END
