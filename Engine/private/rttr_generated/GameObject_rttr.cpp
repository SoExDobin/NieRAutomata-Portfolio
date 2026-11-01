#include "GameObject.h"
#include "../Engine_RTTRClass.h"
#include "Game.h"
#include "LayerRegistry.h"
#include "TagRegistry.h"
#include "Engine_RTTR_Metadata.h"

using namespace rttr;
using namespace Engine;

RTTR_REGISTRATION_NAMED(GameObject_RTTR)
{
	RTTR_Class<GameObject>("GameObject", "Object") 
		(metadata(Meta_Key_Type::AssetType, Asset_Type_Key::GameObject))
		.method("Clone", &GameObject::Clone)
		;
}
