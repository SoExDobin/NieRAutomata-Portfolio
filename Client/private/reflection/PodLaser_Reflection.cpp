#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "PodLaser.h"

ENGINE_REFLECTION_REGISTRATION(PodLaser)
{
	ReflectedTypeBuilder<PodLaser>(batch, "PodLaser", "GameObject");
}
