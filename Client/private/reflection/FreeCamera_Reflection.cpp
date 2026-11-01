#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "FreeCamera.h"

ENGINE_REFLECTION_REGISTRATION(FreeCamera)
{
	ReflectedTypeBuilder<FreeCamera>(batch, "FreeCamera", "Camera");
}
