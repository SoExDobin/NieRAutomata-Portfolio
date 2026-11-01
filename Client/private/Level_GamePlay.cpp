#include "pch.h"
#include "Level_GamePlay.h"
#include "Game.h"

NS_BEGIN(Client)

Level_GamePlay::Level_GamePlay(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: Level{ device, context }
{
}

HRESULT Level_GamePlay::Initialize_Prototype()
{
	return Level::Initialize_Prototype();
}

HRESULT Level_GamePlay::Initialize(void* arg)
{
	return Level::Initialize(arg);
}

void Level_GamePlay::On_Destroy()
{
	Level::On_Destroy();
}

void Level_GamePlay::Update_Level(Float timeDelta)
{
	Level::Update_Level(timeDelta);
}

HRESULT Level_GamePlay::Render_Level()
{
	return Level::Render_Level();
}

Shared<Level_GamePlay> Level_GamePlay::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto instance = make_shared<Level_GamePlay>(device, context);

	if (FAILED(instance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Create Prototype Level_GamePlay");
		return nullptr;
	}

	return instance;
}

NS_END
