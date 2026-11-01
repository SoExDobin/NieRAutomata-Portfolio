#include "Collider.h"
#include "Game.h"

Collider::Collider() : Component{} {}
Collider::Collider(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: Component{device, context} {}
Collider::Collider(const Collider& rhs)
	: Component{rhs} {}

Collider::~Collider()
{
	
}

void Collider::On_Destroy()
{
	if (GAME_INSTANCE)
	{
		GAME_INSTANCE->Remove_Collider(static_pointer_cast<Collider>(shared_from_this()));
	}
	Component::On_Destroy();
}
void Collider::On_Disable()
{
	m_OverlapMembers.clear();
	Component::On_Disable();
}

HRESULT Collider::Initialize_Prototype()
{
	return Component::Initialize_Prototype();
}

HRESULT Collider::Initialize(void* arg)
{
	if (arg)
	{
		auto desc = *static_cast<COLLIDER_DESC*>(arg);
		m_Offset = desc.offset;
	}

	return Component::Initialize(arg);
}

void Collider::Update(const Matrix& worldMatrix)
{
}

#ifdef _DEBUG

HRESULT Collider::Render_Debug(const Shared<PrimitiveBatch<VertexPositionColor>>& batch, const Color& color)
{
	return S_OK;
}

#endif
