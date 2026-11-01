#include "ScriptComponent.h"

ScriptComponent::ScriptComponent()
{
}

ScriptComponent::ScriptComponent(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: Component(device, context)
{
}

ScriptComponent::ScriptComponent(const ScriptComponent& rhs)
	: Component(rhs)
{
}

