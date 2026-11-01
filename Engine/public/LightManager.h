#pragma once
#include "EngineManager.h"

NS_BEGIN(Engine)

class Light;
class Shader;
class VIBuffer_Rect;

class LightManager final : public EngineManager
{
	NO_COPY(LightManager)
public:
	explicit LightManager();
	~LightManager() override = default;

public:
	const LIGHT_DESC* Get_LightDesc(uint32 index) const;
	/* First active light of the type, or nullptr; the same lights Render_Lights draws. */
	const LIGHT_DESC* Find_ActiveLight(LIGHT type) const;
	/* Lights are owned by the scene object that placed them; it adds and removes its own. */
	HRESULT Add_Light(const Shared<Light>& light);
	HRESULT Remove_Light(const Shared<Light>& light);
	HRESULT Clear_Lights();

public:
	HRESULT Render_Lights(const Shared<Shader>& shader, const Shared<VIBuffer_Rect>& buffer);

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	void On_Destroy() override;

public:
	vector<Shared<Light>>	m_Lights;

public:
	static Unique<LightManager> Create();

};

NS_END