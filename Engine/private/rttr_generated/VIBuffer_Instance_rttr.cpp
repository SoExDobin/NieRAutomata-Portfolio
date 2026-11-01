#include "VIBuffer_Instance.h"
#include "../Engine_RTTRClass.h"

using namespace rttr;
using namespace Engine;

RTTR_REGISTRATION_NAMED(VIBuffer_Instance_RTTR)
{
	RTTR_Class<VIBuffer_Instance>("VIBuffer_Instance", "VIBuffer")
		.method("Clone", &VIBuffer_Instance::Clone)
		// Read-only, not saved: shown in the Inspector so the component is not an empty header.
		.property_readonly("Instances", &VIBuffer_Instance::Get_NumInstances)
		;
}
