#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "Firework.h"

ENGINE_REFLECTION_REGISTRATION(Firework)
{
	ReflectedTypeBuilder<Firework>(batch, "Firework", "GameObject")
		.Property("Interval", &Firework::Get_Interval, &Firework::Set_Interval)
		.Property("Radius", &Firework::Get_Radius, &Firework::Set_Radius)
		.Property("Area", &Firework::Get_Area, &Firework::Set_Area);
}
