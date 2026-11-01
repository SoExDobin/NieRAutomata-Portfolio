#include "Object.h"
#include "Game.h"
#include "SpdLogger.h"
#include "String_Helper.h"

Object::Object()
    : m_RuntimeObjectId(Create_RuntimeObjectId()) {}

Object::~Object()
{

}

HRESULT Object::Initialize_Prototype()
{
    string canonicalTypeKey;
    if (FAILED(GAME_INSTANCE->Find_ReflectedIdentity(*this, m_RuntimeTypeId, canonicalTypeKey, m_TypeAncestors)) ||
        m_RuntimeTypeId == 0) {
        LOG_ERROR(L"Object Initialize Failed By RuntimeTypeId");
        MSG_BOX("Object Initialize Failed By RuntimeTypeId");
        return E_FAIL;
    }

    m_ObjectName = Helper::To_wString(canonicalTypeKey);
    if (m_ObjectName.empty()) {
        LOG_ERROR(L"GameObject Initialize Failed By Set Object Name");
        MSG_BOX("GameObject Initialize Failed By Set Object Name");
        return E_FAIL;
    }
    return S_OK;
}

void Object::Set_Active(Bool isActive)
{
    if (m_IsActive == isActive) return;

    if (isActive)
        On_Enable();
    else
        On_Disable();

    m_IsActive = isActive;
}

Bool Object::Is_Active() const
{
    return m_IsActive;
}

void Object::Assign_ReflectedIdentity(RuntimeTypeId runtimeTypeId, std::string_view registeredName,
	const Shared<const vector<RuntimeTypeId>>& typeAncestors)
{
	m_RuntimeTypeId = runtimeTypeId;
	m_TypeAncestors = typeAncestors;
	m_ObjectName = Helper::To_wString(std::string{ registeredName });
}

