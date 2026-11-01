#include "Navigation.h"
#include "../Engine_RTTRClass.h"

using namespace rttr;
using namespace Engine;

RTTR_REGISTRATION_NAMED(Navigation_RTTR)
{
	RTTR_Class<Navigation>("Navigation", "Component")
		.constructor<>()
		.method("Clone", &Navigation::Clone)
		.method("Create", &Navigation::Create)
		.method("CreatePrototype", &Navigation::CreatePrototype)
		// Read-only, not saved: shown in the Inspector so the component is not an empty header.
		.property_readonly("NavModel", &Navigation::Get_ModelTag)
		.property_readonly("Cells", &Navigation::Get_NumCells)
		.property_readonly("CurrentCell", &Navigation::Get_CurrentCellIndex)
		;
}
