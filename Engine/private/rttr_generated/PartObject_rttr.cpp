#include "PartObject.h"
#include "../Engine_RTTRClass.h"

using namespace rttr;
using namespace Engine;

RTTR_REGISTRATION_NAMED(PartObject_RTTR)
{
	RTTR_Class<PartObject>("PartObject", "GameObject")
		.method("Clone", &PartObject::Clone)
		;
}
