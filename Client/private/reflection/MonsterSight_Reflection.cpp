#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "MonsterSight.h"

ENGINE_REFLECTION_REGISTRATION(MonsterSight)
{
	ReflectedTypeBuilder<MonsterSight>(batch, "MonsterSight", "PartObject");
}
