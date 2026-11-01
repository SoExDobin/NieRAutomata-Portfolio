#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "EmRing.h"

ENGINE_REFLECTION_REGISTRATION(EmRing)
{
	ReflectedTypeBuilder<EmRing>(batch, "EmRing", "GameObject");
}
