#include "PrototypeManager.h"
#include "Game.h"
#include "GameObject.h"
#include "SpdLogger.h"
#include "ScriptComponent.h"

#include "Engine_Define.h"

HRESULT PrototypeManager::Initialize(void* arg) {
    m_LevelCount = arg == nullptr ? 0 : *static_cast<uint32*>(arg);

    m_GameObjects.resize(m_LevelCount);
    m_Components.resize(m_LevelCount);
    m_DefaultGameObjects.resize(m_LevelCount);
    m_DefaultComponents.resize(m_LevelCount);

    return S_OK;
}

void PrototypeManager::On_Destroy() {
    for (uint32 i = 0; i < m_LevelCount; ++i) {
        m_GameObjects[i].clear();
        m_Components[i].clear();
        m_DefaultGameObjects[i].clear();
        m_DefaultComponents[i].clear();
    }

    m_GameObjects.clear();
    m_Components.clear();
    m_DefaultGameObjects.clear();
    m_DefaultComponents.clear();
}

HRESULT PrototypeManager::Add_TypePrototype(uint32 levIndex, const Shared<Object>& object)
{
	return Add_Prototype(levIndex, object, L"");
}

HRESULT PrototypeManager::Add_ResourceComponentPrototype(uint32 levIndex,
	const Shared<Component>& component, const wstring& resourceTag)
{
	if (!component || resourceTag.empty())
		return E_INVALIDARG;
	return Add_Prototype(levIndex, component, resourceTag);
}

HRESULT PrototypeManager::Add_Prototype(uint32 levIndex, const Shared<Object>& object,
	const wstring& prototypeTag)
{
    std::lock_guard<std::recursive_mutex> lock(m_PrototypeMutex);

	if (!Validate_Level(levIndex)) {
		return E_FAIL;
	}

    if (!object)
        return E_INVALIDARG;
	if (object->Get_RuntimeTypeId() == 0 && FAILED(object->Initialize_Prototype())) {
		LOG_ERROR(L"Failed to initialize prototype identity");
		return E_FAIL;
	}

	const RuntimeTypeId runtimeTypeId = object->Get_RuntimeTypeId();
	const string registeredName = GAME_INSTANCE->Find_RegisteredName(runtimeTypeId);
    if (registeredName.empty() || runtimeTypeId == 0 ||
		GAME_INSTANCE->Find_RuntimeTypeId(registeredName) != runtimeTypeId) {
		LOG_ERROR(L"Registry rejected prototype identity: name={}, runtimeTypeId={}, objectName={}",
			Helper::To_wString(registeredName), runtimeTypeId, object->Get_Name());
        return E_FAIL;
    }

	const wstring registeredTag = Helper::To_wString(registeredName);
	const Bool isDefaultPrototype = prototypeTag.empty();
	const wstring resolvedTag = isDefaultPrototype ? registeredTag : prototypeTag;

	if (const Shared<GameObject> gameObject = dynamic_pointer_cast<GameObject>(object)) {
		if (!isDefaultPrototype) {
			LOG_ERROR(L"Named GameObject prototypes are not allowed: type={}, tag={}",
				registeredTag, resolvedTag);
			return E_INVALIDARG;
		}
		auto& prototypes = m_GameObjects[levIndex];
        auto& defaults = m_DefaultGameObjects[levIndex];
		const Bool defaultTypeExists = isDefaultPrototype && std::ranges::any_of(
			m_DefaultGameObjects, [runtimeTypeId](const auto& levelDefaults) {
				return levelDefaults.contains(runtimeTypeId);
			});
		if (prototypes.contains(resolvedTag) || defaultTypeExists)
            return S_FALSE;

        prototypes.emplace(resolvedTag, gameObject);
        if (isDefaultPrototype)
            defaults.emplace(runtimeTypeId, gameObject);
	}
	else if (const Shared<Component> component = dynamic_pointer_cast<Component>(object)) {
		if (!isDefaultPrototype && resolvedTag == registeredTag) {
			LOG_ERROR(L"Resource component tag must differ from its registered type name: {}",
				registeredTag);
			return E_INVALIDARG;
		}
		auto& prototypes = m_Components[levIndex];
        auto& defaults = m_DefaultComponents[levIndex];
		const Bool defaultTypeExists = isDefaultPrototype && std::ranges::any_of(
			m_DefaultComponents, [runtimeTypeId](const auto& levelDefaults) {
				return levelDefaults.contains(runtimeTypeId);
			});
		if (prototypes.contains(resolvedTag) || defaultTypeExists)
            return S_FALSE;

        prototypes.emplace(resolvedTag, component);
        if (isDefaultPrototype)
            defaults.emplace(runtimeTypeId, component);
    }
    else {
        LOG_ERROR(L"Unsupported prototype kind for {}", resolvedTag);
        return E_FAIL;
    }

    return S_OK;
}

