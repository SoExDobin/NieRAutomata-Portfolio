#pragma once
#include <assimp/Importer.hpp>
#include <filesystem>

#include "Nier_Fbx_Rule.h"


NS_BEGIN(Engine)
struct BoneData;
struct MeshData;
struct MaterialData;
struct AnimationData;
NS_END

NS_BEGIN(Tool)
	class Converter final
{
public:
	explicit Converter() = default;
	~Converter() = default;

public:
	void Initialize();

public:
	uint32 GetMeshCount()     const { return static_cast<uint32>(m_Meshes.size()); }
	uint32 GetMaterialCount() const { return static_cast<uint32>(m_Material.size()); }
	uint32 GetBoneCount()     const { return static_cast<uint32>(m_Bones.size()); }
	uint32 GetAnimationCount()     const { return static_cast<uint32>(m_Animation.size()); }
	uint32 GetRoleMeshCount(NIER_FBX_ROLE role) const
	{
		return static_cast<uint32>(m_RoleBuckets[static_cast<uint32>(role)].size());
	}

public:
	Bool ReadAssetFile(const wstring& path);
	Bool ExportModel(const wstring& outPath);
	Bool ExportAnimations(const wstring& modelPath);

private:
	void ReadBoneData(aiNode* node, int32_t parentIndex);
	void ReadMeshData();
	void ReadMaterialData();
	/* Fills texture types the FBX lost from the `materials.json` beside the asset (see Nier_Fbx_Rule). */
	void ApplySourceMaterialTextures(const wstring& assetPath);
	void ReadAnimation();

private:
	/* Groups m_Meshes into m_RoleBuckets using Nier_Fbx_Rule. Every mesh lands in exactly one bucket. */
	void ClassifyMeshRoles();
	static wstring Build_RolePath(const wstring& basePath, NIER_FBX_ROLE role);

private:
	/* Writes only the meshes of `role`. The full material table is written into every role file so
	   mesh material indices stay valid without remapping. */
	Bool WriteModelFile(const wstring& path, NIER_FBX_ROLE role);
	Bool WriteAnimationFiles(const wstring& modelPath);
	Bool WriteAnimationFile(const std::filesystem::path& path, uint32 animationIndex);
	Bool WriteJsonFile(const wstring& path);

private:
	int32 Get_BoneIndex(const Char* boneName);

private:
	Shared<Assimp::Importer>		m_Importer{nullptr};
	const aiScene*					m_AiScene{nullptr};

	Bool m_IsSkeletal{ false };
	vector<Shared<MODEL_BONE>>					m_Bones;
	vector<Shared<MODEL_MESH>>					m_Meshes;
	/* Parallel to m_Meshes; m_RoleBuckets holds indices into m_Meshes per role. */
	vector<Nier_Fbx_Rule::Classification>		m_MeshRoles;
	vector<uint32>								m_RoleBuckets[static_cast<uint32>(NIER_FBX_ROLE::Count)];
	vector<Shared<MODEL_MATERIAL>>				m_Material;
	vector<Shared<MODEL_ANIMATION>>				m_Animation;
	vector<vector<Shared<MODEL_CHANNEL>>>		m_Channels;

public: /* static */
	static Vector4 ToFloat4(const aiColor4D& aiColor)
	{
		return { aiColor.r, aiColor.g, aiColor.b, aiColor.a };
	}
};

NS_END

/*
	typedef struct ChannelData
	{
		std::string name;
		uint32 numKeyFrames;
		std::vector<KEYFRAME> keyFrames;
	} MODEL_CHANNEL;

	typedef struct AnimationData
	{
		std::string name;
		Float duration;
		Float tickPerSecond;
		uint32 numChannel;
		std::vector<MODEL_CHANNEL> channels;
	} MODEL_ANIMATION;

 */
