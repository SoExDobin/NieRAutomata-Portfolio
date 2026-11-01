#include "Camera.h"
#include "../Engine_RTTRClass.h"

using namespace rttr;
using namespace Engine;

RTTR_REGISTRATION_NAMED(Camera_RTTR)
{
	// Aspect follows the view size, so it is edited but never saved.
	RTTR_Class<Camera>("Camera", "GameObject")
		.method("Clone", &Camera::Clone)
		.property("FovY", &Camera::Get_FovY, &Camera::Set_FovY)
		(
			metadata(Meta_Key_Type::TypeTag, Data_Tag::Angle),
			metadata(Meta_Key_Type::SaveData, "FovY"),
			metadata(Meta_Key_Type::Min, XMConvertToRadians(1.f)),
			metadata(Meta_Key_Type::Max, XMConvertToRadians(179.f))
		)
		.property("Aspect", &Camera::Get_Aspect, &Camera::Set_Aspect)
		(
			metadata(Meta_Key_Type::Min, 0.1f),
			metadata(Meta_Key_Type::Max, 10.f)
		)
		.property("Near", &Camera::Get_NearPlane, &Camera::Set_NearPlane)
		(
			metadata(Meta_Key_Type::SaveData, "Near"),
			metadata(Meta_Key_Type::Min, 0.001f)
		)
		.property("Far", &Camera::Get_FarPlane, &Camera::Set_FarPlane)
		(
			metadata(Meta_Key_Type::SaveData, "Far"),
			metadata(Meta_Key_Type::Min, 1.f)
		)
		;

	// A GUID with no live object is kept; Get_Target resolves it once the object exists.
	RTTR_Property<Camera>("Target", &Camera::Get_TargetObjectGuid, &Camera::Set_TargetObjectGuid)
		.Meta(Meta_Key_Type::ReferenceBase, "GameObject");
}
