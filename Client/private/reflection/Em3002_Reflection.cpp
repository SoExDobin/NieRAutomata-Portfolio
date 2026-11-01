#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "Em3002.h"

ENGINE_REFLECTION_REGISTRATION(Em3002)
{
	ReflectedTypeBuilder<Em3002>(batch, "Em3002", "Em3000Parts");
}
