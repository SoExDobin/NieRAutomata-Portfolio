#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "Pl0000EvadeGhost.h"

ENGINE_REFLECTION_REGISTRATION(Pl0000EvadeGhost)
{
	ReflectedTypeBuilder<Pl0000EvadeGhost>(batch, "Pl0000EvadeGhost", "GameObject");
}
