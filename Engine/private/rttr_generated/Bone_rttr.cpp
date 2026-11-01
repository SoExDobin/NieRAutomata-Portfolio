#include "Bone.h"
#include "../Engine_RTTRClass.h"

using namespace rttr;
using namespace Engine;

RTTR_REGISTRATION_NAMED(Bone_RTTR)
{
	RTTR_Class<Bone>("Bone", "Component")
		.constructor<>()
		.method("Clone", &Bone::Clone)
		.method("Create", &Bone::Create)
		;
}
