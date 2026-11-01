#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "StaticCamera.h"

ENGINE_REFLECTION_REGISTRATION(StaticCamera)
{
	ReflectedTypeBuilder<StaticCamera>(batch, "StaticCamera", "Camera");
}
