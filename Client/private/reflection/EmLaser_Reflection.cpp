#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "EmLaser.h"

ENGINE_REFLECTION_REGISTRATION(EmLaser)
{
	ReflectedTypeBuilder<EmLaser>(batch, "EmLaser", "GameObject");
}
