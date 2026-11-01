#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "MonsterAOE.h"

ENGINE_REFLECTION_REGISTRATION(MonsterAOE)
{
	ReflectedTypeBuilder<MonsterAOE>(batch, "MonsterAOE", "PartObject");
}
