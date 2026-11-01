#pragma once
#include "Engine_Define.h"
#include "Engine_Reflection.h"
#include <cassert>

NS_BEGIN(Engine)

class Registry;

class ENGINE_DLL Object abstract {
public:
	typedef struct tagObjectDesc{} OBJECT_DESC;
public:
	Object();
	virtual ~Object();

public:
	RuntimeTypeId Get_RuntimeTypeId() const { return m_RuntimeTypeId; }
	// Registered is-a without dynamic_cast: the registered type and its bases, assigned with the identity.
	template <typename T>
	Bool Is_A() const {
		constexpr RuntimeTypeId runtimeTypeId = Make_RuntimeTypeId<T>();
		return m_RuntimeTypeId == runtimeTypeId || (m_TypeAncestors &&
			std::ranges::find(*m_TypeAncestors, runtimeTypeId) != m_TypeAncestors->end());
	}
	RuntimeObjectId Get_RuntimeObjectId() const { return m_RuntimeObjectId; }
	const wstring &Get_Name() const { return m_ObjectName; }
	void Set_Name(const wstring& name) { m_ObjectName = name; }

public:
	virtual HRESULT Initialize_Prototype();
	virtual HRESULT Initialize(void *arg) { return S_OK; }
	virtual HRESULT Begin() { return S_OK; }

	Bool Is_Destroy() const { return m_IsDestroy; }
	virtual void On_Destroy() { return; }

	virtual void On_Disable() { return; }
	virtual void On_Enable() { return; }

	virtual void Set_Active(Bool isActive);
	Bool Is_Active() const;

	OBJECT_DESC* Get_ObjectDesc() const { return m_ObjectDesc; }

protected:
	void Mark_Destroyed() { m_IsDestroy = true; }
	OBJECT_DESC* m_ObjectDesc{};
	Bool m_IsDestroy = {false};
	Bool m_IsActive = {true};
	RuntimeTypeId m_RuntimeTypeId{};
	Shared<const vector<RuntimeTypeId>> m_TypeAncestors;
	RuntimeObjectId m_RuntimeObjectId{};
	wstring m_ObjectName = {};

private:
	friend class Registry;
	friend class ObjectManager;
	friend class GameObject;
	void Assign_ReflectedIdentity(RuntimeTypeId runtimeTypeId, std::string_view registeredName,
		const Shared<const vector<RuntimeTypeId>>& typeAncestors);
};

// dynamic_pointer_cast through the registered hierarchy (Unreal's Cast<T>). Debug checks it against C++.
template <typename T, typename U> requires is_base_of_v<Object, T> && is_base_of_v<Object, U>
Shared<T> Cast(const Shared<U>& object)
{
	Shared<T> result = object && object->template Is_A<T>() ? std::static_pointer_cast<T>(object) : nullptr;
	assert((result != nullptr) == (dynamic_cast<T*>(object.get()) != nullptr) &&
		"Cast<T>: registered bases disagree with C++ (unregistered T or wrong base name)");
	return result;
}

NS_END
