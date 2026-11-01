#include "UIPanel.h"
#include "../Engine_RTTRClass.h"

using namespace rttr;
using namespace Engine;

RTTR_REGISTRATION_NAMED(UIPanel_RTTR)
{
	RTTR_Class<UIPanel>("UIPanel", "UIObject")
		.constructor<>()
		.method("Clone", &UIPanel::Clone)
		.method("CreatePrototype", &UIPanel::CreatePrototype)
		;
}
