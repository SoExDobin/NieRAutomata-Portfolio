#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "MonsterHitBox.h"

ENGINE_REFLECTION_REGISTRATION(MonsterHitBox)
{
	ReflectedTypeBuilder<MonsterHitBox>(batch, "MonsterHitBox", "PartObject");
}
