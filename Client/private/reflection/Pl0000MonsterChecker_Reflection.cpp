#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "Pl0000MonsterChecker.h"

ENGINE_REFLECTION_REGISTRATION(Pl0000MonsterChecker)
{
	ReflectedTypeBuilder<Pl0000MonsterChecker>(batch, "Pl0000MonsterChecker", "PartObject");
}
