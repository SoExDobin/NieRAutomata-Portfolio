#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "DirectionalLight.h"

ENGINE_REFLECTION_REGISTRATION(DirectionalLight)
{
	ReflectedTypeBuilder<DirectionalLight>(batch, "DirectionalLight", "GameObject")
		.Property("Color", &DirectionalLight::Get_Color, &DirectionalLight::Set_Color)
		.Property("Ambient", &DirectionalLight::Get_Ambient, &DirectionalLight::Set_Ambient)
		.Property("Exposure", &DirectionalLight::Get_Exposure, &DirectionalLight::Set_Exposure)
		.Property("Contrast", &DirectionalLight::Get_Contrast, &DirectionalLight::Set_Contrast)
		.Property("Saturation", &DirectionalLight::Get_Saturation, &DirectionalLight::Set_Saturation)
		.Property("Tint", &DirectionalLight::Get_Tint, &DirectionalLight::Set_Tint)
		.Property("FogColor", &DirectionalLight::Get_FogColor, &DirectionalLight::Set_FogColor)
		.Property("FogDistance", &DirectionalLight::Get_FogDistance, &DirectionalLight::Set_FogDistance)
		.Property("FogOpacity", &DirectionalLight::Get_FogOpacity, &DirectionalLight::Set_FogOpacity)
		.Property("FogSunColor", &DirectionalLight::Get_FogSunColor, &DirectionalLight::Set_FogSunColor)
		.Property("Bloom", &DirectionalLight::Get_Bloom, &DirectionalLight::Set_Bloom);
}
