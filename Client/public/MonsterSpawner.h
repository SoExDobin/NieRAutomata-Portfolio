#pragma once
#include "GameObject.h"

NS_BEGIN(Client)

/* Scene-placed spawner for ordinary enemies. Monsters lists {Prefab name, count} rows (ProjectSetting/Prefab
   documents such as em0010, em0010_Gun, em0000); on its first Update (gameplay only: the Editor runs Update in
   Play, never in Stop) it instantiates every row once at points drawn from Seed within SpreadRadius of its
   position, at least MinSpacing apart, each set on the static world ground found near the spawner's height.
   The monsters are ordinary level roots owned by ObjectManager; nothing respawns, and an Editor Stop reloads the
   saved level, which holds only the spawner. The same Seed and settings give the same placement; change Seed to
   draw again. Level 4 swaps the meshes through ModelSettings "replaces", so the same prefab serves both stages.
   Cue -1 spawns on load as above; Cue >= 0 (a COMMUNICATION_CUE) waits until that dialogue starts, and those
   monsters are sent straight at the player, so an operator's "new enemies" report has enemies behind it.
   Passive makes the monsters park visitors that leave the player alone until hit (Monster::Set_Passive). */
class CLIENT_DLL MonsterSpawner final : public GameObject
{
public:
	explicit MonsterSpawner();
	explicit MonsterSpawner(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit MonsterSpawner(const MonsterSpawner& rhs);
	virtual ~MonsterSpawner() override = default;

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;

public:
	void Priority_Update(Float timeDelta) override {}
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override {}
	void Fixed_Update(Float fixedDelta) override {}
	HRESULT Render() override { return S_OK; }

public:
	Bool Get_Enabled() const { return m_Enabled; }
	void Set_Enabled(Bool enabled) { m_Enabled = enabled; }
	const vector<TagCount>& Get_Monsters() const { return m_Monsters; }
	// Rejects an empty or repeated tag and a count above MaxCount.
	HRESULT Set_Monsters(const vector<TagCount>& monsters);
	Float Get_SpreadRadius() const { return m_SpreadRadius; }
	void Set_SpreadRadius(Float radius) { m_SpreadRadius = (std::max)(radius, 0.f); }
	Float Get_MinSpacing() const { return m_MinSpacing; }
	void Set_MinSpacing(Float spacing) { m_MinSpacing = (std::max)(spacing, 0.f); }
	uint32 Get_Seed() const { return m_Seed; }
	void Set_Seed(uint32 seed) { m_Seed = seed; }
	int32 Get_Cue() const { return m_Cue; }
	void Set_Cue(int32 cue) { m_Cue = cue; }
	Bool Get_OutOfView() const { return m_OutOfView; }
	void Set_OutOfView(Bool outOfView) { m_OutOfView = outOfView; }
	Bool Get_Passive() const { return m_Passive; }
	void Set_Passive(Bool passive) { m_Passive = passive; }

public:
	static constexpr uint32 MaxCount = 32;	// per row

private:
	void Spawn();
	static Bool Is_InView(const Vector3& point);

private:
	Bool m_Enabled{ true };
	vector<TagCount> m_Monsters;
	Float m_SpreadRadius{ 8.f };
	Float m_MinSpacing{ 2.5f };
	uint32 m_Seed{ 1 };
	int32 m_Cue{ -1 };
	Bool m_OutOfView{ false };	// draw only points the camera can't see (a wave arriving from off screen)
	Bool m_Passive{ false };
	Bool m_Fired{ false };	// runtime only: this load already spawned (or queued) its monsters

public:
	static Shared<MonsterSpawner> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;
};

NS_END
