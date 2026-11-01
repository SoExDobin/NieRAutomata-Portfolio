#include "Animation.h"
#include "../Engine_RTTRClass.h"

using namespace rttr;
using namespace Engine;

RTTR_REGISTRATION_NAMED(Animation_RTTR)
{
	RTTR_Class<Animation>("Animation", "Component")
		.constructor<>()
		.method("Clone", &Animation::Clone)
		.method("Create", &Animation::Create)
		;
}
