#pragma once
#include "Em3000Parts.h"

NS_BEGIN(Engine)
NS_END

NS_BEGIN(Client)

class CLIENT_DLL Em3002 final : public Em3000Parts
{
public:
	typedef struct tagEm3002Parts : public EM3000PART_DESC
	{

	} EM3002PART_DESC;

public:
	explicit Em3002() = default;
	explicit Em3002(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Em3002(const Em3002& rhs);
	~Em3002() override = default;

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	void On_Destroy() override;

public:
	void Priority_Update(Float timeDelta) override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	void Fixed_Update(Float fixedDelta) override;
	HRESULT Render() override;
	void Submit_RenderGroup() override;

public:
	static Shared<Em3002> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;

};

NS_END
