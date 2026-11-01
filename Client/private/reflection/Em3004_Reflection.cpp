#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "Em3004.h"

ENGINE_REFLECTION_REGISTRATION(Em3004)
{
	ReflectedTypeBuilder<Em3004>(batch, "Em3004", "Monster")
		.Property("ShotDamage", &Em3004::Get_ShotDamage, &Em3004::Set_ShotDamage)
		.Property("ShotSpeed", &Em3004::Get_ShotSpeed, &Em3004::Set_ShotSpeed)
		.Property("ShotsPerReload", &Em3004::Get_ShotsPerReload, &Em3004::Set_ShotsPerReload)
		.Property("ReloadWait", &Em3004::Get_ReloadWait, &Em3004::Set_ReloadWait);

	// em3004 plays em3004's clips (Em3004Body).
	batch.enums.push_back({ "Em3004.AnimationState", "AnimationState", {
		REFLECTION_ENUM_VALUE(Em3004::EM3004_ANIM::IDLE, IDLE),
		REFLECTION_ENUM_VALUE(Em3004::EM3004_ANIM::FALL, FALL),
		REFLECTION_ENUM_VALUE(Em3004::EM3004_ANIM::GET_UP, GET_UP),
		REFLECTION_ENUM_VALUE(Em3004::EM3004_ANIM::SHOOT_START, SHOOT_START),
		REFLECTION_ENUM_VALUE(Em3004::EM3004_ANIM::SHOOT_LOOP, SHOOT_LOOP),
		REFLECTION_ENUM_VALUE(Em3004::EM3004_ANIM::SHOOT_END, SHOOT_END),
		REFLECTION_ENUM_VALUE(Em3004::EM3004_ANIM::HANG, HANG),
		REFLECTION_ENUM_VALUE(Em3004::EM3004_ANIM::FLINCH, FLINCH),
	} });
}
