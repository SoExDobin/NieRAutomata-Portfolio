#include "Shader.h"
#include "../Engine_RTTRClass.h"

using namespace rttr;
using namespace Engine;

RTTR_REGISTRATION_NAMED(Shader_RTTR)
{
	RTTR_Class<Shader>("Shader", "Component")
		.constructor<>()
		.method("Clone", &Shader::Clone)
		.method("Create", &Shader::Create)
		.method("CreatePrototype", &Shader::CreatePrototype)
		// Read-only, not saved: shown in the Inspector so the component is not an empty header.
		.property_readonly("ShaderTag", &Shader::Get_ShaderTag)
		.property_readonly("Passes", &Shader::Get_NumPasses)
		;
}
