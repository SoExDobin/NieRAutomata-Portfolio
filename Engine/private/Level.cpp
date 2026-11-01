#include "Level.h"

Level::Level(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: m_Device(device), m_Context(context)
{
}

void Level::Update_Level(Float timeDelta)
{
}

HRESULT Level::Render_Level()
{
	return S_OK;
}
