#pragma once
#include "Em3000Parts.h"

NS_BEGIN(Engine)
NS_END

NS_BEGIN(Client)

class CLIENT_DLL Em3001 final : public Em3000Parts
{
public:
	typedef struct tagEm3001Parts : public EM3000PART_DESC
	{
		
	} EM3001PART_DESC;

public:
	explicit Em3001() = default;
	explicit Em3001(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Em3001(const Em3001& rhs);
	~Em3001() override = default;

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
	static Shared<Em3001> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;

};

NS_END
