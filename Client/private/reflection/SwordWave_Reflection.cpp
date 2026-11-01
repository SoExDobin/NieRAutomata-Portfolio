#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "SwordWave.h"

ENGINE_REFLECTION_REGISTRATION(SwordWave)
{
	ReflectedTypeBuilder<SwordWave>(batch, "SwordWave", "GameObject");
}
