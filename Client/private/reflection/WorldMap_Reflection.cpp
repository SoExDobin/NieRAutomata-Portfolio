#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "WorldMap.h"

ENGINE_REFLECTION_REGISTRATION(WorldMap)
{
	ReflectedTypeBuilder<WorldMap>(batch, "WorldMap", "GameObject");
}
