#include "Component.h"
#include "SpdLogger.h"
#include "String_Helper.h"

Component::Component()
{
}

Component::Component(const ComPtr<ID3D11Device> &pDevice, const ComPtr<ID3D11DeviceContext> &context)
    : m_Device(pDevice), m_Context(context) {}

Component::Component(const Component& prototype)
    : Component{prototype.m_Device, prototype.m_Context}
{
	m_ObjectName = prototype.m_ObjectName;
    m_RuntimeTypeId = prototype.m_RuntimeTypeId;
    m_TypeAncestors = prototype.m_TypeAncestors;
}

HRESULT Component::Initialize_Prototype() {
	return __super::Initialize_Prototype();
}

HRESULT Component::Initialize(void *arg) {
  return __super::Initialize(arg);
}
