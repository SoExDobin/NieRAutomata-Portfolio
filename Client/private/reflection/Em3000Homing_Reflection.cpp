#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "Em3000Homing.h"

ENGINE_REFLECTION_REGISTRATION(Em3000Homing)
{
	ReflectedTypeBuilder<Em3000Homing>(batch, "Em3000Homing", "GameObject");
}
