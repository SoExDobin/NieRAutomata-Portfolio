#pragma once
#include "EngineManager.h"



NS_BEGIN(Engine)
class GameObject;
class LevelSerializer : public EngineManager
{
	NO_COPY(LevelSerializer)
public:
	explicit LevelSerializer();
	~LevelSerializer() override = default;

	HRESULT SerializeLevel(uint32 levIndex, const wstring& filePath);
	HRESULT DeSerializeLevel(const wstring& filePath);
	HRESULT SerializePrefab(PrefabGuid prefabGuid, uint32 levIndex,
		const Shared<GameObject>& selectedRoot, const wstring& filePath);
	HRESULT DeSerializePrefab(PrefabGuid prefabGuid, uint32 levIndex,
		const wstring& filePath, Shared<GameObject>& outRoot);
	HRESULT SerializeSubtreeSnapshot(PrefabGuid snapshotGuid, uint32 levIndex,
		const Shared<GameObject>& selectedRoot, string& outSnapshot);
	HRESULT DeSerializeSubtreeSnapshot(PrefabGuid snapshotGuid, uint32 levIndex,
		const string& snapshot, Bool preserveObjectGuids, Shared<GameObject>& outRoot);

private:
	HRESULT SerializeDocument(uint32 levIndex, const wstring& filePath,
		const Shared<GameObject>& selectedRoot, PrefabGuid prefabGuid,
		string* outSnapshot = nullptr);
	HRESULT DeSerializeDocument(const wstring& filePath, uint32 targetLevel,
		PrefabGuid prefabGuid, Shared<GameObject>* outRoot,
		Bool preserveObjectGuids = false, const string* snapshot = nullptr);

	static string ToUtf8(const wstring& ws);
	static wstring FromUtf8(const string& s);

public:
	HRESULT Initialize_Prototype() override { return EngineManager::Initialize_Prototype(); }
	HRESULT Initialize(void* arg) override { return EngineManager::Initialize(arg); }
	void On_Destroy() override { EngineManager::On_Destroy(); }
	void On_Disable() override { EngineManager::On_Disable(); }
	void On_Enable() override { EngineManager::On_Enable(); }
	void Set_Active(Bool isActive) override { EngineManager::Set_Active(isActive); }
	
public:
	static Unique<LevelSerializer> Create();
};
NS_END
