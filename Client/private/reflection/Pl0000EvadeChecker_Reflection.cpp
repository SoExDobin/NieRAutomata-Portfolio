#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "Pl0000EvadeChecker.h"

ENGINE_REFLECTION_REGISTRATION(Pl0000EvadeChecker)
{
	ReflectedTypeBuilder<Pl0000EvadeChecker>(batch, "Pl0000EvadeChecker", "PartObject");
}
