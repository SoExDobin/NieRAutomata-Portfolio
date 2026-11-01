#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "ConfettiEffect.h"

ENGINE_REFLECTION_REGISTRATION(ConfettiEffect)
{
	ReflectedTypeBuilder<ConfettiEffect>(batch, "ConfettiEffect", "GameObject");
}
