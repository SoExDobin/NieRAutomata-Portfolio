#include "VISphere.h"
#include "../Engine_RTTRClass.h"

using namespace rttr;
using namespace Engine;

RTTR_REGISTRATION_NAMED(VISphere_RTTR)
{
	RTTR_Class<VISphere>("VISphere", "VIBuffer")
		.constructor<>()
		.method("Clone", &VISphere::Clone)
		.method("Create", &VISphere::Create)
		.method("CreatePrototype", &VISphere::CreatePrototype)
		;
}
