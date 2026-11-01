#include "OBBCollider.h"
#include "../Engine_RTTRClass.h"

using namespace rttr;
using namespace Engine;

RTTR_REGISTRATION_NAMED(OBBCollider_RTTR)
{
	RTTR_Class<OBBCollider>("OBBCollider", "Collider")
		.constructor<>()
		.method("Clone", &OBBCollider::Clone)
		.method("Create", &OBBCollider::Create)
		.method("CreatePrototype", &OBBCollider::CreatePrototype)
		.property("Extents", &OBBCollider::Get_Extents, &OBBCollider::Set_Extents)
		(
			metadata(Meta_Key_Type::DataType, Data_Type_Key::Vector3),
			metadata(Meta_Key_Type::TypeTag, Data_Tag::NoneTag),
			metadata(Meta_Key_Type::SaveData, "Extents"),
			metadata(Meta_Key_Type::DisplayName, "Extents (Half-Size)"),
			metadata(Meta_Key_Type::Min, 0.01f)
		)
		;
}
