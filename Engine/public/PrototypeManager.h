#pragma once
#include "EngineManager.h"
#include "Object.h"
#include "String_Helper.h"

NS_BEGIN(Engine)

class GameObject;
class Component;

class PrototypeManager final : public EngineManager {
	NO_COPY(PrototypeManager)
public:
	explicit PrototypeManager() = default;
	~PrototypeManager() override = default;

public:
	HRESULT Initialize(void *arg) override;
	void On_Destroy() override;

public:
    HRESULT Add_TypePrototype(uint32 levIndex, const Shared<Object>& object);
    HRESULT Add_ResourceComponentPrototype(uint32 levIndex,
        const Shared<Component>& component, const wstring& resourceTag);
    Shared<GameObject> Find_DefaultGameObject(uint32 levIndex, RuntimeTypeId runtimeTypeId) const;
    Shared<Component> Find_DefaultComponent(uint32 levIndex, RuntimeTypeId runtimeTypeId) const;
    Shared<GameObject> Find_GameObject(uint32 levIndex, const wstring &prototypeTag) const;
    Shared<Component> Find_Component(uint32 levIndex, const wstring &prototypeTag) const;
    HRESULT Clear_Prototypes();
    HRESULT Clear_Prototypes(uint32 levIndex);

public: /* Read Only */
    const vector<unordered_map<wstring, Shared<GameObject>>>&
    Get_GameObjects() const { return m_GameObjects; }
    const vector<unordered_map<wstring, Shared<Component>>>&
    Get_Components() const { return m_Components; }

private:
    HRESULT Add_Prototype(uint32 levIndex, const Shared<Object>& object,
        const wstring& prototypeTag);

private:
    uint32 m_LevelCount = {};
    vector<unordered_map<wstring, Shared<GameObject>>> m_GameObjects;
    vector<unordered_map<wstring, Shared<Component>>> m_Components;
    vector<unordered_map<RuntimeTypeId, Shared<GameObject>>> m_DefaultGameObjects;
    vector<unordered_map<RuntimeTypeId, Shared<Component>>> m_DefaultComponents;
    mutable std::recursive_mutex m_PrototypeMutex;

public:
	static Unique<PrototypeManager> Create(uint32 levCount);

private: /* validation method*/
	Bool Validate_Level(uint32 levIndex) const;
};

NS_END
