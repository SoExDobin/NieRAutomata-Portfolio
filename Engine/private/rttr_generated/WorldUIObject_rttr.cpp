#include "WorldUIObject.h"
#include "../Engine_RTTRClass.h"

using namespace rttr;
using namespace Engine;

RTTR_REGISTRATION_NAMED(WorldUIObject_RTTR)
{
	RTTR_Class<WorldUIObject>("WorldUIObject", "UIObject")
		;
}
