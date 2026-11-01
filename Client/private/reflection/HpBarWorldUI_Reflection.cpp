#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "HpBarWorldUI.h"

ENGINE_REFLECTION_REGISTRATION(HpBarWorldUI)
{
	using namespace Meta_Key_Type;

	ReflectedTypeBuilder<HpBarWorldUI>(batch, "HpBarWorldUI", "WorldUIObject")
		.Property("Target", &HpBarWorldUI::Get_TargetObjectGuid, &HpBarWorldUI::Set_TargetObjectGuid)
			.Meta(ReferenceBase, "Entity")
		.Property("WorldOffset", &HpBarWorldUI::Get_WorldOffset, &HpBarWorldUI::Set_WorldOffset)
			.Meta(TypeTag, Data_Tag::Position)
		.Property("DisplayName", &HpBarWorldUI::Get_DisplayName, &HpBarWorldUI::Set_DisplayName)
		.Property("Level", &HpBarWorldUI::Get_Level, &HpBarWorldUI::Set_Level);
}
