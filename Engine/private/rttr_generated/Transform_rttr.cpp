#include "Transform.h"
#include "../Engine_RTTRClass.h"

using namespace rttr;
using namespace Engine;

RTTR_REGISTRATION_NAMED(Transform_RTTR)
{
	using namespace Meta_Key_Type;
	using namespace Data_Type_Key;
	using namespace Data_Tag;
	using namespace Asset_Type_Key;
	using namespace Save_Data_Key;

	RTTR_Class<Transform>("Transform", "Component")
		.constructor<>()
		.method("Clone", &Transform::Clone)
		.method("Create", &Transform::Create)
		.method("CreatePrototype", &Transform::CreatePrototype)

		.property("Position", &Transform::Get_LocalPosition, &Transform::Set_LocalPositionByValue)
			(rttr::metadata(DataType, Data_Type_Key::Vector3),
			rttr::metadata(TypeTag, Data_Tag::Position),
			rttr::metadata(AssetType, NoneAsset),
			rttr::metadata(SaveData, TransformPos))
		.property("Rotation", &Transform::Get_LocalEulerAngles, &Transform::Set_LocalEulerAngleByValue)
			(rttr::metadata(DataType, Data_Type_Key::Vector3),
			rttr::metadata(TypeTag, Data_Tag::Rotation),
			rttr::metadata(AssetType, NoneAsset),
			rttr::metadata(SaveData, TransformRot))
		.property("Scale", &Transform::Get_LocalScale, &Transform::Set_LocalScaleByValue)
			(rttr::metadata(DataType, Data_Type_Key::Vector3),
			rttr::metadata(TypeTag, Data_Tag::Scale),
			rttr::metadata(AssetType, NoneAsset),
			rttr::metadata(SaveData, TransformScale))
		;
}
