#include "VIBuffer_Particle_Point.h"
#include "../Engine_RTTRClass.h"

using namespace rttr;
using namespace Engine;

RTTR_REGISTRATION_NAMED(VIBuffer_Particle_Point_RTTR)
{
	RTTR_Class<VIBuffer_Particle_Point>("VIBuffer_Particle_Point", "VIBuffer_Instance")
		.constructor<>()
		.method("Clone", &VIBuffer_Particle_Point::Clone)
		.method("CreatePrototype", &VIBuffer_Particle_Point::CreatePrototype)
		// Read-only, not saved: shown in the Inspector so the component is not an empty header.
		.property_readonly("Loop", &VIBuffer_Particle_Point::Get_IsLoop)
		;
}
