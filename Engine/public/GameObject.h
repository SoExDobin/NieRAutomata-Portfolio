#pragma once
#include "Transform.h"
#include "LayerRegistry.h"
#include "Object.h"
#include "ScriptComponent.h"
#include "TagRegistry.h"
#include "String_Helper.h"

NS_BEGIN(Engine)
class Game;
class LevelSerializer;
class Component;
class ScriptComponent;
class Transform;
class Collider;
class ENGINE_DLL GameObject abstract : public Object, public enable_shared_from_this<GameObject> {
public:
	typedef struct tagGameObjectDesc : public OBJECT_DESC {} GAMEOBJECT_DESC;

public:
    explicit GameObject();
    explicit GameObject(const ComPtr<ID3D11Device> &pDevice, const ComPtr<ID3D11DeviceContext> &context);
    explicit GameObject(const GameObject &prototype);
    virtual ~GameObject() override = default;

    operator Shared<Transform>() const { return m_Transform; }
	Shared<Transform> Get_Transform() const { return m_Transform; }
	ObjectGuid Get_ObjectGuid() const { return m_ObjectGuid; }
	/* The level this object was placed in; code-made children, events and spawns go to the same level. */
	uint32 Get_LevelIndex() const { return m_LevelIndex; }
	// Runtime-owned objects may use Prefab assets without becoming authored Scene objects.
	virtual Bool Is_SceneSerializable() const { return true; }

public:
    LAYER_MASK &Get_LayerMask() { return m_LayerMask; }
    TAG_MASK &Get_TagMask() { return m_TagMask; }

public:
    virtual GAMEOBJECTTYPE Get_GameObjectType() { return GAMEOBJECTTYPE::GAMEOBJECT; }
    virtual HRESULT Initialize_Prototype() override;
    virtual HRESULT Initialize(void *arg) override;
    void On_Destroy() override;
    void On_Enable() override;
    virtual void On_Disable() override;
    void Set_Active(Bool isActive) final;

public:
    virtual void Priority_Update(Float timeDelta);
    virtual void Update(Float timeDelta);
    virtual void Late_Update(Float timeDelta);
    virtual void Fixed_Update(Float fixedDelta);
    virtual HRESULT Render();
    virtual void Submit_RenderGroup();

public: // 충돌 함수
    virtual void OnCollisionEnter(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) {};
    virtual void OnCollisionStay(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) {};
    virtual void OnCollisionExit(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) {};

protected:
    ComPtr<ID3D11Device> m_Device = {nullptr};
    ComPtr<ID3D11DeviceContext> m_Context = {nullptr};
    Shared<Transform> m_Transform = {nullptr};
    LayerMask m_LayerMask{};
    TagMask m_TagMask{};
    ObjectGuid m_ObjectGuid{};
    uint32 m_LevelIndex{ UINT_MAX };

protected: /* Parent Child */
    Weak<GameObject> m_Parent = {};
    vector<Shared<GameObject>> m_Children;
    wstring m_StableChildKey;

public:
    Bool Has_Parent() const { return !m_Parent.expired(); }
    HRESULT Set_Parent(const Shared<GameObject> &parent);
    HRESULT Set_Parent(const Shared<GameObject>& parent, const wstring& stableChildKey, size_t insertIndex = numeric_limits<size_t>::max());
    HRESULT Remove_Parent();
    HRESULT Add_Child(const Shared<GameObject> &child);
    HRESULT Add_Child(const Shared<GameObject>& child, const wstring& stableChildKey, size_t insertIndex = numeric_limits<size_t>::max());
    HRESULT Add_Child(uint32 prototypeLevIndex, const wstring& registeredName,
        const wstring& stableChildKey, void* arg = nullptr);
    HRESULT Remove_Child(const Shared<GameObject> &child);
	HRESULT Reorder_Child(const Shared<GameObject>& child, size_t targetIndex);
	HRESULT Get_ChildIndex(ObjectGuid childGuid, size_t& outIndex) const;
    Shared<GameObject> Get_Parent() const;
    const vector<Shared<GameObject>> &Get_Children() const;
    Shared<GameObject> Find_Child(ObjectGuid objectGuid) const;
    Shared<GameObject> Find_Child(const wstring& stableChildKey) const;
    const wstring& Get_StableChildKey() const { return m_StableChildKey; }
    void Destroy_Subtree();

protected: /* Component */
	map<RuntimeTypeId, Shared<Component>> m_Components;
	map<RuntimeTypeId, Shared<ScriptComponent>> m_Scripts;

public:
    vector<Shared<Component>> Get_Components();
    vector<Shared<ScriptComponent>> Get_Scripts();
    Bool Has_Component(RuntimeTypeId runtimeTypeId) const;
    HRESULT Add_Component(const Shared<Component> &component);
    Shared<Component> Add_Component(uint32 levIndex, const wstring& prototypeTag, void* arg = nullptr);
	Shared<Component> Add_Component_Reflected(uint32 levIndex, RuntimeTypeId runtimeTypeId,
		void* arg = nullptr);

    template <typename T>
    Shared<T> Add_Component(uint32 levIndex = 0, void* arg = nullptr)
    {
		return static_pointer_cast<T>(Add_Component_Reflected(levIndex,
			Make_RuntimeTypeId<T>(), arg));
    }

    template <typename T>
    Shared<T> Add_Component_Tag(uint32 levIndex, const wstring& prototypeTag, void* arg = nullptr)
    {
        return static_pointer_cast<T>(Add_Component(levIndex, prototypeTag, arg));
    }

public:
     template <typename T> requires is_base_of_v<Component, T>
	Shared<T> Get_Component() {
		// final 타입은 자기 RuntimeTypeId 키로만 저장되므로 맵 조회 한 번으로 찾는다. 컴포넌트마다 Is_A를 확인하지 않는다.
		if constexpr (std::is_final_v<T>) {
			constexpr RuntimeTypeId runtimeTypeId = Make_RuntimeTypeId<T>();
			if (const auto it = m_Components.find(runtimeTypeId); it != m_Components.end())
				return static_pointer_cast<T>(it->second);
			if constexpr (is_base_of_v<ScriptComponent, T>) {
				if (const auto it = m_Scripts.find(runtimeTypeId); it != m_Scripts.end())
					return static_pointer_cast<T>(it->second);
			}
			return nullptr;
		}
		for (auto &[objectID, component] : m_Components) {
			if (Shared<T> typed = Cast<T>(component))
				return typed;
		}
		if constexpr (is_base_of_v<ScriptComponent, T> || is_base_of_v<T, ScriptComponent>) {
			for (auto &[objectID, script] : m_Scripts) {
				if (Shared<T> typed = Cast<T>(script))
					return typed;
			}
		}

        return nullptr;
    }
      	
public:
	virtual Shared<GameObject> Clone(void *arg) PURE;

private:
	friend class LevelSerializer;
	HRESULT Post_Load();

};

NS_END
