#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "LoadingFadeIn.h"

ENGINE_REFLECTION_REGISTRATION(LoadingFadeIn)
{
	ReflectedTypeBuilder<LoadingFadeIn>(batch, "LoadingFadeIn", "UIImage")
        .Property("Duration", &LoadingFadeIn::Get_Duration, &LoadingFadeIn::Set_Duration)
        .Meta(Meta_Key_Type::Min, 0.f).Meta(Meta_Key_Type::Max, 30.f);
}
