#pragma once
#include "Object.h"

NS_BEGIN(Engine)

class Shader;
class VIBuffer_Rect;

class ENGINE_DLL Light final : public Object
{
	NO_COPY(Light)
public:
	Light();
	~Light() override = default;

public:
	const LIGHT_DESC& Get_LightDesc() const { return m_LightDesc; }
	void Set_LightDesc(const LIGHT_DESC& desc) { m_LightDesc = desc; }

public:
	HRESULT Initialize_Prototype(const LIGHT_DESC& desc);
	HRESULT Initialize(void* arg) override;

	void On_Destroy() override { Object::On_Destroy(); }
	void On_Disable() override { Object::On_Disable(); }
	void On_Enable() override { Object::On_Enable(); }
	void Set_Active(Bool isActive) override { Object::Set_Active(isActive); }

public:
	HRESULT Render(const Shared<Shader>& shader, const Shared<VIBuffer_Rect>& buffer);

private:
	LIGHT_DESC m_LightDesc = {};

public:
	static Shared<Light> Create(const LIGHT_DESC& desc);
};

NS_END
