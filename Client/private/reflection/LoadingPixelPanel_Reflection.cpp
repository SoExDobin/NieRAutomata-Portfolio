#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "LoadingPixelPanel.h"

ENGINE_REFLECTION_REGISTRATION(LoadingPixelPanel)
{
	ReflectedTypeBuilder<LoadingPixelPanel>(batch, "LoadingPixelPanel", "UIObject");
}
