#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "AreaTrigger.h"

ENGINE_REFLECTION_REGISTRATION(AreaTrigger)
{
	ReflectedTypeBuilder<AreaTrigger>(batch, "AreaTrigger", "GameObject")
		.Property("LightOverride", &AreaTrigger::Get_LightOverride, &AreaTrigger::Set_LightOverride)
		.Property("LightColor", &AreaTrigger::Get_LightColor, &AreaTrigger::Set_LightColor)
		.Property("LightAmbient", &AreaTrigger::Get_LightAmbient, &AreaTrigger::Set_LightAmbient)
		.Property("LightExposure", &AreaTrigger::Get_LightExposure, &AreaTrigger::Set_LightExposure)
		.Property("LightFogOpacity", &AreaTrigger::Get_LightFogOpacity, &AreaTrigger::Set_LightFogOpacity)
		.Property("NextLevel", &AreaTrigger::Get_NextLevel, &AreaTrigger::Set_NextLevel)
		.Property("CameraEvent", &AreaTrigger::Get_CameraEvent, &AreaTrigger::Set_CameraEvent)
		.Property("Communication", &AreaTrigger::Get_Communication, &AreaTrigger::Set_Communication);
}
