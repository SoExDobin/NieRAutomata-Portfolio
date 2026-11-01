#include "AnimationTracker.h"
#include "../Engine_RTTRClass.h"

using namespace rttr;
using namespace Engine;

RTTR_REGISTRATION_NAMED(AnimationTracker_RTTR)
{
	RTTR_Class<AnimationTracker>("AnimationTracker", "Component")
		.constructor<>()
		.method("Clone", &AnimationTracker::Clone)
		.method("Create", &AnimationTracker::Create)
		.method("CreatePrototype", &AnimationTracker::CreatePrototype)
		;
}
