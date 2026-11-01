#include "Material.h"
#include "../Engine_RTTRClass.h"

using namespace rttr;
using namespace Engine;

RTTR_REGISTRATION_NAMED(Material_RTTR)
{
	RTTR_Class<Material>("Material", "Component")
		.constructor<>()
		.method("Clone", &Material::Clone)
		.method("Create", &Material::Create)
		.method("CreatePrototype", &Material::CreatePrototype)
		;
}
