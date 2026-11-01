#pragma once
#include "EngineManager.h"

NS_BEGIN(Engine)

class GameObject;

class ObjectManager final : public EngineManager {
	NO_COPY(ObjectManager)
public:
	ObjectManager() = default;
	~ObjectManager() override = default;

public:
	uint32 Get_LayerBits() const { return m_LayerMask; }

public:
	HRESULT Initialize(void *arg) override;
	void On_Destroy() override;

public:
	void PriorityUpdate(Float timeDelta);
	void Update(Float timeDelta);
	void LateUpdate(Float timeDelta);
	void FixedUpdate(Float fixedDelta);
	void Submit_RenderGroup();
	void Cleanup_GameObjects(uint32 levIndex);

public:
	HRESULT Add_GameObject(uint32 levIndex, const Shared<GameObject>& object);
	HRESULT Remove_GameObject(uint32 levIndex, const Shared<GameObject>& object);
	HRESULT Clear_GameObjects(uint32 levIndex);
	HRESULT Clear_AllGameObjects();

	Shared<GameObject> Find_ByObjectGuid(ObjectGuid objectGuid) const;
	Shared<GameObject> Find_ByRuntimeObjectId(RuntimeObjectId runtimeObjectId) const;
	Bool Contains(uint32 levIndex, ObjectGuid objectGuid) const;
	HRESULT Find_Level(ObjectGuid objectGuid, uint32& outLevelIndex) const;
	vector<Shared<GameObject>> Find_AllByRuntimeTypeId(RuntimeTypeId runtimeTypeId) const;
	HRESULT Destroy(ObjectGuid objectGuid);
	const unordered_map<RuntimeObjectId, Shared<GameObject>>& Get_GameObjects(uint32 levIndex);
	Shared<GameObject> Find_ObjectByObjectTag(uint32 levIndex, const wstring& tagName);

	/* The level a GameObject being cloned will be added to. GameObject::Initialize takes it first,
	   before it creates children, so a scene-loaded object knows its level while the current level is still Loading. */
	void Prepare_InitializingLevel(uint32 levIndex) { m_InitializingLevel = levIndex; }
	uint32 Consume_InitializingLevel() noexcept { return std::exchange(m_InitializingLevel, UINT_MAX); }

private:
	uint32 m_LevelCount{};
	uint32 m_InitializingLevel{ UINT_MAX };
	vector<map<uint32, vector<Shared<GameObject>>>> m_ObjectByLayer;
	vector<unordered_map<RuntimeObjectId, Shared<GameObject>>> m_ObjectsByLevel;
	unordered_map<ObjectGuid, Weak<GameObject>, GuidHash> m_ObjectByGuid;
	unordered_map<RuntimeObjectId, Weak<GameObject>> m_ObjectByRuntimeId;
	uint32 m_LayerMask = {ETOI(LAYER::ALL_LAYER)};

public:
  static Unique<ObjectManager> Create(uint32 levCount);
};

NS_END
