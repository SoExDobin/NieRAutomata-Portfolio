#include "VIBuffer_Rect.h"
#include "../Engine_RTTRClass.h"

using namespace rttr;
using namespace Engine;

RTTR_REGISTRATION_NAMED(VIBuffer_Rect_RTTR)
{
	RTTR_Class<VIBuffer_Rect>("VIBuffer_Rect", "VIBuffer")
		.constructor<>()
		.method("Clone", &VIBuffer_Rect::Clone)
		.method("Create", &VIBuffer_Rect::Create)
		.method("CreatePrototype", &VIBuffer_Rect::CreatePrototype)
		;
}
