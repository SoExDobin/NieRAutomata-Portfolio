#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "SkyBox.h"

ENGINE_REFLECTION_REGISTRATION(SkyBox)
{
	ReflectedTypeBuilder<SkyBox>(batch, "SkyBox", "GameObject");
}
