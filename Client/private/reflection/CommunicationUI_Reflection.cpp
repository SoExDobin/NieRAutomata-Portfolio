#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "CommunicationUI.h"

ENGINE_REFLECTION_REGISTRATION(CommunicationUI)
{
    ReflectedTypeBuilder<CommunicationUI>(batch, "CommunicationUI", "UIPanel");
}
