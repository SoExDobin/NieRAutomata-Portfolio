#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "PointLight.h"

ENGINE_REFLECTION_REGISTRATION(PointLight)
{
	ReflectedTypeBuilder<PointLight>(batch, "PointLight", "GameObject")
		.Property("Color", &PointLight::Get_Color, &PointLight::Set_Color)
		.Property("Range", &PointLight::Get_Range, &PointLight::Set_Range)
		.Property("Glow", &PointLight::Get_Glow, &PointLight::Set_Glow);
}
