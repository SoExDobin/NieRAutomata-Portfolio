#include "AABBCollider.h"
#include "../Engine_RTTRClass.h"

using namespace rttr;
using namespace Engine;

RTTR_REGISTRATION_NAMED(AABBCollider_RTTR)
{
	RTTR_Class<AABBCollider>("AABBCollider", "Collider")
		.constructor<>()
		.method("Clone", &AABBCollider::Clone)
		.method("Create", &AABBCollider::Create)
		.method("CreatePrototype", &AABBCollider::CreatePrototype)
		.property("Extents", &AABBCollider::Get_Extents, &AABBCollider::Set_Extents)
		(
			metadata(Meta_Key_Type::DataType, Data_Type_Key::Vector3),
			metadata(Meta_Key_Type::TypeTag, Data_Tag::NoneTag),
			metadata(Meta_Key_Type::SaveData, "Extents"),
			metadata(Meta_Key_Type::DisplayName, "Extents (Half-Size)"),
			metadata(Meta_Key_Type::Min, 0.01f)
		)
		;
}
