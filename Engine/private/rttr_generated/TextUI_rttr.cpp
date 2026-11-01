#include "TextUI.h"
#include "../Engine_RTTRClass.h"

using namespace rttr;
using namespace Engine;

RTTR_REGISTRATION_NAMED(TextUI_RTTR)
{
	RTTR_Class<TextUI>("TextUI", "UIObject")
		.constructor<>()
		.method("Clone", &TextUI::Clone)
		.method("Create", &TextUI::Create)
		.method("CreatePrototype", &TextUI::CreatePrototype)
		.property("Text", &TextUI::Get_Text, &TextUI::Set_Text)
		(
			metadata(Meta_Key_Type::DataType, Data_Type_Key::wString),
			metadata(Meta_Key_Type::SaveData, "Text")
		)
		.property("FontTag", &TextUI::Get_FontTag, &TextUI::Set_FontTag)
		(
			metadata(Meta_Key_Type::DataType, Data_Type_Key::wString),
			metadata(Meta_Key_Type::SaveData, "FontTag"),
			metadata(Meta_Key_Type::DisplayName, "Font Tag")
		)
		.property("Color", &TextUI::Get_Color, &TextUI::Set_Color)
		(
			metadata(Meta_Key_Type::DataType, Data_Type_Key::Vector4),
			metadata(Meta_Key_Type::TypeTag, Data_Tag::Color),
			metadata(Meta_Key_Type::SaveData, "Color")
		)
		.property("FontScale", &TextUI::Get_FontScale, &TextUI::Set_FontScale)
		(
			metadata(Meta_Key_Type::DataType, Data_Type_Key::Float),
			metadata(Meta_Key_Type::SaveData, "FontScale"),
			metadata(Meta_Key_Type::DisplayName, "Font Scale"),
			metadata(Meta_Key_Type::Min, 0.01f)
		)
		.property("Spacing", &TextUI::Get_Spacing, &TextUI::Set_Spacing)
		(
			metadata(Meta_Key_Type::DataType, Data_Type_Key::Float),
			metadata(Meta_Key_Type::SaveData, "Spacing")
		)
		.property("Pivot", &TextUI::Get_Pivot, &TextUI::Set_Pivot)
		(
			metadata(Meta_Key_Type::DataType, Data_Type_Key::Float),
			metadata(Meta_Key_Type::SaveData, "Pivot"),
			metadata(Meta_Key_Type::Min, 0.f),
			metadata(Meta_Key_Type::Max, 1.f)
		)
		;
}
