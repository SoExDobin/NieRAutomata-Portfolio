#pragma once
#include <filesystem>

NS_BEGIN(Editor)

/* Authored scene and prefab documents are overwritten in place by the Editor's own Save.
   A single bad write used to be unrecoverable outside git, which is exactly how 1193
   animation references were lost on 2026-09-17. Every authored write now copies the
   existing file aside first. */
class AuthoringBackup final
{
public:
	/* Copies `target` to ProjectSetting/Backup/<stem>.<yyyyMMdd-HHmmss><ext> and prunes
	   older backups of the same stem beyond KeepPerStem.

	   Returns true when it is safe to proceed with the write: either a backup was taken,
	   or `target` does not exist yet so there is nothing to lose. Returns false only when
	   an existing file could not be backed up - callers must then skip the write. */
	static Bool Capture(const std::filesystem::path& target);

	static constexpr size_t KeepPerStem = 10;

private:
	static std::filesystem::path Backup_Directory();
	static std::wstring Timestamp();
	static void Prune(const std::filesystem::path& directory, const std::wstring& stem);
};

NS_END
