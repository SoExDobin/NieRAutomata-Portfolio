#include "pch.h"
#include "Converter.h"

#include <cwctype>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	namespace fs = std::filesystem;

	const std::vector<std::wstring> SupportedExtensions{
		L".fbx", L".obj", L".gltf", L".glb"
	};

	std::string ToUtf8(const fs::path& path)
	{
		const std::u8string utf8 = path.u8string();
		return { utf8.begin(), utf8.end() };
	}

	Bool IsSupportedAsset(const fs::path& path)
	{
		std::wstring extension = path.extension().wstring();
		std::transform(extension.begin(), extension.end(), extension.begin(),
			[](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });

		return std::find(SupportedExtensions.begin(), SupportedExtensions.end(), extension)
			!= SupportedExtensions.end();
	}

	void AddAssetFile(const fs::path& path, std::vector<fs::path>& assetFiles)
	{
		if (!IsSupportedAsset(path))
			return;

		if (std::find(assetFiles.begin(), assetFiles.end(), path) == assetFiles.end())
			assetFiles.push_back(path);
	}

	Bool CollectAssetFiles(const fs::path& inputPath, std::vector<fs::path>& assetFiles)
	{
		std::error_code errorCode;
		fs::path normalizedPath = fs::weakly_canonical(inputPath, errorCode);
		if (errorCode)
			normalizedPath = fs::absolute(inputPath, errorCode).lexically_normal();

		if (errorCode || !fs::exists(normalizedPath))
		{
			std::cerr << "[Error] Path not found: " << ToUtf8(inputPath) << "\n";
			return false;
		}

		if (fs::is_regular_file(normalizedPath))
		{
			if (!IsSupportedAsset(normalizedPath))
			{
				std::cerr << "[Skip] Unsupported file: " << ToUtf8(normalizedPath) << "\n";
				return false;
			}

			AddAssetFile(normalizedPath, assetFiles);
			return true;
		}

		if (!fs::is_directory(normalizedPath))
		{
			std::cerr << "[Error] Unsupported path type: " << ToUtf8(normalizedPath) << "\n";
			return false;
		}

		fs::recursive_directory_iterator iterator{
			normalizedPath,
			fs::directory_options::skip_permission_denied,
			errorCode
		};
		const fs::recursive_directory_iterator end{};

		while (!errorCode && iterator != end)
		{
			if (iterator->is_regular_file(errorCode) && !errorCode)
				AddAssetFile(iterator->path(), assetFiles);

			iterator.increment(errorCode);
		}

		if (errorCode)
		{
			std::cerr << "[Error] Failed to scan directory: " << ToUtf8(normalizedPath)
				<< " (" << errorCode.message() << ")\n";
			return false;
		}

		return true;
	}
}

int wmain(int argc, wchar_t* argv[])
{
	SetConsoleOutputCP(CP_UTF8);

	const Bool animationOnly = argc >= 2 && std::wstring_view{ argv[1] } == L"--animation-only";
	const int32 firstInputIndex = animationOnly ? 2 : 1;
	std::vector<fs::path> assetFiles;
	int32 inputFailures = 0;

	if (argc > firstInputIndex)
	{
		for (int32 i = firstInputIndex; i < argc; ++i)
		{
			if (!CollectAssetFiles(fs::path{ argv[i] }, assetFiles))
				++inputFailures;
		}
	}
	else if (!animationOnly)
	{
		const fs::path defaultResourcesPath = fs::current_path() / L"../Client/bin/resources";
		if (!CollectAssetFiles(defaultResourcesPath, assetFiles))
			++inputFailures;
	}

	if (assetFiles.empty())
	{
		std::cerr << "[Error] No supported asset files were found.\n";
		std::cerr << "Usage: Drag FBX files onto ModelConverter.exe\n";
		std::cerr << "   or: ModelConverter.exe <asset_file_or_directory> [...]\n";
		std::cerr << "   or: ModelConverter.exe --animation-only <asset_file_or_directory> [...]\n";
		std::cin.get();
		return -1;
	}

	std::cout << "========================================\n";
	std::cout << " ModelConverter\n";
	std::cout << " Mode : " << (animationOnly ? "Animation Only" : "Model + Animation") << "\n";
	std::cout << " Files: " << assetFiles.size() << "\n";
	std::cout << "========================================\n\n";

	int32 success = 0;
	int32 fail = inputFailures;

	for (const fs::path& assetPath : assetFiles)
	{
		fs::path outputPath = assetPath;
		outputPath.replace_extension(L".model");

		std::cout << "[Convert] " << ToUtf8(assetPath) << "\n";

		Tool::Converter converter;
		converter.Initialize();

		if (!converter.ReadAssetFile(assetPath.wstring()))
		{
			std::cerr << "  [FAIL] ReadAssetFile\n\n";
			++fail;
			continue;
		}

		std::cout << "  Meshes       : " << converter.GetMeshCount() << "\n";
		std::cout << "  Materials    : " << converter.GetMaterialCount() << "\n";
		std::cout << "  Bones        : " << converter.GetBoneCount() << "\n";
		std::cout << "  Animations   : " << converter.GetAnimationCount() << "\n";
		std::cout << "  Mesh / Lod / Shadow / Col / Inst : "
			<< converter.GetRoleMeshCount(NIER_FBX_ROLE::Mesh) << " / "
			<< converter.GetRoleMeshCount(NIER_FBX_ROLE::Lod) << " / "
			<< converter.GetRoleMeshCount(NIER_FBX_ROLE::Shadow) << " / "
			<< converter.GetRoleMeshCount(NIER_FBX_ROLE::Col) << " / "
			<< converter.GetRoleMeshCount(NIER_FBX_ROLE::Instance) << "\n";

		const Bool exported = animationOnly
			? converter.ExportAnimations(outputPath.wstring())
			: converter.ExportModel(outputPath.wstring());

		if (!exported)
		{
			std::cerr << "  [FAIL] "
				<< (animationOnly ? "ExportAnimations" : "ExportModel") << "\n\n";
			++fail;
			continue;
		}

		if (animationOnly)
		{
			const fs::path animationDirectory =
				outputPath.parent_path() / L"Animations";
			std::cout << "  [OK] -> " << ToUtf8(animationDirectory) << "\n\n";
		}
		else
		{
			std::cout << "  [OK] -> " << ToUtf8(outputPath) << "\n\n";
		}
		++success;
	}

	std::cout << "========================================\n";
	std::cout << " Result\n";
	std::cout << "   Files     : " << success << " OK / " << fail << " FAIL\n";
	std::cout << "========================================\n";
	std::cin.get();

	return fail > 0 ? -1 : 0;
}
