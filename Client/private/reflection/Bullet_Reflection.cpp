#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "Bullet.h"

ENGINE_REFLECTION_REGISTRATION(Bullet)
{
	ReflectedTypeBuilder<Bullet>(batch, "Bullet", "GameObject");
}
