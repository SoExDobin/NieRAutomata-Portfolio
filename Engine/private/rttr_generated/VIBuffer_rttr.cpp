#include "VIBuffer.h"
#include "../Engine_RTTRClass.h"

using namespace rttr;
using namespace Engine;

RTTR_REGISTRATION_NAMED(VIBuffer_RTTR)
{
	RTTR_Class<VIBuffer>("VIBuffer", "Component")
		.method("Clone", &VIBuffer::Clone)
		// Read-only, not saved: shown in the Inspector so the component is not an empty header.
		.property_readonly("Vertices", &VIBuffer::Get_NumVertices)
		.property_readonly("Indices", &VIBuffer::Get_NumIndices)
		;
}
