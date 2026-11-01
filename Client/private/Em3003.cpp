#include "pch.h"
#include "Em3003.h"
#include <SpdLogger.h>

Em3003::Em3003(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: Em3000Parts{device, context} { }
Em3003::Em3003(const Em3003& rhs)
	: Em3000Parts{ rhs } { }

HRESULT Em3003::Initialize_Prototype()
{
	if (FAILED(Em3000Parts::Initialize_Prototype()))
	{
		LOG_ERROR(L"Failed Initialize_Prototype Em3003");
		return E_FAIL;
	}

	return S_OK;
}

HRESULT Em3003::Initialize(void* arg)
{
	if (FAILED(Em3000Parts::Initialize(arg)))
	{
		LOG_ERROR(L"Failed Initialize Em3003");
		return E_FAIL;
	}

	return S_OK;
}

void Em3003::On_Destroy()
{
	Em3000Parts::On_Destroy();
}

void Em3003::Priority_Update(Float timeDelta)
{
	
}

void Em3003::Update(Float timeDelta)
{
	Em3000Parts::Update(timeDelta);
}

void Em3003::Late_Update(Float timeDelta)
{
	Em3000Parts::Late_Update(timeDelta);
}

void Em3003::Fixed_Update(Float fixedDelta)
{
	
}

HRESULT Em3003::Render()
{
	return Em3000Parts::Render();
}

void Em3003::Submit_RenderGroup()
{
	Em3000Parts::Submit_RenderGroup();
}

Shared<Em3003> Em3003::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<Em3003>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Em3003");
		return nullptr;
	}

	return prototype;
}

Shared<GameObject> Em3003::Clone(void* arg)
{
	auto instance = make_shared<Em3003>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : Em3003");
		return nullptr;
	}

	return instance;
}