HRESULT PrototypeManager::Clear_Prototypes()
{
    // 레벨 0 (Static/Engine)은 엔진의 핵심 데이터이므로 전체 삭제 시에도 보존합니다.
    // 일반적인 씬 전환이나 에디터 리셋 시에는 레벨 1 이상의 가용 레벨 데이터만 삭제합니다.
    for (uint32 i = 1; i < m_LevelCount; ++i) {
        m_GameObjects[i].clear();
        m_Components[i].clear();
        m_DefaultGameObjects[i].clear();
        m_DefaultComponents[i].clear();
    }

    return S_OK;
}

HRESULT PrototypeManager::Clear_Prototypes(uint32 levIndex) {
    if (!Validate_Level(levIndex)) {
      return E_FAIL;
    }

    m_GameObjects[levIndex].clear();
    m_Components[levIndex].clear();
    m_DefaultGameObjects[levIndex].clear();
    m_DefaultComponents[levIndex].clear();

    return S_OK;
}

Shared<GameObject> PrototypeManager::Find_DefaultGameObject(
	uint32 levIndex, RuntimeTypeId runtimeTypeId) const {
    std::lock_guard<std::recursive_mutex> lock(m_PrototypeMutex);
    if (!Validate_Level(levIndex) || runtimeTypeId == 0)
        return nullptr;

    const auto& defaults = m_DefaultGameObjects[levIndex];
    const auto it = defaults.find(runtimeTypeId);
    return it == defaults.end() ? nullptr : it->second;
}

Shared<Component> PrototypeManager::Find_DefaultComponent(
	uint32 levIndex, RuntimeTypeId runtimeTypeId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_PrototypeMutex);
    if (!Validate_Level(levIndex) || runtimeTypeId == 0)
        return nullptr;

    const auto& defaults = m_DefaultComponents[levIndex];
    const auto it = defaults.find(runtimeTypeId);
    return it == defaults.end() ? nullptr : it->second;
}

Shared<GameObject> PrototypeManager::Find_GameObject(
	uint32 levIndex, const wstring& prototypeTag) const
{
    std::lock_guard<std::recursive_mutex> lock(m_PrototypeMutex);
    if (!Validate_Level(levIndex) || prototypeTag.empty())
        return nullptr;

    const auto& prototypes = m_GameObjects[levIndex];
    const auto it = prototypes.find(prototypeTag);
    return it == prototypes.end() ? nullptr : it->second;
}

Shared<Component> PrototypeManager::Find_Component(
	uint32 levIndex, const wstring& prototypeTag) const
{
    std::lock_guard<std::recursive_mutex> lock(m_PrototypeMutex);
    if (!Validate_Level(levIndex) || prototypeTag.empty())
        return nullptr;

    const auto& prototypes = m_Components[levIndex];
    const auto it = prototypes.find(prototypeTag);
    return it == prototypes.end() ? nullptr : it->second;
}

Unique<PrototypeManager> PrototypeManager::Create(uint32 levCount) {
    auto prototypeManager = make_unique<PrototypeManager>();

    if (FAILED(prototypeManager->Initialize(&levCount))) {
          MSG_BOX("Failed To Create PrototypeManager");
          return nullptr;
    }

    return prototypeManager;
}

Bool PrototypeManager::Validate_Level(uint32 levIndex) const {
  if (levIndex >= m_LevelCount) {
    LOG_ERROR(L"{}: Out Of Level", m_ObjectName);
    MSG_BOX("Out Of Level Count");
    return false;
  }

  return true;
}
