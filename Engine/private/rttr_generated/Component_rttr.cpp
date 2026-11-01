#include "Component.h"
#include "../Engine_RTTRClass.h"

using namespace rttr;
using namespace Engine;

RTTR_REGISTRATION_NAMED(Component_RTTR)
{
	RTTR_Class<Component>("Component", "Object")
		.method("Clone", &Component::Clone)
		;
}
