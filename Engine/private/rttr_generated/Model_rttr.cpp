#include "Model.h"
#include "../Engine_RTTRClass.h"

using namespace rttr;
using namespace Engine;

RTTR_REGISTRATION_NAMED(Model_RTTR)
{
	RTTR_Class<Model>("Model", "Component")
		(metadata(Meta_Key_Type::AssetType, Asset_Type_Key::Model))
		.constructor<>()
		.method("Clone", &Model::Clone)
		.method("Create", &Model::Create)
		.method("CreatePrototype", &Model::CreatePrototype)
		.property("AnimationPreset", &Model::Get_AnimationPreset, &Model::Set_AnimationPreset)
		(
			metadata(Meta_Key_Type::DataType, "AnimationPreset"),
			metadata(Meta_Key_Type::TypeTag, "AnimationPreset"),
			metadata(Meta_Key_Type::SaveData, Save_Data_Key::AnimationPreset)
		);

	// Set_ModelTag rejects an unloaded tag or a skeletal/static mismatch; the write reports it.
	RTTR_Property<Model>("ModelTag", &Model::Get_ModelTag, &Model::Set_ModelTag)
		.Meta(Meta_Key_Type::TypeTag, Data_Tag::ResourceTag)
		.Meta(Meta_Key_Type::AssetType, Asset_Type_Key::Model)
		.Meta(Meta_Key_Type::SaveData, Save_Data_Key::ModelTag);
}
