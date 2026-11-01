#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "ThirdPersonCamera.h"

ENGINE_REFLECTION_REGISTRATION(ThirdPersonCamera)
{
	ReflectedTypeBuilder<ThirdPersonCamera>(batch, "ThirdPersonCamera", "Camera");
}
