#include "ObjectManager.h"

#include "Game.h"
#include "GameObject.h"

HRESULT ObjectManager::Initialize(void *arg) {
	m_LayerMask = ETOI(LAYER::ALL_LAYER);
    m_LevelCount = *static_cast<uint32*>(arg);

	m_ObjectByLayer.resize(m_LevelCount);
	m_ObjectsByLevel.resize(m_LevelCount);

	return EngineManager::Initialize(arg);
}

void ObjectManager::On_Destroy() {
	Clear_AllGameObjects();
	EngineManager::On_Destroy();
}

void ObjectManager::PriorityUpdate(Float timeDelta) {
    auto curLevel = GAME_INSTANCE->Get_CurrentLevelIndex();
    const auto priorityUpdate = [timeDelta](auto&& self, const Shared<GameObject>& object) -> void {
        if (!object || object->Is_Destroy() || !object->Is_Active())
            return;
        object->Priority_Update(timeDelta);
        if (object->Is_Destroy())
            return;
        const vector<Shared<GameObject>> children = object->Get_Children();
        for (const auto& child : children)
            if (child && child->Get_Parent() == object) self(self, child);
    };

	for (auto& [layerBit, objects] : m_ObjectByLayer[0]) {
		for (auto& obj : objects) {
			if (obj && !obj->Has_Parent())
				priorityUpdate(priorityUpdate, obj);
		}
	}

	if (curLevel != 0) {
		for (auto& [layerBit, objects] : m_ObjectByLayer[curLevel]) {
			if ((m_LayerMask & layerBit) == 0) continue;
			for (size_t i = 0; i < objects.size(); ++i) {
				auto obj = objects[i];
				if (obj && !obj->Has_Parent())
					priorityUpdate(priorityUpdate, obj);
			}
		}
	}
}

void ObjectManager::Update(Float timeDelta) {
    auto curLevel = GAME_INSTANCE->Get_CurrentLevelIndex();
    const auto update = [timeDelta](auto&& self, const Shared<GameObject>& object) -> void {
        if (!object || object->Is_Destroy() || !object->Is_Active())
            return;
        object->Update(timeDelta);
        if (object->Is_Destroy())
            return;
        const vector<Shared<GameObject>> children = object->Get_Children();
        for (const auto& child : children)
            if (child && child->Get_Parent() == object) self(self, child);
    };

	for (auto& [layerBit, objects] : m_ObjectByLayer[0]) {
		for (auto& obj : objects) {
			if (obj && !obj->Has_Parent())
				update(update, obj);
		}
	}

	if (curLevel != 0) {
		for (auto& [layerBit, objects] : m_ObjectByLayer[curLevel]) {
			if ((m_LayerMask & layerBit) == 0) continue;
			for (size_t i = 0; i < objects.size(); ++i) {
				auto obj = objects[i];
				if (obj && !obj->Has_Parent())
					update(update, obj);
			}
		}
	}
}

void ObjectManager::LateUpdate(Float timeDelta) {
    auto curLevel = GAME_INSTANCE->Get_CurrentLevelIndex();
    const auto lateUpdate = [timeDelta](auto&& self, const Shared<GameObject>& object) -> void {
        if (!object || object->Is_Destroy() || !object->Is_Active())
            return;
        object->Late_Update(timeDelta);
        if (object->Is_Destroy())
            return;
        const vector<Shared<GameObject>> children = object->Get_Children();
        for (const auto& child : children)
            if (child && child->Get_Parent() == object) self(self, child);
    };

	for (auto& [layerBit, objects] : m_ObjectByLayer[0]) {
		for (auto& obj : objects) {
			if (obj && !obj->Has_Parent())
				lateUpdate(lateUpdate, obj);
		}
	}

	if (curLevel != 0) {
		for (auto& [layerBit, objects] : m_ObjectByLayer[curLevel]) {
			if ((m_LayerMask & layerBit) == 0) continue;
			for(size_t i = 0; i < objects.size(); ++i) {
				auto obj = objects[i];
				if (obj && !obj->Has_Parent())
					lateUpdate(lateUpdate, obj);
			}
		}
	}
}

void ObjectManager::FixedUpdate(Float fixedDelta) {
    auto curLevel = GAME_INSTANCE->Get_CurrentLevelIndex();
    const auto fixedUpdate = [fixedDelta](auto&& self, const Shared<GameObject>& object) -> void {
        if (!object || object->Is_Destroy() || !object->Is_Active())
            return;
        object->Fixed_Update(fixedDelta);
        if (object->Is_Destroy())
            return;
        const vector<Shared<GameObject>> children = object->Get_Children();
        for (const auto& child : children)
            if (child && child->Get_Parent() == object) self(self, child);
    };

	for (auto& [layerBit, objects] : m_ObjectByLayer[0]) {
		for (auto& obj : objects) {
			if (obj && !obj->Has_Parent())
				fixedUpdate(fixedUpdate, obj);
		}
	}

	if (curLevel != 0) {
		for (auto& [layerBit, objects] : m_ObjectByLayer[curLevel]) {
			if ((m_LayerMask & layerBit) == 0) continue;
			for (size_t i = 0; i < objects.size(); ++i) {
				auto obj = objects[i];
				if (obj && !obj->Has_Parent())
					fixedUpdate(fixedUpdate, obj);
			}
		}
	}
}

