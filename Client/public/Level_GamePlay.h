#pragma once
#include "Level.h"
#include "Client_Define.h"

NS_BEGIN(Client)

class CLIENT_DLL Level_GamePlay final : public Level
{
public:
	explicit Level_GamePlay(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	virtual ~Level_GamePlay() override = default;

public:
	virtual HRESULT Initialize_Prototype() override;
	virtual HRESULT Initialize(void* arg) override;
	virtual void On_Destroy() override;

public:
	virtual void Update_Level(Float timeDelta) override;
	virtual HRESULT Render_Level() override;

public:
	static Shared<Level_GamePlay> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
};

NS_END
