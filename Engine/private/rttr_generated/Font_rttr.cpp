#include "Font.h"
#include "../Engine_RTTRClass.h"

using namespace rttr;
using namespace Engine;

RTTR_REGISTRATION_NAMED(Font_RTTR)
{
	RTTR_Class<Font>("Font", "Object")
		.constructor<>()
		.method("Create", &Font::Create)
		;
}