void ObjectManager::Submit_RenderGroup() {
    auto curLevel = GAME_INSTANCE->Get_CurrentLevelIndex();
    const auto submitRenderGroup = [](auto&& self, const Shared<GameObject>& object) -> void {
        if (!object || object->Is_Destroy() || !object->Is_Active())
            return;
        object->Submit_RenderGroup();
        if (object->Is_Destroy())
            return;
        const vector<Shared<GameObject>> children = object->Get_Children();
        for (const auto& child : children)
            if (child && child->Get_Parent() == object) self(self, child);
    };

	for (auto& [layerBit, objects] : m_ObjectByLayer[0]) {
		for (auto& obj : objects) {
			if (obj && !obj->Has_Parent())
				submitRenderGroup(submitRenderGroup, obj);
		}
	}

	if (curLevel != 0) {
		for (auto& [layerBit, objects] : m_ObjectByLayer[curLevel]) {
			if ((m_LayerMask & layerBit) == 0) continue;
			for (auto& obj : objects) {
				if (obj && !obj->Has_Parent())
					submitRenderGroup(submitRenderGroup, obj);
			}
		}
	}
}

void ObjectManager::Cleanup_GameObjects(uint32 levIndex) {
    if (levIndex >= m_LevelCount)
        return;

    vector<Shared<GameObject>> garbages;
    for (auto &[layerBit, objects] : m_ObjectByLayer[levIndex]) {
		for (auto &obj : objects) {
			if (obj && obj->Is_Destroy()) {
				garbages.push_back(obj);
			}
		}
    }

    for (const auto& garbage : garbages)
        Remove_GameObject(levIndex, garbage);
}

HRESULT ObjectManager::Add_GameObject(uint32 levIndex, const Shared<GameObject>& object) {
    if (levIndex >= m_LevelCount || !object || !object->Get_ObjectGuid().Is_Valid() ||
        object->Get_RuntimeObjectId() == 0) {
        return E_INVALIDARG;
    }

    if (const auto guidIt = m_ObjectByGuid.find(object->Get_ObjectGuid());
        guidIt != m_ObjectByGuid.end() && !guidIt->second.expired()) {
        return E_FAIL;
    }

    if (const auto runtimeIt = m_ObjectByRuntimeId.find(object->Get_RuntimeObjectId());
        runtimeIt != m_ObjectByRuntimeId.end() && !runtimeIt->second.expired()) {
        return E_FAIL;
    }

    if (m_ObjectsByLevel[levIndex].contains(object->Get_RuntimeObjectId())) {
        return E_FAIL;
    }

    m_ObjectByGuid[object->Get_ObjectGuid()] = object;
    m_ObjectByRuntimeId[object->Get_RuntimeObjectId()] = object;
    m_ObjectByLayer[levIndex][object->Get_LayerMask().Get_Layer()].push_back(object);
    m_ObjectsByLevel[levIndex].emplace(object->Get_RuntimeObjectId(), object);

    return S_OK;
}

HRESULT ObjectManager::Clear_GameObjects(uint32 levIndex) {
	if (levIndex >= m_LevelCount) return E_FAIL;

    for (auto &layer : m_ObjectByLayer[levIndex]) {
		for (auto &obj : layer.second) {
			if (!obj->Is_Destroy()) {
				obj->On_Destroy();
				obj->Mark_Destroyed();
			}
			m_ObjectByGuid.erase(obj->Get_ObjectGuid());
			m_ObjectByRuntimeId.erase(obj->Get_RuntimeObjectId());
		}
		layer.second.clear();
    }

	m_ObjectByLayer[levIndex].clear();
	m_ObjectsByLevel[levIndex].clear();

    m_LayerMask = ETOI(LAYER::ALL_LAYER);
    return S_OK;
}

HRESULT ObjectManager::Clear_AllGameObjects()
{
	for (uint32 i = 0; i < m_LevelCount; ++i)
	{
		for (auto& layer : m_ObjectByLayer[i]) {
			for (auto& obj : layer.second) {
				if (!obj->Is_Destroy()) {
					obj->On_Destroy();
					obj->Mark_Destroyed();
				}
				m_ObjectByGuid.erase(obj->Get_ObjectGuid());
				m_ObjectByRuntimeId.erase(obj->Get_RuntimeObjectId());
			}
			layer.second.clear();
		}
		m_ObjectByLayer[i].clear();
		m_ObjectsByLevel[i].clear();
	}

	m_LayerMask = ETOI(LAYER::ALL_LAYER);
	m_ObjectByGuid.clear();
	m_ObjectByRuntimeId.clear();
	return S_OK;
}

