#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "LoadingBackground.h"

ENGINE_REFLECTION_REGISTRATION(LoadingBackground)
{
	ReflectedTypeBuilder<LoadingBackground>(batch, "LoadingBackground", "UIPanel")
        .Property("LogDuration", &LoadingBackground::Get_LogDuration, &LoadingBackground::Set_LogDuration)
        .Meta(Meta_Key_Type::Min, 0.1f).Meta(Meta_Key_Type::Max, 30.f);
}
