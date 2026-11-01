#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "SparkEffect.h"

ENGINE_REFLECTION_REGISTRATION(SparkEffect)
{
	ReflectedTypeBuilder<SparkEffect>(batch, "SparkEffect", "GameObject");
}
