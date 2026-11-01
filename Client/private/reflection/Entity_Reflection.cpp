#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "Entity.h"

ENGINE_REFLECTION_REGISTRATION(Entity)
{
	ReflectedTypeBuilder<Entity>(batch, "Entity", "GameObject");
}
