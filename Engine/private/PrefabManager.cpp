#include "PrefabManager.h"
#include "Game.h"
#include "LevelSerializer.h"
#include <fstream>
#include <optional>

using namespace Engine;

namespace
{
	optional<wstring> Normalize_Path(const wstring& filePath)
	{
		if (filePath.empty())
			return nullopt;
		std::error_code errorCode;
		filesystem::path normalized = filesystem::weakly_canonical(filePath, errorCode);
		if (errorCode)
		{
			errorCode.clear();
			normalized = filesystem::absolute(filePath, errorCode).lexically_normal();
		}
		return errorCode || normalized.empty()
			? nullopt
			: optional<wstring>{ normalized.wstring() };
	}

	Bool Same_Path(const wstring& lhs, const wstring& rhs)
	{
		return _wcsicmp(lhs.c_str(), rhs.c_str()) == 0;
	}

	HRESULT Read_PrefabHeader(const wstring& filePath, PrefabGuid& outPrefabGuid)
	{
		outPrefabGuid = {};
		try
		{
			ifstream input(filePath);
			if (!input.is_open())
				return E_FAIL;

			nlohmann::json document;
			input >> document;
			if (!document.is_object() || document.value("schemaVersion", 0u) != 2u ||
				document.value("documentType", string{}) != "Prefab" ||
				!document.contains("prefabGuid") || !document["prefabGuid"].is_string() ||
				!Try_Parse_PrefabGuid(document["prefabGuid"].get<string>(), outPrefabGuid) ||
				!outPrefabGuid.Is_Valid())
				return E_FAIL;
			return S_OK;
		}
		catch (const std::exception&)
		{
			return E_FAIL;
		}
	}
}

HRESULT PrefabManager::Register_Prefab(PrefabGuid prefabGuid, const wstring& filePath)
{
	if (!prefabGuid.Is_Valid() || filePath.empty())
		return E_INVALIDARG;
	const optional<wstring> normalizedPath = Normalize_Path(filePath);
	if (!normalizedPath)
		return E_FAIL;
	for (const auto& [registeredGuid, registeredPath] : m_PrefabPaths)
	{
		if (registeredGuid != prefabGuid && Same_Path(registeredPath, *normalizedPath))
			return E_FAIL;
	}

	const auto [it, inserted] = m_PrefabPaths.emplace(prefabGuid, *normalizedPath);
	if (inserted)
		return S_OK;
	return Same_Path(it->second, *normalizedPath) ? S_FALSE : E_FAIL;
}

HRESULT PrefabManager::Register_PrefabDocument(const wstring& filePath,
	PrefabGuid& outPrefabGuid)
{
	outPrefabGuid = {};
	if (filePath.empty())
		return E_INVALIDARG;

	PrefabGuid prefabGuid{};
	if (FAILED(Read_PrefabHeader(filePath, prefabGuid)))
		return E_FAIL;
	const HRESULT registerResult = Register_Prefab(prefabGuid, filePath);
	if (FAILED(registerResult))
		return registerResult;
	outPrefabGuid = prefabGuid;
	return S_OK;
}

HRESULT PrefabManager::Load_PrefabRepository(const wstring& directoryPath)
{
	if (directoryPath.empty())
		return E_INVALIDARG;

	std::error_code errorCode;
	const filesystem::path directory(directoryPath);
	if (!filesystem::exists(directory, errorCode))
	{
		if (errorCode)
			return E_FAIL;
		m_PrefabPaths.clear();
		return S_OK;
	}
	if (!filesystem::is_directory(directory, errorCode) || errorCode)
		return E_FAIL;

	vector<filesystem::path> documents;
	for (filesystem::directory_iterator it(directory, errorCode), end;
		!errorCode && it != end; it.increment(errorCode))
	{
		const filesystem::directory_entry& entry = *it;
		if (!entry.is_regular_file(errorCode))
		{
			if (errorCode)
				break;
			continue;
		}
		if (_wcsicmp(entry.path().extension().c_str(), L".json") == 0)
			documents.push_back(entry.path());
	}
	if (errorCode)
		return E_FAIL;
	std::ranges::sort(documents, [](const filesystem::path& lhs,
		const filesystem::path& rhs) {
		return _wcsicmp(lhs.c_str(), rhs.c_str()) < 0;
	});

	unordered_map<PrefabGuid, wstring, GuidHash> stagedPaths;
	for (const filesystem::path& document : documents)
	{
		const optional<wstring> normalizedPath = Normalize_Path(document.wstring());
		PrefabGuid prefabGuid{};
		if (!normalizedPath || FAILED(Read_PrefabHeader(*normalizedPath, prefabGuid)))
			return E_FAIL;
		const auto [it, inserted] = stagedPaths.emplace(prefabGuid, *normalizedPath);
		if (!inserted && !Same_Path(it->second, *normalizedPath))
			return E_FAIL;
	}

	m_PrefabPaths = std::move(stagedPaths);
	return S_OK;
}

HRESULT PrefabManager::Unregister_Prefab(PrefabGuid prefabGuid)
{
	if (!prefabGuid.Is_Valid())
		return E_INVALIDARG;
	return m_PrefabPaths.erase(prefabGuid) == 1 ? S_OK : S_FALSE;
}

wstring PrefabManager::Find_PrefabPath(PrefabGuid prefabGuid) const
{
	const auto it = m_PrefabPaths.find(prefabGuid);
	return it == m_PrefabPaths.end() ? wstring{} : it->second;
}

vector<pair<PrefabGuid, wstring>> PrefabManager::Get_PrefabDocuments() const
{
	vector<pair<PrefabGuid, wstring>> documents(
		m_PrefabPaths.begin(), m_PrefabPaths.end());
	std::ranges::sort(documents, [](const auto& lhs, const auto& rhs) {
		return _wcsicmp(lhs.second.c_str(), rhs.second.c_str()) < 0;
	});
	return documents;
}

HRESULT PrefabManager::SerializePrefabDocument(PrefabGuid prefabGuid, uint32 levIndex,
	const Shared<GameObject>& selectedRoot) const
{
	const wstring filePath = Find_PrefabPath(prefabGuid);
	return !filePath.empty() && selectedRoot
		? GAME_INSTANCE->m_LevelSerializer->SerializePrefab(
			prefabGuid, levIndex, selectedRoot, filePath)
		: E_FAIL;
}

HRESULT PrefabManager::DeSerializePrefabDocument(PrefabGuid prefabGuid, uint32 levIndex,
	Shared<GameObject>& outRoot) const
{
	outRoot.reset();
	const wstring filePath = Find_PrefabPath(prefabGuid);
	return !filePath.empty()
		? GAME_INSTANCE->m_LevelSerializer->DeSerializePrefab(
			prefabGuid, levIndex, filePath, outRoot)
		: E_FAIL;
}

HRESULT PrefabManager::Prepare_RestoredObjectGuid(ObjectGuid objectGuid)
{
	if (!objectGuid.Is_Valid())
		return E_INVALIDARG;
	if (m_PendingObjectGuid.Is_Valid())
		return E_FAIL;
	m_PendingObjectGuid = objectGuid;
	return S_OK;
}

ObjectGuid PrefabManager::Consume_RestoredObjectGuid() noexcept
{
	const ObjectGuid objectGuid = m_PendingObjectGuid;
	m_PendingObjectGuid = {};
	return objectGuid;
}
