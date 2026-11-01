#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "FireFlashEffect.h"

ENGINE_REFLECTION_REGISTRATION(FireFlashEffect)
{
	ReflectedTypeBuilder<FireFlashEffect>(batch, "FireFlashEffect", "GameObject");
}
