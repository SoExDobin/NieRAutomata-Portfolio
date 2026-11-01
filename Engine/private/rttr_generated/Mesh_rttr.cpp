#include "Mesh.h"
#include "../Engine_RTTRClass.h"

using namespace rttr;
using namespace Engine;

RTTR_REGISTRATION_NAMED(Mesh_RTTR)
{
	RTTR_Class<Mesh>("Mesh", "VIBuffer")
		.constructor<>()
		.method("Clone", &Mesh::Clone)
		.method("Create", &Mesh::Create)
		.method("CreatePrototype", &Mesh::CreatePrototype)
		;
}
