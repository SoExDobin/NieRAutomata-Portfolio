#include "LightManager.h"
#include "Light.h"

LightManager::LightManager()
{
}

const LIGHT_DESC* LightManager::Get_LightDesc(uint32 index) const
{
	if (index >= m_Lights.size())
		return nullptr;

	return &m_Lights[index]->Get_LightDesc();
}

const LIGHT_DESC* LightManager::Find_ActiveLight(LIGHT type) const
{
	for (const auto& light : m_Lights)
		if (light && light->Is_Active() && light->Get_LightDesc().type == type)
			return &light->Get_LightDesc();
	return nullptr;
}

HRESULT LightManager::Add_Light(const Shared<Light>& light)
{
	if (!light)
		return E_INVALIDARG;
	if (ranges::find(m_Lights, light) != m_Lights.end())
		return S_FALSE;

	m_Lights.push_back(light);
	return S_OK;
}

HRESULT LightManager::Remove_Light(const Shared<Light>& light)
{
	const auto it = ranges::find(m_Lights, light);
	if (it == m_Lights.end())
		return S_FALSE;

	m_Lights.erase(it);
	return S_OK;
}

HRESULT LightManager::Clear_Lights()
{
	m_Lights.clear();
	return S_OK;
}

HRESULT LightManager::Render_Lights(const Shared<Shader>& shader, const Shared<VIBuffer_Rect>& buffer)
{
	for (auto& light : m_Lights)
	{
		if (light && light->Is_Active())
			light->Render(shader, buffer);
	}

	return S_OK;
}

HRESULT LightManager::Initialize_Prototype() { return EngineManager::Initialize_Prototype(); }
HRESULT LightManager::Initialize(void* arg) { return EngineManager::Initialize(arg); }
void LightManager::On_Destroy()
{
	EngineManager::On_Destroy();
	m_Lights.clear();
}

Unique<LightManager> LightManager::Create()
{
	auto lightManager = make_unique<LightManager>();

	if (FAILED(lightManager->Initialize_Prototype()))
	{
		MSG_BOX("Failed to create LightManager");
		return nullptr;
	}
	if (FAILED(lightManager->Initialize(nullptr)))
	{
		MSG_BOX("Failed to create LightManager");
		return nullptr;
	}

	return lightManager;
}
