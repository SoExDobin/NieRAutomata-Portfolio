#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "Pl0000Shockwave.h"

ENGINE_REFLECTION_REGISTRATION(Pl0000Shockwave)
{
	ReflectedTypeBuilder<Pl0000Shockwave>(batch, "Pl0000Shockwave", "GameObject");
}
