#include "Channel.h"
#include "../Engine_RTTRClass.h"

using namespace rttr;
using namespace Engine;

RTTR_REGISTRATION_NAMED(Channel_RTTR)
{
	RTTR_Class<Channel>("Channel", "Component")
		.constructor<>()
		.method("Clone", &Channel::Clone)
		.method("Create", &Channel::Create)
		;
}
