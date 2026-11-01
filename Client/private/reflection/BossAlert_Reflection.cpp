#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "BossAlert.h"

ENGINE_REFLECTION_REGISTRATION(BossAlert)
{
	ReflectedTypeBuilder<BossAlert>(batch, "BossAlert", "UIPanel");
}
