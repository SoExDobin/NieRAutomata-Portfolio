#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "Terrain.h"

ENGINE_REFLECTION_REGISTRATION(Terrain)
{
	ReflectedTypeBuilder<Terrain>(batch, "Terrain", "GameObject");
}
