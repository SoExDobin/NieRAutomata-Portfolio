#pragma once
#include "Pl0000Parts.h"

NS_BEGIN(Engine)
class SoftBody;
NS_END

NS_BEGIN(Client)

class CLIENT_DLL SheathWP0070Body final : public Pl0000Parts
{
public:
	explicit SheathWP0070Body() = default;
	explicit SheathWP0070Body(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit SheathWP0070Body(const SheathWP0070Body& rhs);
	~SheathWP0070Body() override = default;

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	void On_Enable() override;
	void On_Destroy() override;

public:
	void Priority_Update(Float timeDelta) override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	void Fixed_Update(Float fixedDelta) override;
	HRESULT Render() override;
	void Submit_RenderGroup() override;

private:
	HRESULT Ready_Components();
	HRESULT Bind_ShaderResources();

private:
	int32 m_RootBoneIndex{};
	Matrix m_LightSheathMatrix{};
	Shared<SoftBody> m_SoftBody{ nullptr };

public:
	static Shared<SheathWP0070Body> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;
};

NS_END
