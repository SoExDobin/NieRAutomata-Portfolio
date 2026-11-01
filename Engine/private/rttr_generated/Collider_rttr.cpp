#include "Collider.h"
#include "../Engine_RTTRClass.h"

using namespace rttr;
using namespace Engine;

RTTR_REGISTRATION_NAMED(Collider_RTTR)
{
	// Every collider shape has Offset; each shape's Set_Offset moves its local bounds.
	RTTR_Class<Collider>("Collider", "Component")
		.method("Clone", &Collider::Clone)
		.property("Offset", &Collider::Get_Offset, &Collider::Set_Offset)
		(
			metadata(Meta_Key_Type::DataType, Data_Type_Key::Vector3),
			metadata(Meta_Key_Type::TypeTag, Data_Tag::NoneTag),
			metadata(Meta_Key_Type::SaveData, "Offset")
		)
		;
}
