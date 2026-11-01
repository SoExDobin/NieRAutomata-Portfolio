#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "Em3001.h"

ENGINE_REFLECTION_REGISTRATION(Em3001)
{
	ReflectedTypeBuilder<Em3001>(batch, "Em3001", "Em3000Parts");
}
