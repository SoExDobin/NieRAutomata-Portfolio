#include "SoundEmitter.h"
#include "../Engine_RTTRClass.h"

using namespace rttr;
using namespace Engine;

RTTR_REGISTRATION_NAMED(SoundEmitter_RTTR)
{
	using namespace Meta_Key_Type;
	using namespace Data_Type_Key;
	using namespace Data_Tag;

	RTTR_Class<SoundEmitter>("SoundEmitter", "Component")
		.constructor<>()
		.method("Clone", &SoundEmitter::Clone)
		.method("Create", &SoundEmitter::Create)
		.method("CreatePrototype", &SoundEmitter::CreatePrototype)
		.property("SoundTag", &SoundEmitter::Get_SoundTag, &SoundEmitter::Set_SoundTag)
		(
			metadata(DataType, Data_Type_Key::wString),
			metadata(TypeTag, Data_Tag::NoneTag),
			metadata(SaveData, "SoundTag")
		)
		.property("Volume", &SoundEmitter::Get_Volume, &SoundEmitter::Set_Volume)
		(
			metadata(DataType, Data_Type_Key::Float),
			metadata(TypeTag, Data_Tag::NoneTag),
			metadata(SaveData, "Volume"),
			metadata(Min, 0.f)
		)
		.property("NearDistance", &SoundEmitter::Get_NearDistance, &SoundEmitter::Set_NearDistance)
		(
			metadata(DataType, Data_Type_Key::Float),
			metadata(TypeTag, Data_Tag::NoneTag),
			metadata(SaveData, "NearDistance"),
			metadata(Min, 0.f)
		)
		.property("FarDistance", &SoundEmitter::Get_FarDistance, &SoundEmitter::Set_FarDistance)
		(
			metadata(DataType, Data_Type_Key::Float),
			metadata(TypeTag, Data_Tag::NoneTag),
			metadata(SaveData, "FarDistance"),
			metadata(Min, 0.f)
		)
		;
}
