#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "LoadingFadeOut.h"

ENGINE_REFLECTION_REGISTRATION(LoadingFadeOut)
{
	ReflectedTypeBuilder<LoadingFadeOut>(batch, "LoadingFadeOut", "LoadingFadeIn");
}
