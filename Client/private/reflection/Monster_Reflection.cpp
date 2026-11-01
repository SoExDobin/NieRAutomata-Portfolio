#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "Monster.h"

ENGINE_REFLECTION_REGISTRATION(Monster)
{
	ReflectedTypeBuilder<Monster>(batch, "Monster", "Entity");
}
