#include "pch.h"
#include "MonsterSpawner.h"
#include "ClientApp.h"
#include "Game.h"
#include "Monster.h"
#include "Pl0000.h"
#include "UIManager.h"
#include "WorldUIObject.h"
#include "Transform.h"
#include <SpdLogger.h>
#include <filesystem>
#include <random>

namespace
{
	// The ground under a drawn point is looked for from a little above the spawner down to a few meters below it,
	// so a point over a lower street, a cliff or a roof above is a miss rather than a monster on the wrong floor.
	constexpr Float GroundProbeAbove = 3.f;
	constexpr Float GroundProbeBelow = 6.f;
	constexpr uint32 TriesPerMonster = 30;
}

MonsterSpawner::MonsterSpawner() : GameObject{} {}
MonsterSpawner::MonsterSpawner(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: GameObject{ device, context } {}
MonsterSpawner::MonsterSpawner(const MonsterSpawner& rhs)
	: GameObject{ rhs }, m_Enabled{ rhs.m_Enabled }, m_Monsters{ rhs.m_Monsters },
	m_SpreadRadius{ rhs.m_SpreadRadius }, m_MinSpacing{ rhs.m_MinSpacing }, m_Seed{ rhs.m_Seed }, m_Cue{ rhs.m_Cue },
	m_OutOfView{ rhs.m_OutOfView }, m_Passive{ rhs.m_Passive } {}

HRESULT MonsterSpawner::Initialize_Prototype()
{
	return GameObject::Initialize_Prototype();
}

HRESULT MonsterSpawner::Initialize(void* arg)
{
	if (FAILED(GameObject::Initialize(arg)))
	{
		LOG_ERROR(L"Failed to Initialize MonsterSpawner {}", m_ObjectName);
		return E_FAIL;
	}
	return S_OK;
}

HRESULT MonsterSpawner::Set_Monsters(const vector<TagCount>& monsters)
{
	for (size_t row = 0; row < monsters.size(); ++row)
	{
		const TagCount& entry = monsters[row];
		const Bool repeated = std::any_of(monsters.begin(), monsters.begin() + row,
			[&entry](const TagCount& earlier) { return earlier.tag == entry.tag; });
		if (entry.tag.empty() || repeated || entry.count > MaxCount)
		{
			LOG_WARN(L"[MonsterSpawner] {}: rejected row {} ('{}' x{}): empty or repeated tag, or more than {}",
				m_ObjectName, row, entry.tag, entry.count, MaxCount);
			return E_INVALIDARG;
		}
	}
	m_Monsters = monsters;
	return S_OK;
}

void MonsterSpawner::Update(Float timeDelta)
{
	if (m_Fired)
		return;
	if (m_Cue >= 0)
	{
		const auto client = ClientApp::GetInstance();
		if (m_Cue >= ETOI(COMMUNICATION_CUE::END) || !client)
		{
			LOG_WARN(L"[MonsterSpawner] {}: Cue {} is not a communication cue", m_ObjectName, m_Cue);
			m_Fired = true;
			return;
		}
		if (!client->Get_UIManager()->Is_ScenarioQueued(static_cast<COMMUNICATION_CUE>(m_Cue)))
			return;
	}
	m_Fired = true;
	if (!m_Enabled || m_Monsters.empty())
		return;

	// Spawning adds roots to the level ObjectManager is iterating, so it waits for the frame's event pass.
	const Weak<MonsterSpawner> weakThis = static_pointer_cast<MonsterSpawner>(shared_from_this());
	GAME_INSTANCE->Add_Instance_Event(Get_LevelIndex(),
		L"MonsterSpawner_" + std::to_wstring(Get_RuntimeObjectId()), [weakThis]()
		{
			if (const Shared<MonsterSpawner> spawner = weakThis.lock(); spawner && !spawner->Is_Destroy())
				spawner->Spawn();
		});
}

void MonsterSpawner::Spawn()
{
	const uint32 levelIndex = Get_LevelIndex();
	const Vector3 center = m_Transform->Get_Position();
	const vector<pair<PrefabGuid, wstring>> prefabs = GAME_INSTANCE->Get_PrefabDocuments();

	std::mt19937 random{ m_Seed };
	std::uniform_real_distribution<Float> unit{ 0.f, 1.f };
	vector<Vector3> placed;
	uint32 requested = 0;
	uint32 failed = 0;
	uint32 inView = 0;

	// A cued wave hunts the player at once instead of waiting to be seen (the movement collider's owner, Pl0000).
	Shared<GameObject> player;
	if (m_Cue >= 0)
		for (const auto& [id, object] : GAME_INSTANCE->Get_GameObjects(levelIndex))
			if (object && !object->Get_Parent() && !object->Is_Destroy() && Cast<Pl0000>(object))
			{
				player = object;
				break;
			}

	for (const TagCount& entry : m_Monsters)
	{
		requested += entry.count;
		const auto prefabIt = std::find_if(prefabs.begin(), prefabs.end(), [&entry](const auto& prefab) {
			return std::filesystem::path(prefab.second).stem().wstring() == entry.tag;
		});
		if (prefabIt == prefabs.end())
		{
			LOG_WARN(L"[MonsterSpawner] {}: no Prefab document named '{}'", m_ObjectName, entry.tag);
			failed += entry.count;
			continue;
		}

		for (uint32 index = 0; index < entry.count; ++index)
		{
			// Uniform over the disc; a point needs ground near the spawner's height and room from the others.
			// OutOfView also wants it off screen; if every try is in view, the last roomy one is used anyway.
			Bool found = false;
			Vector3 point{};
			Vector3 fallback{};
			Bool hasFallback = false;
			for (uint32 attempt = 0; attempt < TriesPerMonster && !found; ++attempt)
			{
				const Float angle = unit(random) * XM_2PI;
				const Float distance = m_SpreadRadius * std::sqrt(unit(random));
				const Vector3 probe{ center.x + std::cos(angle) * distance, center.y + GroundProbeAbove,
					center.z + std::sin(angle) * distance };
				if (!GAME_INSTANCE->Raycast_Physics(probe, -Vector3::UnitY, GroundProbeAbove + GroundProbeBelow, point))
					continue;
				found = std::none_of(placed.begin(), placed.end(), [this, &point](const Vector3& other) {
					return Vector2{ point.x - other.x, point.z - other.z }.LengthSquared() < m_MinSpacing * m_MinSpacing;
				});
				if (found && m_OutOfView && Is_InView(point))
				{
					fallback = point;
					hasFallback = true;
					found = false;
				}
			}
			if (!found && hasFallback)
			{
				point = fallback;
				found = true;
				++inView;
			}
			const Float yaw = unit(random) * XM_2PI;
			if (!found)
			{
				++failed;
				continue;
			}

			Shared<GameObject> root;
			if (FAILED(GAME_INSTANCE->DeSerializePrefabDocument(prefabIt->first, root, levelIndex)) || !root)
			{
				LOG_ERROR(L"[MonsterSpawner] {}: failed to instantiate Prefab '{}'", m_ObjectName, entry.tag);
				++failed;
				continue;
			}
			const Shared<Monster> monster = Cast<Monster>(root);
			if (!monster)
			{
				LOG_ERROR(L"[MonsterSpawner] {}: Prefab '{}' is not a monster", m_ObjectName, entry.tag);
				root->Destroy_Subtree();
				failed += entry.count - index;
				break;
			}
			if (player)
				monster->Set_Target(player);
			monster->Set_Passive(m_Passive);

			const Shared<Transform> transform = root->Get_Transform();
			transform->Set_Position(point);
			transform->Set_Rotation(Quaternion::CreateFromYawPitchRoll(yaw, 0.f, 0.f));
			transform->Update_WorldMatrix();
			placed.push_back(point);
		}
	}

	if (inView > 0)
		LOG_WARN(L"[MonsterSpawner] {}: {} found no spot off screen and spawned in view", m_ObjectName, inView);
	if (failed > 0)
		LOG_WARN(L"[MonsterSpawner] {}: spawned {} of {} (seed {}); {} found no ground or room within {} m",
			m_ObjectName, placed.size(), requested, m_Seed, failed, m_SpreadRadius);
	else
		LOG_INFO(L"[MonsterSpawner] {}: spawned {} (seed {})", m_ObjectName, placed.size(), m_Seed);
}

Bool MonsterSpawner::Is_InView(const Vector3& point)
{
	// The monster's chest projected into the 1920x1080 canvas, with a margin so one at the very edge counts as seen.
	constexpr Float Margin = 120.f;
	Vector2 canvas{};
	return WorldUIObject::To_Canvas(point + Vector3{ 0.f, 1.f, 0.f }, canvas) &&
		canvas.x >= -Margin && canvas.x <= 1920.f + Margin && canvas.y >= -Margin && canvas.y <= 1080.f + Margin;
}

Shared<MonsterSpawner> MonsterSpawner::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<MonsterSpawner>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		LOG_ERROR(L"Failed to Created : MonsterSpawner");
		MSG_BOX("Failed to Created : MonsterSpawner");
	}
	return prototype;
}

Shared<GameObject> MonsterSpawner::Clone(void* arg)
{
	auto instance = make_shared<MonsterSpawner>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		LOG_ERROR(L"Failed to Cloned : MonsterSpawner");
		MSG_BOX("Failed to Cloned : MonsterSpawner");
	}
	return instance;
}
