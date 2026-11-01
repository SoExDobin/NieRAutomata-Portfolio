#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "Pl0000Body.h"

ENGINE_REFLECTION_REGISTRATION(Pl0000Body)
{
	ReflectedTypeBuilder<Pl0000Body>(batch, "Pl0000Body", "PartObject");
}
