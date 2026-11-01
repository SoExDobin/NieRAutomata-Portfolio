#include "SphereCollider.h"
#include "../Engine_RTTRClass.h"

using namespace rttr;
using namespace Engine;

RTTR_REGISTRATION_NAMED(SphereCollider_RTTR)
{
	RTTR_Class<SphereCollider>("SphereCollider", "Collider")
		.constructor<>()
		.method("Clone", &SphereCollider::Clone)
		.method("Create", &SphereCollider::Create)
		.method("CreatePrototype", &SphereCollider::CreatePrototype)
		.property("Radius", &SphereCollider::Get_Radius, &SphereCollider::Set_Radius)
		(
			metadata(Meta_Key_Type::DataType, Data_Type_Key::Float),
			metadata(Meta_Key_Type::TypeTag, Data_Tag::NoneTag),
			metadata(Meta_Key_Type::SaveData, "Radius"),
			metadata(Meta_Key_Type::Min, 0.01f)
		)
		;
}
