#include "pch.h"
#include "Em3002.h"
#include <SpdLogger.h>

Em3002::Em3002(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: Em3000Parts{ device, context } { }
Em3002::Em3002(const Em3002& rhs)
	: Em3000Parts{ rhs } { }

HRESULT Em3002::Initialize_Prototype()
{
	if (FAILED(Em3000Parts::Initialize_Prototype()))
	{
		LOG_ERROR(L"Failed Initialize_Prototype Em3002");
		return E_FAIL;
	}

	return S_OK;
}

HRESULT Em3002::Initialize(void* arg)
{
	if (FAILED(Em3000Parts::Initialize(arg)))
	{
		LOG_ERROR(L"Failed Initialize Em3002");
		return E_FAIL;
	}

	return S_OK;
}

void Em3002::On_Destroy()
{
	Em3000Parts::On_Destroy();
}

void Em3002::Priority_Update(Float timeDelta)
{
	
}

void Em3002::Update(Float timeDelta)
{
	Em3000Parts::Update(timeDelta);
}

void Em3002::Late_Update(Float timeDelta)
{
	Em3000Parts::Late_Update(timeDelta);
}

void Em3002::Fixed_Update(Float fixedDelta)
{
	
}

HRESULT Em3002::Render()
{
	return Em3000Parts::Render();
}

void Em3002::Submit_RenderGroup()
{
	Em3000Parts::Submit_RenderGroup();
}

Shared<Em3002> Em3002::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<Em3002>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Em3002");
		return nullptr;
	}

	return prototype;
}

Shared<GameObject> Em3002::Clone(void* arg)
{
	auto instance = make_shared<Em3002>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : Em3002");
		return nullptr;
	}

	return instance;
}