Shared<GameObject> ObjectManager::Find_ByObjectGuid(ObjectGuid objectGuid) const
{
    if (!objectGuid.Is_Valid())
        return nullptr;

    const auto it = m_ObjectByGuid.find(objectGuid);
    return it == m_ObjectByGuid.end() ? nullptr : it->second.lock();
}

Shared<GameObject> ObjectManager::Find_ByRuntimeObjectId(RuntimeObjectId runtimeObjectId) const
{
    if (runtimeObjectId == 0)
        return nullptr;

    const auto it = m_ObjectByRuntimeId.find(runtimeObjectId);
    return it == m_ObjectByRuntimeId.end() ? nullptr : it->second.lock();
}

Bool ObjectManager::Contains(uint32 levIndex, ObjectGuid objectGuid) const
{
    if (levIndex >= m_LevelCount || !objectGuid.Is_Valid())
        return false;

    const Shared<GameObject> object = Find_ByObjectGuid(objectGuid);
    return object && m_ObjectsByLevel[levIndex].contains(object->Get_RuntimeObjectId());
}

HRESULT ObjectManager::Find_Level(ObjectGuid objectGuid, uint32& outLevelIndex) const
{
	const Shared<GameObject> object = Find_ByObjectGuid(objectGuid);
	if (!object)
		return E_FAIL;

	for (uint32 levelIndex = 0; levelIndex < m_LevelCount; ++levelIndex) {
		if (m_ObjectsByLevel[levelIndex].contains(object->Get_RuntimeObjectId())) {
			outLevelIndex = levelIndex;
			return S_OK;
		}
	}

	return E_FAIL;
}

vector<Shared<GameObject>> ObjectManager::Find_AllByRuntimeTypeId(RuntimeTypeId runtimeTypeId) const
{
    vector<Shared<GameObject>> result;
    if (runtimeTypeId == 0)
        return result;

    result.reserve(m_ObjectByGuid.size());
    for (const auto& [objectGuid, weakObject] : m_ObjectByGuid) {
        if (const Shared<GameObject> object = weakObject.lock();
            object && !object->Is_Destroy() && object->Get_RuntimeTypeId() == runtimeTypeId) {
            result.push_back(object);
        }
    }
    std::ranges::sort(result, {}, [](const Shared<GameObject>& object) {
        return object->Get_ObjectGuid();
    });
    return result;
}

HRESULT ObjectManager::Destroy(ObjectGuid objectGuid)
{
    const Shared<GameObject> object = Find_ByObjectGuid(objectGuid);
    if (!object)
        return E_FAIL;
    if (object->Is_Destroy())
        return S_FALSE;

    object->Destroy_Subtree();
    return S_OK;
}

HRESULT ObjectManager::Remove_GameObject(uint32 levIndex, const Shared<GameObject>& object) {
    if (levIndex >= m_LevelCount || !object)
        return E_INVALIDARG;

    m_ObjectByGuid.erase(object->Get_ObjectGuid());
    m_ObjectByRuntimeId.erase(object->Get_RuntimeObjectId());
    m_ObjectsByLevel[levIndex].erase(object->Get_RuntimeObjectId());

    if (auto layerIt = m_ObjectByLayer[levIndex].find(object->Get_LayerMask().Get_Layer());
        layerIt != m_ObjectByLayer[levIndex].end()) {
        std::erase(layerIt->second, object);
        if (layerIt->second.empty())
            m_ObjectByLayer[levIndex].erase(layerIt);
    }

    object->On_Destroy();
	object->Mark_Destroyed();
    return S_OK;
}

const unordered_map<RuntimeObjectId, Shared<GameObject>>& ObjectManager::Get_GameObjects(uint32 levIndex) {
	return m_ObjectsByLevel[levIndex];
}

Shared<GameObject> ObjectManager::Find_ObjectByObjectTag(uint32 levIndex, const wstring& tagName)
{
	if (levIndex >= m_LevelCount)
		return nullptr;
	
	for (auto& pair : m_ObjectsByLevel[levIndex])
	{
		if (pair.second->Get_Name() == tagName)
		{
			return pair.second;
		}
	}
	return nullptr;
}

Unique<ObjectManager> ObjectManager::Create(uint32 levCount) {
	auto objectManager = make_unique<ObjectManager>();

	if (FAILED(objectManager->Initialize(&levCount))) {
		MSG_BOX("Failed To Create ObjectManager");
		return nullptr;
	}

	return objectManager;
}
