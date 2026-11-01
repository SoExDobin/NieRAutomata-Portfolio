#include "pch.h"
#include "AuthoringBackup.h"

#include "PathManager.h"
#include "SpdLogger.h"

#include <algorithm>
#include <chrono>
#include <vector>

using namespace Editor;
namespace fs = std::filesystem;

fs::path AuthoringBackup::Backup_Directory()
{
	return fs::path(PATH.GetProjectSettingDir()) / L"Backup";
}

std::wstring AuthoringBackup::Timestamp()
{
	const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	std::tm local{};
	localtime_s(&local, &now);

	wchar_t buffer[32]{};
	std::wcsftime(buffer, std::size(buffer), L"%Y%m%d-%H%M%S", &local);
	return buffer;
}

void AuthoringBackup::Prune(const fs::path& directory, const std::wstring& stem)
{
	std::error_code error;
	std::vector<fs::path> matches;

	for (const fs::directory_entry& entry : fs::directory_iterator(directory, error))
	{
		if (error)
			return;
		if (!entry.is_regular_file(error) || error)
			continue;

		/* Backups are "<stem>.<timestamp><ext>", so the stem of the backup's stem is the
		   original stem. Anything else in the folder is left alone. */
		const fs::path withoutExtension = entry.path().stem();
		if (withoutExtension.stem().wstring() == stem)
			matches.push_back(entry.path());
	}

	if (matches.size() <= KeepPerStem)
		return;

	/* Timestamps are fixed width and zero padded, so lexical order is chronological. */
	std::sort(matches.begin(), matches.end());
	const size_t removeCount = matches.size() - KeepPerStem;
	for (size_t i = 0; i < removeCount; ++i)
		fs::remove(matches[i], error);
}

Bool AuthoringBackup::Capture(const fs::path& target)
{
	std::error_code error;
	if (!fs::exists(target, error) || error)
		return true; /* nothing to lose yet */

	const fs::path directory = Backup_Directory();
	fs::create_directories(directory, error);
	if (error)
	{
		LOG_ERROR(L"[AuthoringBackup] Could not create {} ({})",
			directory.wstring(), Helper::To_wString(error.message()));
		return false;
	}

	fs::path backupPath = directory /
		(target.stem().wstring() + L"." + Timestamp() + target.extension().wstring());

	/* Two saves inside the same second must not collide silently. */
	for (uint32 suffix = 1; fs::exists(backupPath, error) && suffix < 100; ++suffix)
	{
		backupPath = directory / (target.stem().wstring() + L"." + Timestamp() +
			L"-" + std::to_wstring(suffix) + target.extension().wstring());
	}

	fs::copy_file(target, backupPath, fs::copy_options::overwrite_existing, error);
	if (error)
	{
		LOG_ERROR(L"[AuthoringBackup] Could not back up {} ({})",
			target.wstring(), Helper::To_wString(error.message()));
		return false;
	}

	Prune(directory, target.stem().wstring());
	LOG_INFO(L"[AuthoringBackup] {} -> {}", target.filename().wstring(), backupPath.filename().wstring());
	return true;
}
