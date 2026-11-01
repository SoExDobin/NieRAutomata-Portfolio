#include "UIImage.h"
#include "../Engine_RTTRClass.h"

using namespace rttr;
using namespace Engine;

RTTR_REGISTRATION_NAMED(UIImage_RTTR)
{
	RTTR_Class<UIImage>("UIImage", "UIPanel")
		.constructor<>()
		.method("Clone", &UIImage::Clone)
		.method("CreatePrototype", &UIImage::CreatePrototype)
		;
}
