#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "Em3000Parts.h"

ENGINE_REFLECTION_REGISTRATION(Em3000Parts)
{
	ReflectedTypeBuilder<Em3000Parts>(batch, "Em3000Parts", "PartObject");
}
