#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "LaserEffect.h"

ENGINE_REFLECTION_REGISTRATION(LaserEffect)
{
	ReflectedTypeBuilder<LaserEffect>(batch, "LaserEffect", "GameObject");
}
