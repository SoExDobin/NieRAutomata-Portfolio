#pragma once
#include "Pl0000Parts.h"

NS_BEGIN(Client)

class CLIENT_DLL SheathWP0220Body final : public Pl0000Parts
{
public:
	explicit SheathWP0220Body() = default;
	explicit SheathWP0220Body(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit SheathWP0220Body(const SheathWP0220Body& rhs);
	~SheathWP0220Body() override = default;

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

private:
	HRESULT Ready_Components();
	HRESULT Bind_ShaderResources();

private:
	int32 m_RootBoneIndex{};
	Matrix m_HeavySheathMatrix{};

public:
	static Shared<SheathWP0220Body> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;
};

NS_END
