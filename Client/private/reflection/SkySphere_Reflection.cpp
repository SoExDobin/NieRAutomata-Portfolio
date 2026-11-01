#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "SkySphere.h"

ENGINE_REFLECTION_REGISTRATION(SkySphere)
{
	ReflectedTypeBuilder<SkySphere>(batch, "SkySphere", "GameObject");
}
