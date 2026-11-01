#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "MonsterSpawner.h"

ENGINE_REFLECTION_REGISTRATION(MonsterSpawner)
{
	using namespace Meta_Key_Type;

	ReflectedTypeBuilder<MonsterSpawner>(batch, "MonsterSpawner", "GameObject")
		.Property("Enabled", &MonsterSpawner::Get_Enabled, &MonsterSpawner::Set_Enabled)
		.Property("Monsters", &MonsterSpawner::Get_Monsters, &MonsterSpawner::Set_Monsters)
			.Meta(AssetType, Asset_Type_Key::Prefab)
		.Property("SpreadRadius", &MonsterSpawner::Get_SpreadRadius, &MonsterSpawner::Set_SpreadRadius)
			.Meta(DisplayName, "Spread Radius")
			.Meta(Min, 0.f)
			.Meta(Max, 100.f)
		.Property("MinSpacing", &MonsterSpawner::Get_MinSpacing, &MonsterSpawner::Set_MinSpacing)
			.Meta(DisplayName, "Min Spacing")
			.Meta(Min, 0.f)
			.Meta(Max, 20.f)
		.Property("Seed", &MonsterSpawner::Get_Seed, &MonsterSpawner::Set_Seed)
		.Property("Cue", &MonsterSpawner::Get_Cue, &MonsterSpawner::Set_Cue)
		.Property("OutOfView", &MonsterSpawner::Get_OutOfView, &MonsterSpawner::Set_OutOfView)
			.Meta(DisplayName, "Out Of View")
		.Property("Passive", &MonsterSpawner::Get_Passive, &MonsterSpawner::Set_Passive);
}
