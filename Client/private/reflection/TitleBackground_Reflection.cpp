#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "TitleBackground.h"

ENGINE_REFLECTION_REGISTRATION(TitleBackground)
{
	ReflectedTypeBuilder<TitleBackground>(batch, "TitleBackground", "UIObject");
}
