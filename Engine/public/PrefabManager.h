#pragma once

#include "Engine_Define.h"

NS_BEGIN(Engine)

class GameObject;

class PrefabManager final
{
	NO_COPY(PrefabManager)

public:
	PrefabManager() = default;
	~PrefabManager() = default;

	HRESULT Register_Prefab(PrefabGuid prefabGuid, const wstring& filePath);
	HRESULT Register_PrefabDocument(const wstring& filePath, PrefabGuid& outPrefabGuid);
	HRESULT Load_PrefabRepository(const wstring& directoryPath);
	HRESULT Unregister_Prefab(PrefabGuid prefabGuid);
	wstring Find_PrefabPath(PrefabGuid prefabGuid) const;
	vector<pair<PrefabGuid, wstring>> Get_PrefabDocuments() const;
	HRESULT SerializePrefabDocument(PrefabGuid prefabGuid, uint32 levIndex,
		const Shared<GameObject>& selectedRoot) const;
	HRESULT DeSerializePrefabDocument(PrefabGuid prefabGuid, uint32 levIndex,
		Shared<GameObject>& outRoot) const;
	HRESULT Prepare_RestoredObjectGuid(ObjectGuid objectGuid);
	ObjectGuid Consume_RestoredObjectGuid() noexcept;

private:
	unordered_map<PrefabGuid, wstring, GuidHash> m_PrefabPaths;
	ObjectGuid m_PendingObjectGuid{};
};

NS_END
