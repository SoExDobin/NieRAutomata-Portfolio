#pragma once
#include "GameObject.h"

NS_BEGIN(Engine)
class Shader;
class VISphere;
class Texture;
NS_END

NS_BEGIN(Client)

class CLIENT_DLL SkySphere final : public GameObject
{
public:
	explicit SkySphere();
	explicit SkySphere(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit SkySphere(const SkySphere& rhs);
	~SkySphere() override = default;
	
public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;

public:
	void Priority_Update(Float timeDelta) override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	void Fixed_Update(Float fixedDelta) override;
	HRESULT Render() override;
	void Submit_RenderGroup() override;

private:
	HRESULT Bind_ShaderResources();
	HRESULT Ready_Components();

private:
	Shared<Shader>	m_Shader{ nullptr };
	Shared<VISphere> m_SphereBuffer{ nullptr };
	Shared<Texture>	m_Texture{ nullptr };

public:
	static Shared<SkySphere> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;
};

NS_END
