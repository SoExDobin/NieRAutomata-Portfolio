#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "Em3003.h"

ENGINE_REFLECTION_REGISTRATION(Em3003)
{
	ReflectedTypeBuilder<Em3003>(batch, "Em3003", "Em3000Parts");
}
