#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "LoadingLogo.h"

ENGINE_REFLECTION_REGISTRATION(LoadingLogo)
{
	ReflectedTypeBuilder<LoadingLogo>(batch, "LoadingLogo", "UIImage")
        .Property("UVRect", &LoadingLogo::Get_UVRect, &LoadingLogo::Set_UVRect);
}
