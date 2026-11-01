#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "SheathWP0220Body.h"

ENGINE_REFLECTION_REGISTRATION(SheathWP0220Body)
{
	ReflectedTypeBuilder<SheathWP0220Body>(batch, "SheathWP0220Body", "PartObject");
}
