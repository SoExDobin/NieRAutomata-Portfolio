#include "ScriptComponent.h"
#include "../Engine_RTTRClass.h"

using namespace rttr;
using namespace Engine;

RTTR_REGISTRATION_NAMED(ScriptComponent_RTTR)
{
	RTTR_Class<ScriptComponent>("ScriptComponent", "Component")
		.method("Clone", &ScriptComponent::Clone)
		;
}
