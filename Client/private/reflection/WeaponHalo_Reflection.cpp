#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "WeaponHalo.h"

ENGINE_REFLECTION_REGISTRATION(WeaponHalo)
{
	ReflectedTypeBuilder<WeaponHalo>(batch, "WeaponHalo", "GameObject");
}
