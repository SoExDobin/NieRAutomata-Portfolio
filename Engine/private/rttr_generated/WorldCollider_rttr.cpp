#include "WorldCollider.h"
#include "../Engine_RTTRClass.h"

using namespace rttr;
using namespace Engine;

RTTR_REGISTRATION_NAMED(WorldCollider_RTTR)
{
	RTTR_Class<WorldCollider>("WorldCollider", "Component")
		.constructor<>()
		.method("Clone", &WorldCollider::Clone)
		.method("Create", &WorldCollider::Create)
		.method("CreatePrototype", &WorldCollider::CreatePrototype)
		// Read-only, not saved: shown in the Inspector so the component is not an empty header.
		.property_readonly("CollisionModel", &WorldCollider::Get_SourceModelTag)
		;
}
