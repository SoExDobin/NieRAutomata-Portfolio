#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "Pl0000HpBar.h"

ENGINE_REFLECTION_REGISTRATION(Pl0000HpBar)
{
	ReflectedTypeBuilder<Pl0000HpBar>(batch, "Pl0000HpBar", "UIObject")
		.Property("Level", &Pl0000HpBar::Get_Level, &Pl0000HpBar::Set_Level);
}
