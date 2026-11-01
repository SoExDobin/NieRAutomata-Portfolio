#include "VICube.h"
#include "../Engine_RTTRClass.h"

using namespace rttr;
using namespace Engine;

RTTR_REGISTRATION_NAMED(VICube_RTTR)
{
	RTTR_Class<VICube>("VICube", "VIBuffer")
		.constructor<>()
		.method("Clone", &VICube::Clone)
		.method("Create", &VICube::Create)
		.method("CreatePrototype", &VICube::CreatePrototype)
		;
}
