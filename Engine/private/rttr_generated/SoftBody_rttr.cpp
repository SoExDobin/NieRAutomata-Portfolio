#include "SoftBody.h"
#include "../Engine_RTTRClass.h"

using namespace rttr;
using namespace Engine;

RTTR_REGISTRATION_NAMED(SoftBody_RTTR)
{
	using namespace Meta_Key_Type;
	using namespace Data_Type_Key;
	using namespace Data_Tag;

	RTTR_Class<SoftBody>("SoftBody", "Component")
		.constructor<>()
		.method("Clone", &SoftBody::Clone)
		.method("Create", &SoftBody::Create)
		.method("CreatePrototype", &SoftBody::CreatePrototype)
		.property("Simulation", &SoftBody::Get_Simulation, &SoftBody::Set_Simulation)
		(
			metadata(TypeTag, Data_Tag::NoneTag),
			metadata(SaveData, "Simulation")
		)
		.property("Damping", &SoftBody::Get_Damping, &SoftBody::Set_Damping)
		(
			metadata(DataType, Data_Type_Key::Float),
			metadata(TypeTag, Data_Tag::NoneTag),
			metadata(SaveData, "Damping"),
			metadata(DisplayName, "Damping Scale"),
			metadata(Min, 0.f)
		)
		.property("Gravity", &SoftBody::Get_Gravity, &SoftBody::Set_Gravity)
		(
			metadata(DataType, Data_Type_Key::Float),
			metadata(TypeTag, Data_Tag::NoneTag),
			metadata(SaveData, "Gravity"),
			metadata(DisplayName, "Gravity Scale"),
			metadata(Min, 0.f)
		)
		.property("SwingLimit", &SoftBody::Get_SwingLimit, &SoftBody::Set_SwingLimit)
		(
			metadata(DataType, Data_Type_Key::Float),
			metadata(TypeTag, Data_Tag::NoneTag),
			metadata(SaveData, "SwingLimit"),
			metadata(DisplayName, "Swing Limit Scale"),
			metadata(Min, 0.f)
		)
		.property("Follow", &SoftBody::Get_Follow, &SoftBody::Set_Follow)
		(
			metadata(DataType, Data_Type_Key::Float),
			metadata(TypeTag, Data_Tag::NoneTag),
			metadata(SaveData, "Follow"),
			metadata(DisplayName, "Follow Scale"),
			metadata(Min, 0.f)
		)
		.property("Wind", &SoftBody::Get_Wind, &SoftBody::Set_Wind)
		(
			metadata(DataType, Data_Type_Key::Float),
			metadata(TypeTag, Data_Tag::NoneTag),
			metadata(SaveData, "Wind"),
			metadata(DisplayName, "Wind Scale"),
			metadata(Min, 0.f)
		)
		;
}
