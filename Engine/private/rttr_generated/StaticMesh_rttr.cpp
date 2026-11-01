#include "StaticMesh.h"
#include "../Engine_RTTRClass.h"

using namespace rttr;
using namespace Engine;

// 생성자는 일부러 등록하지 않는다. StaticMesh는 Mesh::Create가 만들고,
// 기본 생성자를 등록하면 TypeCatalog가 팩토리 없는 구체 Object 타입으로 거부한다.
RTTR_REGISTRATION_NAMED(StaticMesh_RTTR)
{
	RTTR_Class<StaticMesh>("StaticMesh", "Mesh")
		.method("Clone", &StaticMesh::Clone)
		;
}
