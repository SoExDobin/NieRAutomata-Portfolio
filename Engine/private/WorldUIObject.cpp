#include "WorldUIObject.h"

#include "Game.h"

WorldUIObject::WorldUIObject() : UIObject{} {}
WorldUIObject::WorldUIObject(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: UIObject{device, context} {}
WorldUIObject::WorldUIObject(const WorldUIObject& rhs)
	: UIObject{rhs} {}

HRESULT WorldUIObject::Initialize_Prototype()
{
	return UIObject::Initialize_Prototype();
}

HRESULT WorldUIObject::Initialize(void* arg)
{
	return UIObject::Initialize(arg);
}

Bool WorldUIObject::To_Canvas(const Vector3& worldPosition, Vector2& outPosition)
{
	const Vector3 viewPosition = XMVector3TransformCoord(worldPosition, GAME_INSTANCE->Get_Transform(D3DTS::VIEW));
	if (viewPosition.z < 0.f)
		return false;
	const Vector3 clipPosition = XMVector3TransformCoord(viewPosition, GAME_INSTANCE->Get_Transform(D3DTS::PROJ));
	outPosition = Vector2{ (clipPosition.x * 0.5f + 0.5f) * 1920.f, (0.5f - clipPosition.y * 0.5f) * 1080.f };
	return true;
}

