#include "VIBuffer_Particle_Rect.h"
#include "../Engine_RTTRClass.h"

using namespace rttr;
using namespace Engine;

RTTR_REGISTRATION_NAMED(VIBuffer_Particle_Rect_RTTR)
{
	RTTR_Class<VIBuffer_Particle_Rect>("VIBuffer_Particle_Rect", "VIBuffer_Instance")
		.constructor<>()
		.method("Clone", &VIBuffer_Particle_Rect::Clone)
		.method("CreatePrototype", &VIBuffer_Particle_Rect::CreatePrototype)
		;
}
