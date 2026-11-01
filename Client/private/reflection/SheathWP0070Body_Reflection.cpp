#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "SheathWP0070Body.h"

ENGINE_REFLECTION_REGISTRATION(SheathWP0070Body)
{
	ReflectedTypeBuilder<SheathWP0070Body>(batch, "SheathWP0070Body", "PartObject");
}
