#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "EmTelegraph.h"

ENGINE_REFLECTION_REGISTRATION(EmTelegraph)
{
	ReflectedTypeBuilder<EmTelegraph>(batch, "EmTelegraph", "GameObject");
}
