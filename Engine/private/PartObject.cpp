#include "PartObject.h"

PartObject::PartObject() : GameObject{} {}
PartObject::PartObject(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context) 
	: GameObject{device, context } {}
PartObject::PartObject(const PartObject& rhs)
	: GameObject{ rhs } {}
HRESULT PartObject::Initialize_Prototype()
{
	return GameObject::Initialize_Prototype();
}

HRESULT PartObject::Initialize(void* arg)
{
	if (FAILED(GameObject::Initialize(arg)))
		return E_FAIL;

	return S_OK;
}
