#include "VIBuffer_Terrain.h"
#include "../Engine_RTTRClass.h"

using namespace rttr;
using namespace Engine;

RTTR_REGISTRATION_NAMED(VIBuffer_Terrain_RTTR)
{
	RTTR_Class<VIBuffer_Terrain>("VIBuffer_Terrain", "VIBuffer")
		.constructor<>()
		.method("Clone", &VIBuffer_Terrain::Clone)
		.method("Create", &VIBuffer_Terrain::Create)
		.method("CreatePrototype", &VIBuffer_Terrain::CreatePrototype)
		;
}
