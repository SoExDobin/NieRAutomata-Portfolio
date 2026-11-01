#include "UIObject.h"
#include "../Engine_RTTRClass.h"

using namespace rttr;
using namespace Engine;

RTTR_REGISTRATION_NAMED(UIObject_RTTR)
{
	RTTR_Class<UIObject>("UIObject", "GameObject")
		.method("Clone", &UIObject::Clone)
		;

	// UI_ANCHOR as its index (the Inspector shows the anchor names); an index past BOTTOM_RIGHT is rejected.
	RTTR_Property<UIObject>("Anchor", &UIObject::Get_AnchorIndex, &UIObject::Set_AnchorIndex)
		.Meta(Meta_Key_Type::TypeTag, "UIAnchor")
		.Meta(Meta_Key_Type::SaveData, Save_Data_Key::UIAnchor);
}
