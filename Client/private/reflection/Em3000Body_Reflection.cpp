#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "Em3000Body.h"

ENGINE_REFLECTION_REGISTRATION(Em3000Body)
{
	ReflectedTypeBuilder<Em3000Body>(batch, "Em3000Body", "PartObject");
}
