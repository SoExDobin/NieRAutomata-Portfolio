#include "pch.h"
#include "Converter.h"
#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <set>

using namespace Tool;

void Tool::Converter::Initialize()
{
	if (m_Importer) return;
	m_Importer = make_shared<Assimp::Importer>();
}

Bool Tool::Converter::ReadAssetFile(const wstring& path)
{
	m_Bones.clear();
	m_Meshes.clear();
	m_MeshRoles.clear();
	for (auto& bucket : m_RoleBuckets)
		bucket.clear();
	m_Material.clear();
	m_Animation.clear();
	m_Channels.clear();

	const std::u8string utf8Path = filesystem::path(path).u8string();
	const string modelFilePath{ utf8Path.begin(), utf8Path.end() };

	m_AiScene = m_Importer->ReadFile(
		modelFilePath,
		aiProcess_ConvertToLeftHanded |
		aiProcess_Triangulate |
		aiProcess_GenNormals |
		aiProcess_CalcTangentSpace
	);

	if (!m_AiScene || m_AiScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)
	{
		std::cerr << "[Assimp] Error: " << m_Importer->GetErrorString() << "\n";
		return false;
	}
	std::cout << "  mNumMeshes=" << m_AiScene->mNumMeshes << " mNumMaterials=" << m_AiScene->mNumMaterials << "\n";

	m_IsSkeletal = false;
	for (uint32 i = 0; i < m_AiScene->mNumMeshes; ++i)
	{
		if (m_AiScene->mMeshes[i]->HasBones())
		{
			m_IsSkeletal = true;
			break;
		}
	}

	ReadBoneData(m_AiScene->mRootNode, -1);
	ReadMeshData();
	ReadMaterialData();
	// After the materials: a mesh drawn with the shadow material is classified by its material name.
	ClassifyMeshRoles();
	ApplySourceMaterialTextures(path);
	ReadAnimation();

	return true;
}

void Tool::Converter::ApplySourceMaterialTextures(const wstring& assetPath)
{
	const filesystem::path assetDirectory = filesystem::path(assetPath).parent_path();
	const filesystem::path referencePath = assetDirectory / L"materials.json";
	if (!filesystem::is_regular_file(referencePath))
		return;

	nlohmann::json referenceMaterials;
	try
	{
		ifstream referenceFile(referencePath);
		referenceMaterials = nlohmann::json::parse(referenceFile);
	}
	catch (const std::exception& exception)
	{
		std::cerr << "  [Nier] materials.json unreadable, textures left as exported: " << exception.what() << "\n";
		return;
	}
	if (!referenceMaterials.is_object())
		return;

	uint32 filledMaterials = 0;
	uint32 addedTextures = 0;
	uint32 missingFiles = 0;
	for (const Shared<MODEL_MATERIAL>& material : m_Material)
	{
		const auto sourceIt = referenceMaterials.find(material->name);
		if (sourceIt == referenceMaterials.end())
			continue;

		Bool filled = false;
		for (const MODEL_ENTRY& entry : Nier_Fbx_Rule::Resolve_SourceTextures(*sourceIt))
		{
			/* What the FBX did carry stays authoritative. */
			const Bool hasType = std::ranges::any_of(material->textures,
				[&](const MODEL_ENTRY& existing) { return existing.typeIndex == entry.typeIndex; });
			if (hasType)
				continue;

			if (!filesystem::is_regular_file(assetDirectory / entry.path))
			{
				std::cerr << "  [Nier] Missing texture " << entry.path << " for material " << material->name << "\n";
				++missingFiles;
				continue;
			}

			material->textures.push_back(entry);
			++addedTextures;
			filled = true;
		}
		if (filled)
			++filledMaterials;
	}

	std::cout << "  [Nier] materials.json textures: +" << addedTextures << " on "
		<< filledMaterials << "/" << m_Material.size() << " materials, " << missingFiles << " missing files\n";
}

void Tool::Converter::ClassifyMeshRoles()
{
	m_MeshRoles.clear();
	m_MeshRoles.reserve(m_Meshes.size());
	for (auto& bucket : m_RoleBuckets)
		bucket.clear();

	for (uint32 i = 0; i < static_cast<uint32>(m_Meshes.size()); ++i)
	{
		const auto& mesh = m_Meshes[i];
		const Bool validMaterialIndex = mesh && mesh->materialIndex >= 0 &&
			static_cast<size_t>(mesh->materialIndex) < m_Material.size() && m_Material[mesh->materialIndex];
		const Nier_Fbx_Rule::Classification classification = Nier_Fbx_Rule::Classify(
			mesh ? mesh->name : string{}, validMaterialIndex ? m_Material[mesh->materialIndex]->name : string{});

		m_MeshRoles.push_back(classification);
		m_RoleBuckets[static_cast<uint32>(classification.role)].push_back(i);
	}

	std::cout << "  Role split  :";
	for (uint32 role = 0; role < static_cast<uint32>(NIER_FBX_ROLE::Count); ++role)
	{
		std::cout << " " << Nier_Fbx_Rule::Get_RoleName(static_cast<NIER_FBX_ROLE>(role))
			<< "=" << m_RoleBuckets[role].size();
	}
	std::cout << "\n";
}

wstring Tool::Converter::Build_RolePath(const wstring& basePath, NIER_FBX_ROLE role)
{
	const Char* suffix = Nier_Fbx_Rule::Get_RoleSuffix(role);
	if ('\0' == suffix[0])
		return basePath;

	filesystem::path path{ basePath };
	const wstring stem = path.stem().wstring();
	const string narrowSuffix{ suffix };
	const wstring wideSuffix{ narrowSuffix.begin(), narrowSuffix.end() };

	path.replace_filename(stem + wideSuffix + path.extension().wstring());
	return path.wstring();
}

Bool Tool::Converter::ExportModel(const wstring& outPath)
{
	if (m_Meshes.empty())
	{
		std::cerr << "  [Warning] Mesh 없음, 스킵\n";
		return false;
	}

	for (const auto& mesh : m_Meshes)
	{
		if (!mesh || mesh->materialIndex < 0 ||
			static_cast<size_t>(mesh->materialIndex) >= m_Material.size())
		{
			std::cerr << "  [Audit] Invalid mesh material index: "
				<< (mesh ? mesh->name : "<null>") << "\n";
			return false;
		}
	}

	/* One .model per occupied role so the engine can apply render, LOD, collision and
	   placement data independently. A source file that only ever produces one role
	   (an LOD-only map, a collision-only export) simply writes that one file. */
	uint32 writtenRoles = 0;
	for (uint32 role = 0; role < static_cast<uint32>(NIER_FBX_ROLE::Count); ++role)
	{
		if (m_RoleBuckets[role].empty())
			continue;

		const NIER_FBX_ROLE typedRole = static_cast<NIER_FBX_ROLE>(role);
		const wstring rolePath = Build_RolePath(outPath, typedRole);

		if (!WriteModelFile(rolePath, typedRole))
			return false;

		std::cout << "  [Write] " << Nier_Fbx_Rule::Get_RoleName(typedRole)
			<< " meshes=" << m_RoleBuckets[role].size() << "\n";
		++writtenRoles;
	}

	if (0 == writtenRoles)
	{
		std::cerr << "  [Error] No mesh role produced an output file\n";
		return false;
	}

	if (!WriteAnimationFiles(outPath))
		return false;

	// JSON 파일 경로는 확장자만 변경
	wstring jsonPath = filesystem::path(outPath).replace_extension(L".json").wstring();
	return WriteJsonFile(jsonPath);
}

Bool Tool::Converter::ExportAnimations(const wstring& modelPath)
{
	if (m_Animation.empty())
	{
		std::cerr << "  [Warning] Animation not found, skipped\n";
		return false;
	}

	return WriteAnimationFiles(modelPath);
}

void Tool::Converter::ReadBoneData(aiNode* node, int32_t parentIndex)
{
	auto bone = make_shared<MODEL_BONE>();
	bone->name = node->mName.C_Str();
	bone->parentIndex = parentIndex;

	auto m = node->mTransformation;
	Matrix matrix( // Transpose() 재배열
		m.a1, m.b1, m.c1, m.d1,
		m.a2, m.b2, m.c2, m.d2,
		m.a3, m.b3, m.c3, m.d3,
		m.a4, m.b4, m.c4, m.d4
	);
	bone->transform = matrix;

	std::cout << "========================================\n";
	std::cout << "Bone Scan: " << bone->name << "\n";
	m_Bones.push_back(bone);

	size_t boneIndex = m_Bones.size() - 1;
	for (uint32 i = 0; i < node->mNumChildren; ++i) {
		ReadBoneData(node->mChildren[i], static_cast<int32_t>(boneIndex));
	}
}

void Tool::Converter::ReadMeshData()
{
	for (uint32 i = 0; i < m_AiScene->mNumMeshes; ++i)
	{
		auto aiMesh = m_AiScene->mMeshes[i];
		auto mesh = make_shared<MODEL_MESH>();
		
		mesh->name = aiMesh->mName.C_Str();
		mesh->materialIndex = aiMesh->mMaterialIndex;

		/* Animation / Bone Info */
		if (m_IsSkeletal)
		{
			mesh->animVertices.resize(aiMesh->mNumVertices);
			memset(mesh->animVertices.data(), 0, sizeof(VTXANIMMESH) * aiMesh->mNumVertices);

			for (uint32 j = 0; j < aiMesh->mNumVertices; ++j)
			{
				VTXANIMMESH& vtxMesh = mesh->animVertices[j];
				auto aiPos = aiMesh->mVertices[j];
				vtxMesh.position = Vector3{ aiPos.x, aiPos.y, aiPos.z };

				if (aiMesh->HasNormals()) {
					auto aiNorm = aiMesh->mNormals[j];
					vtxMesh.normal = Vector3{ aiNorm.x, aiNorm.y, aiNorm.z };
				}

				if (aiMesh->HasTangentsAndBitangents()) {
					auto aiTangent = aiMesh->mTangents[j];
					vtxMesh.tangent = Vector3{ aiTangent.x, aiTangent.y, aiTangent.z };
				}

				if (aiMesh->HasTextureCoords(0)) {
					auto aiTexcoord = aiMesh->mTextureCoords[0][j];
					vtxMesh.texcoord = Vector2{ aiTexcoord.x, aiTexcoord.y };
				}
			}

			for (uint32 j = 0; j < aiMesh->mNumBones; ++j)
			{
				aiBone* aiBone = aiMesh->mBones[j];
				
				// 1. 글로벌 본 리스트에서 해당 본의 인덱스 찾기
				int32 globalBoneIndex = Get_BoneIndex(aiBone->mName.C_Str());
				mesh->boneIndices.push_back(static_cast<uint32>(globalBoneIndex));
				if (-1 == globalBoneIndex)
				{
					std::cerr << "[Assimp] Error: Bone Index Error index was -1" << "\n";
					return;
				}

				// 2. 본인덱스 차례의 오프셋 매트릭스 
				Matrix offsetMatrix;
				memcpy(&offsetMatrix, &aiBone->mOffsetMatrix, sizeof(Matrix));
				mesh->offsetMatrices.push_back(offsetMatrix.Transpose());

				// 3. Blend인덱스와 가중치 업데이트
				for (uint32 k = 0; k < aiBone->mNumWeights; ++k)
				{
					aiVertexWeight vertexWeight = aiBone->mWeights[k];
					uint32 weightIndex = vertexWeight.mVertexId;

					if (mesh->animVertices[weightIndex].blendWeight.x == 0.f) {
						mesh->animVertices[weightIndex].blendIndex.x = j;
						mesh->animVertices[weightIndex].blendWeight.x = vertexWeight.mWeight;
					}
					else if (mesh->animVertices[weightIndex].blendWeight.y == 0.f) {
						mesh->animVertices[weightIndex].blendIndex.y = j;
						mesh->animVertices[weightIndex].blendWeight.y = vertexWeight.mWeight;
					}
					else if (mesh->animVertices[weightIndex].blendWeight.z == 0.f) {
						mesh->animVertices[weightIndex].blendIndex.z = j;
						mesh->animVertices[weightIndex].blendWeight.z = vertexWeight.mWeight;
					}
					else if (mesh->animVertices[weightIndex].blendWeight.w == 0.f) {
						mesh->animVertices[weightIndex].blendIndex.w = j;
						mesh->animVertices[weightIndex].blendWeight.w = vertexWeight.mWeight;
					}
				}
			}
		}
		else
		{
			mesh->vertices.resize(aiMesh->mNumVertices);
			memset(mesh->vertices.data(), 0, sizeof(VTXMESH) * aiMesh->mNumVertices);

			for (uint32 j = 0; j < aiMesh->mNumVertices; ++j)
			{
				VTXMESH& vtxMesh = mesh->vertices[j];
				auto aiPos = aiMesh->mVertices[j];
				vtxMesh.position = Vector3{ aiPos.x, aiPos.y, aiPos.z };

				if (aiMesh->HasNormals()) {
					auto aiNorm = aiMesh->mNormals[j];
					vtxMesh.normal = Vector3{ aiNorm.x, aiNorm.y, aiNorm.z };
				}

				if (aiMesh->HasTangentsAndBitangents()) {
					auto aiTangent = aiMesh->mTangents[j];
					vtxMesh.tangent = Vector3{ aiTangent.x, aiTangent.y, aiTangent.z };
				}

				if (aiMesh->HasTextureCoords(0)) {
					auto aiTexcoord = aiMesh->mTextureCoords[0][j];
					vtxMesh.texcoord = Vector2{ aiTexcoord.x, aiTexcoord.y };
				}

				// MODEL_VERSION 3: second UV set (NieR UVMap2) and vertex color (NieR "Col", PBS10 layer weights).
				// Without them the second UV repeats the first and the color is white.
				vtxMesh.texcoord1 = vtxMesh.texcoord;
				if (aiMesh->HasTextureCoords(1)) {
					auto aiTexcoord = aiMesh->mTextureCoords[1][j];
					vtxMesh.texcoord1 = Vector2{ aiTexcoord.x, aiTexcoord.y };
				}
				vtxMesh.color = 0xFFFFFFFFu;
				if (aiMesh->HasVertexColors(0)) {
					const aiColor4D& c = aiMesh->mColors[0][j];
					auto channel = [](float value) {
						return static_cast<uint32>(std::clamp(value, 0.f, 1.f) * 255.f + 0.5f);
					};
					vtxMesh.color = channel(c.r) | (channel(c.g) << 8) | (channel(c.b) << 16) | (channel(c.a) << 24);
				}
			}
		}

		/* indices */
		for (uint32 j = 0; j < aiMesh->mNumFaces; ++j)
		{
			const aiFace& face = aiMesh->mFaces[j];
			for (uint32 k = 0; k < face.mNumIndices; ++k)
			{
				mesh->indices.push_back(face.mIndices[k]);
			}
		}

		std::cout << "========================================\n";
		std::cout << "Mesh Scan: " << mesh->name << "\n";

		m_Meshes.push_back(mesh);
	}
}

void Tool::Converter::ReadMaterialData()
{
	for (uint32 i = 0; i < m_AiScene->mNumMaterials; ++i)
	{
		const aiMaterial* aiMat = m_AiScene->mMaterials[i];
		auto mat = make_shared<MODEL_MATERIAL>();

		aiString name;
		aiMat->Get(AI_MATKEY_NAME, name);
		mat->name = name.C_Str();

		for (uint32 t = 0; t < AI_TEXTURE_TYPE_MAX; ++t)
		{
			const aiTextureType texType = static_cast<aiTextureType>(t);
			const uint32        numTex = aiMat->GetTextureCount(texType);

			if (0 == numTex) continue;

			for (uint32 j = 0; j < numTex; ++j)
			{
				aiString texPath;
				if (AI_SUCCESS != aiMat->GetTexture(texType, j, &texPath))
					continue;

				MODEL_ENTRY entry;
				entry.typeIndex = t;

				// 절대경로 혼재 방지: 순수 파일명(ex: "diffuse.dds")만 파싱해서 저장
				string fullPath = texPath.C_Str();
				size_t lastPos = fullPath.find_last_of("\\/"); // 윈도우/리눅스 구분자 모두 체크
				if (lastPos != string::npos)
					entry.path = fullPath.substr(lastPos + 1);
				else
					entry.path = fullPath;

				if (entry.path.find(".dds") == string::npos && entry.path.find(".DDS") == string::npos)
				{
					entry.path += ".dds";
				}
				entry.path = "Textures/" + entry.path;

				mat->textures.push_back(entry);
			}
		}

		m_Material.push_back(mat);
	}
}

void Converter::ReadAnimation()
{
	m_Channels.resize(m_AiScene->mNumAnimations);
	for (uint32 i = 0; i < m_AiScene->mNumAnimations; ++i)
	{
		const aiAnimation* aiAnim = m_AiScene->mAnimations[i];
		auto anim = make_shared<MODEL_ANIMATION>();

		anim->name = aiAnim->mName.C_Str();
		anim->duration = static_cast<Float>(aiAnim->mDuration);
		anim->tickPerSecond = static_cast<Float>(aiAnim->mTicksPerSecond);
		anim->numChannel = static_cast<uint32>(aiAnim->mNumChannels);
		anim->rootTotalRotation = Vector4{ 0.f, 0.f, 0.f, 1.f };
		

		// --- 루트 모션 추출 로직 수정 ---
		// 1. 애니메이션 채널들 중 계층 구조상 가장 최상위에 있는 본을 '루트 모션 본'으로 간주합니다.
		aiNodeAnim* rootMotionChannel = nullptr;
		int32 minParentIndex = 100000; // 부모 인덱스가 작을수록 최상위에 가까움 (-1이 최상위)

		for (uint32 k = 0; k < aiAnim->mNumChannels; ++k) {
			aiNodeAnim* aiChannel = aiAnim->mChannels[k];
			int32 boneIdx = Get_BoneIndex(aiChannel->mNodeName.C_Str());

			if (boneIdx != -1) {
				int32 parentIdx = m_Bones[boneIdx]->parentIndex;
				// 가장 부모에 가까운 본 채널을 선택 (보통 parentIndex가 -1이거나 0, 1 정도인 본)
				if (parentIdx < minParentIndex) {
					minParentIndex = parentIdx;
					rootMotionChannel = aiChannel;
					if (minParentIndex == -1) break; // 완벽한 루트를 찾으면 즉시 종료
				}
			}
		}

		if (rootMotionChannel) {
			// 2. 선택된 루트 모션 본 채널에서 변위 계산
			if (rootMotionChannel->mNumPositionKeys > 0)
			{
				aiVector3D startPos = rootMotionChannel->mPositionKeys[0].mValue;
				aiVector3D endPos = rootMotionChannel->mPositionKeys[rootMotionChannel->mNumPositionKeys - 1].mValue;
				anim->rootTotalTranslation = Vector3(endPos.x - startPos.x, endPos.y - startPos.y, endPos.z - startPos.z);
			}

			if (rootMotionChannel->mNumRotationKeys > 0)
			{
				aiQuaternion startRot = rootMotionChannel->mRotationKeys[0].mValue;
				aiQuaternion endRot = rootMotionChannel->mRotationKeys[rootMotionChannel->mNumRotationKeys - 1].mValue;
				aiQuaternion startInverse = startRot;
				startInverse.Conjugate();
				aiQuaternion deltaRot = endRot * startInverse;
				anim->rootTotalRotation = Vector4(deltaRot.x, deltaRot.y, deltaRot.z, deltaRot.w);
			}

			// std::cout << "[RootMotion Found] Bone: " << rootMotionChannel->mNodeName.C_Str() << " (ParentIdx: " << minParentIndex << ")\n";
		}
		// -------------------------


		for (uint32 j = 0; j < aiAnim->mNumChannels; ++j)
		{
			const aiNodeAnim* aiChannel = aiAnim->mChannels[j];
			auto channel = make_shared<MODEL_CHANNEL>();

			channel->name = aiChannel->mNodeName.C_Str();
			channel->boneIndex = Get_BoneIndex(channel->name.c_str());
			uint32 numKeys = std::max(aiChannel->mNumPositionKeys, aiChannel->mNumRotationKeys);
			numKeys = std::max(numKeys, aiChannel->mNumScalingKeys);
			channel->numKeyFrames = numKeys;

			Float3 scale{ 1.f, 1.f, 1.f };
			Float4 rotation{ 0.f, 0.f, 0.f, 1.f };
			Float3 translation{};
			for (uint32 k = 0; k < numKeys; ++k)
			{
				KEYFRAME keyFrame{}; 
				if (aiChannel->mNumScalingKeys > k)
				{
					memcpy(&scale, &aiChannel->mScalingKeys[k].mValue, sizeof(Float3));
					keyFrame.trackPosition = static_cast<Float>(aiChannel->mScalingKeys[k].mTime);
				}
				if (aiChannel->mNumRotationKeys > k)
				{
					rotation.x = aiChannel->mRotationKeys[k].mValue.x;
					rotation.y = aiChannel->mRotationKeys[k].mValue.y;
					rotation.z = aiChannel->mRotationKeys[k].mValue.z;
					rotation.w = aiChannel->mRotationKeys[k].mValue.w;
					keyFrame.trackPosition = static_cast<Float>(aiChannel->mRotationKeys[k].mTime);
				}
				if (aiChannel->mNumPositionKeys > k)
				{
					memcpy(&translation, &aiChannel->mPositionKeys[k].mValue, sizeof(Float3));
					keyFrame.trackPosition = static_cast<Float>(aiChannel->mPositionKeys[k].mTime);
				}

				keyFrame.scale = scale;
				keyFrame.rotation = rotation;
				keyFrame.position = translation;

				channel->keyFrames.push_back(keyFrame);
			}
			m_Channels[i].push_back(channel);
		}

		std::cout << "========================================\n";
		std::cout << "Animation Scan: " << anim->name << "\n";

		m_Animation.push_back(anim);
	}
}

Bool Tool::Converter::WriteModelFile(const wstring& path, NIER_FBX_ROLE role)
{
	const vector<uint32>& bucket = m_RoleBuckets[static_cast<uint32>(role)];

	filesystem::create_directories(filesystem::path(path).parent_path());
	ofstream out(path, std::ios::binary);
	if (!out.is_open())
	{
		std::cerr << "  [Error] Failed to create model file\n";
		return false;
	}

	/* Header */
	MODEL_HEADER header{};
	memcpy(header.magic, "NMDL", 4);
	header.version = MODEL_VERSION;
	header.isAnim = m_IsSkeletal;
	header.numBones = static_cast<uint32>(m_Bones.size());
	header.numMeshes = static_cast<uint32>(bucket.size());
	header.numMaterials = static_cast<uint32>(m_Material.size());
	header.numAnimations = 0;
	out.write(BIN(&header), sizeof(header));

	/* Bones */
	for (auto& bone : m_Bones)
	{
		uint32 length = static_cast<uint32>(bone->name.size());
		out.write(BIN(&length), sizeof(uint32));
		out.write(bone->name.data(), length);
		out.write(BIN(&bone->parentIndex), sizeof(int32));
		out.write(BIN(&bone->transform), sizeof(Matrix));
	}

	/* Meshes */
	for (const uint32 meshIndex : bucket)
	{
		const auto& mesh = m_Meshes[meshIndex];
		uint32 nameLength = static_cast<uint32>(mesh->name.size());
		out.write(BIN(&nameLength), sizeof(uint32));
		out.write(mesh->name.data(), nameLength);
		out.write(BIN(&mesh->materialIndex), sizeof(uint32));

		uint32 numBoneIndex = static_cast<uint32>(mesh->boneIndices.size());
		out.write(BIN(&numBoneIndex), sizeof(uint32));
		if (numBoneIndex > 0)
		{
			out.write(BIN(mesh->boneIndices.data()), numBoneIndex * sizeof(uint32));
			uint32 numOffsetIndex = static_cast<uint32>(mesh->offsetMatrices.size());
			out.write(BIN(&numOffsetIndex), sizeof(uint32));
			out.write(BIN(mesh->offsetMatrices.data()), numOffsetIndex * sizeof(Matrix));
		}
		
		uint32 numVertex = (header.isAnim) ? static_cast<uint32>(mesh->animVertices.size()) : static_cast<uint32>(mesh->vertices.size());
		out.write(BIN(&numVertex), sizeof(uint32));
		if (header.isAnim)
			out.write(BIN(mesh->animVertices.data()), numVertex * sizeof(VTXANIMMESH));
		else
			out.write(BIN(mesh->vertices.data()), numVertex * sizeof(VTXMESH));
		
		uint32 numIndex = static_cast<uint32>(mesh->indices.size());
		out.write(BIN(&numIndex), sizeof(uint32));
		out.write(BIN(mesh->indices.data()), numIndex * sizeof(uint32));
	}

	/* Materials */
	uint32 maxTextureType = AI_TEXTURE_TYPE_MAX;
	for (auto& mat : m_Material)
	{
		uint32 nameLength = static_cast<uint32>(mat->name.size());
		out.write(BIN(&nameLength), sizeof(uint32));
		out.write(mat->name.data(), nameLength);
		out.write(BIN(&maxTextureType), sizeof(uint32));
		
		uint32 numTex = static_cast<uint32>(mat->textures.size());
		out.write(BIN(&numTex), sizeof(uint32));
		for (auto& texture : mat->textures)
		{
			uint32 pathLength = static_cast<uint32>(texture.path.size());
			out.write(BIN(&texture.typeIndex), sizeof(uint32));
			out.write(BIN(&pathLength), sizeof(uint32));
			out.write(texture.path.data(), pathLength);
		}
	}

	out.close();
	return out.good();
}

Bool Tool::Converter::WriteAnimationFiles(const wstring& modelPath)
{
	if (m_Animation.empty())
		return true;

	const filesystem::path modelFilePath{ modelPath };
	const filesystem::path animationDirectory =
		modelFilePath.parent_path() / L"Animations";

	std::error_code errorCode;
	filesystem::create_directories(animationDirectory, errorCode);
	if (errorCode)
	{
		std::cerr << "  [Error] Failed to create animation directory: " << errorCode.message() << "\n";
		return false;
	}

	unordered_map<string, uint32> usedNames;
	nlohmann::json manifest;
	manifest["schemaVersion"] = 1;
	manifest["model"] = modelFilePath.stem().string();
	manifest["animations"] = nlohmann::json::array();
	for (uint32 i = 0; i < m_Animation.size(); ++i)
	{
		string fileName = m_Animation[i]->name;
		for (Char& ch : fileName)
		{
			const unsigned char value = static_cast<unsigned char>(ch);
			if (value < 0x20 || ch == '<' || ch == '>' || ch == ':' || ch == '"' ||
				ch == '/' || ch == '\\' || ch == '|' || ch == '?' || ch == '*')
				ch = '_';
		}
		while (!fileName.empty() && (fileName.back() == ' ' || fileName.back() == '.'))
			fileName.pop_back();
		if (fileName.empty())
			fileName = "Animation_" + std::to_string(i);

		uint32& duplicateCount = usedNames[fileName];
		const string uniqueName = duplicateCount++ == 0
			? fileName
			: fileName + "_" + std::to_string(duplicateCount);

		const string animationFileName = uniqueName + ".anim";
		const filesystem::path animationPath = animationDirectory / filesystem::path(animationFileName);
		if (!WriteAnimationFile(animationPath, i))
			return false;

		manifest["animations"].push_back({
			{"index", i},
			{"name", m_Animation[i]->name},
			{"file", animationFileName}
		});
	}

	ofstream manifestFile(animationDirectory / "manifest.json");
	if (!manifestFile.is_open())
	{
		std::cerr << "  [Error] Failed to create animation manifest\n";
		return false;
	}
	manifestFile << manifest.dump(4);
	if (!manifestFile.good())
	{
		std::cerr << "  [Error] Failed to write animation manifest\n";
		return false;
	}

	return true;
}

Bool Tool::Converter::WriteAnimationFile(const filesystem::path& path, uint32 animationIndex)
{
	if (animationIndex >= m_Animation.size() || animationIndex >= m_Channels.size())
		return false;

	ofstream out(path, std::ios::binary);
	if (!out.is_open())
	{
		std::cerr << "  [Error] Failed to create animation file\n";
		return false;
	}

	const auto& anim = m_Animation[animationIndex];
	const auto& channels = m_Channels[animationIndex];

	ANIMATION_HEADER header{};
	memcpy(header.magic, ANIMATION_MAGIC, sizeof(header.magic));
	header.version = ANIMATION_VERSION;
	header.numChannels = static_cast<uint32>(channels.size());
	out.write(BIN(&header), sizeof(header));

	const uint32 animNameLength = static_cast<uint32>(anim->name.size());
	out.write(BIN(&animNameLength), sizeof(animNameLength));
	out.write(anim->name.data(), animNameLength);
	out.write(BIN(&anim->duration), sizeof(anim->duration));
	out.write(BIN(&anim->tickPerSecond), sizeof(anim->tickPerSecond));
	out.write(BIN(&anim->rootTotalTranslation), sizeof(anim->rootTotalTranslation));
	out.write(BIN(&anim->rootTotalRotation), sizeof(anim->rootTotalRotation));

	for (const auto& channel : channels)
	{
		const uint32 channelNameLength = static_cast<uint32>(channel->name.size());
		out.write(BIN(&channelNameLength), sizeof(channelNameLength));
		out.write(channel->name.data(), channelNameLength);
		out.write(BIN(&channel->numKeyFrames), sizeof(channel->numKeyFrames));
		if (channel->numKeyFrames > 0)
			out.write(BIN(channel->keyFrames.data()), channel->numKeyFrames * sizeof(Engine::KEYFRAME));
	}

	out.close();
	return out.good();
}

int32 Converter::Get_BoneIndex(const Char* boneName)
{
	int32 boneIndex{ -1 };
	// 인자 타입을 const MODEL_BONE& 에서 auto& 또는 const shared_ptr<MODEL_BONE>& 로 수정
	auto iter = find_if(m_Bones.begin(), m_Bones.end(), [&](const Shared<MODEL_BONE>& bone) -> Bool
		{
			++boneIndex;
			return !strcmp(boneName, bone->name.c_str());
		});
	if (iter == m_Bones.end())
		return -1;
	return boneIndex;
}
Bool Tool::Converter::WriteJsonFile(const wstring& path)
{
	using json = nlohmann::json;
	json root{
		{ "inspectionSchemaVersion", 1 },
		{ "materials", json::array() },
		{ "animations", json::array() },
		{ "bones", json::array() },
		{ "meshes", json::array() }
	};

	// 1. Materials
	std::set<string> converterMaterialNames;
	std::set<string> duplicateConverterMaterialNames;
	for (size_t materialIndex = 0; materialIndex < m_Material.size(); ++materialIndex)
	{
		const auto& mat = m_Material[materialIndex];
		json matJson;
		matJson["materialIndex"] = materialIndex;
		matJson["name"] = mat->name;
		if (!converterMaterialNames.insert(mat->name).second)
			duplicateConverterMaterialNames.insert(mat->name);
		
		json textures = json::array();
		for (auto& tex : mat->textures)
		{
			json texEntry;
			texEntry["type"] = tex.typeIndex;
			texEntry["path"] = tex.path;
			textures.push_back(texEntry);
		}
		matJson["textures"] = textures;
		root["materials"].push_back(matJson);
	}

	// 2. Animations
	for (size_t i = 0; i < m_Animation.size(); ++i)
	{
		auto& anim = m_Animation[i];
		json animJson;
		animJson["name"] = anim->name;
		animJson["duration"] = anim->duration;
		animJson["tickPerSecond"] = anim->tickPerSecond;
		
		// Root Motion
		animJson["rootMove"] = { anim->rootTotalTranslation.x, anim->rootTotalTranslation.y, anim->rootTotalTranslation.z };
		animJson["rootRot"] = { anim->rootTotalRotation.x, anim->rootTotalRotation.y, anim->rootTotalRotation.z, anim->rootTotalRotation.w };

		// Channels
		json channelsJson = json::array();
		for (auto& channel : m_Channels[i])
		{
			json channelJson;
			channelJson["name"] = channel->name;
			channelJson["boneIndex"] = channel->boneIndex;
			channelJson["numKeyFrames"] = channel->numKeyFrames;
			channelsJson.push_back(channelJson);
		}
		animJson["channels"] = channelsJson;

		root["animations"].push_back(animJson);
	}

	// 3. Bones
	for (auto& bone : m_Bones)
	{
		json boneJson;
		boneJson["name"] = bone->name;
		boneJson["parentIndex"] = bone->parentIndex;
		root["bones"].push_back(boneJson);
	}

	// 4. Meshes
	for (size_t meshIndex = 0; meshIndex < m_Meshes.size(); ++meshIndex)
	{
		const auto& mesh = m_Meshes[meshIndex];
		json meshJson;
		meshJson["name"] = mesh->name;
		if (meshIndex < m_MeshRoles.size())
		{
			const auto& classification = m_MeshRoles[meshIndex];
			meshJson["role"] = Nier_Fbx_Rule::Get_RoleName(classification.role);
			meshJson["lodLevel"] = classification.lodLevel;
			meshJson["roleBaseName"] = classification.baseName;
			meshJson["roleMatchedToken"] = classification.matchedToken;
		}
		meshJson["materialIndex"] = mesh->materialIndex;
		const Bool validMaterialIndex = mesh->materialIndex >= 0 &&
			static_cast<size_t>(mesh->materialIndex) < m_Material.size();
		meshJson["materialName"] = validMaterialIndex
			? json(m_Material[mesh->materialIndex]->name)
			: json(nullptr);
		meshJson["numVertices"] = m_IsSkeletal ? mesh->animVertices.size() : mesh->vertices.size();
		meshJson["numIndices"] = mesh->indices.size();
		root["meshes"].push_back(meshJson);
	}

	// 5. Converter/reference material inspection. This is authoring evidence only;
	// runtime .model loading never reads this JSON or the optional materials.json.
	json inspection;
	inspection["converterMaterialCount"] = m_Material.size();
	inspection["meshCount"] = m_Meshes.size();

	json roleCounts;
	for (uint32 role = 0; role < static_cast<uint32>(NIER_FBX_ROLE::Count); ++role)
	{
		roleCounts[Nier_Fbx_Rule::Get_RoleName(static_cast<NIER_FBX_ROLE>(role))] =
			m_RoleBuckets[role].size();
	}
	inspection["roleMeshCounts"] = roleCounts;

	inspection["duplicateConverterMaterialNames"] = duplicateConverterMaterialNames;

	json invalidMeshes = json::array();
	size_t validMeshMaterialIndexCount = 0;
	for (const auto& mesh : m_Meshes)
	{
		const Bool valid = mesh && mesh->materialIndex >= 0 &&
			static_cast<size_t>(mesh->materialIndex) < m_Material.size();
		if (valid)
		{
			++validMeshMaterialIndexCount;
			continue;
		}

		invalidMeshes.push_back({
			{ "meshName", mesh ? mesh->name : "" },
			{ "materialIndex", mesh ? mesh->materialIndex : -1 }
		});
	}
	inspection["validMeshMaterialIndexCount"] = validMeshMaterialIndexCount;
	inspection["invalidMeshMaterialIndices"] = invalidMeshes;

	const filesystem::path referencePath = filesystem::path(path).parent_path() / L"materials.json";
	json referenceAudit{
		{ "path", "materials.json" },
		{ "present", filesystem::is_regular_file(referencePath) },
		{ "status", "not-found" },
		{ "materialCount", 0 },
		{ "exactMatchCount", 0 },
		{ "exactMatches", json::array() },
		{ "converterOnlyMaterials", json::array() },
		{ "sourceOnlyMaterials", json::array() },
		{ "duplicateSourceMaterialNames", json::array() }
	};

	if (referenceAudit["present"].get<Bool>())
	{
		json referenceMaterials;
		std::set<string> sourceMaterialNames;
		std::set<string> duplicateSourceMaterialNames;
		try
		{
			ifstream referenceFile(referencePath);
			if (!referenceFile.is_open())
				throw runtime_error("unable to open materials.json");

			referenceMaterials = json::parse(referenceFile,
				[&](int depth, json::parse_event_t event, json& parsed)
				{
					if (event == json::parse_event_t::key && depth == 1 && parsed.is_string())
					{
						const string name = parsed.get<string>();
						if (!sourceMaterialNames.insert(name).second)
							duplicateSourceMaterialNames.insert(name);
					}
					return true;
				});

			if (!referenceMaterials.is_object())
				throw runtime_error("materials.json root must be an object");

			referenceAudit["status"] = duplicateSourceMaterialNames.empty()
				? "ok" : "duplicate-names";
			referenceAudit["materialCount"] = sourceMaterialNames.size();
			referenceAudit["duplicateSourceMaterialNames"] = duplicateSourceMaterialNames;

			for (size_t materialIndex = 0; materialIndex < m_Material.size(); ++materialIndex)
			{
				const string& materialName = m_Material[materialIndex]->name;
				const auto sourceIt = referenceMaterials.find(materialName);
				if (sourceIt == referenceMaterials.end())
				{
					referenceAudit["converterOnlyMaterials"].push_back(materialName);
					continue;
				}

				json match{
					{ "materialIndex", materialIndex },
					{ "materialName", materialName }
				};
				if (sourceIt->is_object())
				{
					const auto shaderIt = sourceIt->find("Shader_Name");
					if (shaderIt != sourceIt->end() && shaderIt->is_string())
						match["sourceShaderName"] = shaderIt->get<string>();
					const auto techniqueIt = sourceIt->find("Technique_Name");
					if (techniqueIt != sourceIt->end() && techniqueIt->is_string())
						match["sourceTechniqueName"] = techniqueIt->get<string>();
				}
				referenceAudit["exactMatches"].push_back(std::move(match));
			}

			for (const string& sourceName : sourceMaterialNames)
			{
				if (!converterMaterialNames.contains(sourceName))
					referenceAudit["sourceOnlyMaterials"].push_back(sourceName);
			}
			referenceAudit["exactMatchCount"] = referenceAudit["exactMatches"].size();
		}
		catch (const std::exception& exception)
		{
			referenceAudit["status"] = "invalid";
			referenceAudit["error"] = exception.what();
		}
	}
	inspection["referenceMaterials"] = std::move(referenceAudit);
	root["inspection"] = std::move(inspection);

	ofstream out(path);
	if (!out.is_open())
	{
		std::cerr << "  [JSON] Failed to open inspection output\n";
		return false;
	}

	out << root.dump(4);
	if (!out.good())
	{
		std::cerr << "  [JSON] Failed to write inspection output\n";
		return false;
	}
	out.close();

	const json& writtenInspection = root["inspection"];
	const json& writtenReference = writtenInspection["referenceMaterials"];
	std::cout << "  [Audit] Mesh material indices: "
		<< writtenInspection["validMeshMaterialIndexCount"] << "/"
		<< writtenInspection["meshCount"] << " valid\n";
	if (writtenReference["present"].get<Bool>())
	{
		std::cout << "  [Audit] Reference material exact matches: "
			<< writtenReference["exactMatchCount"] << "/"
			<< writtenInspection["converterMaterialCount"]
			<< " (status=" << writtenReference["status"].get<string>() << ")\n";
	}

	const std::u8string utf8Path = filesystem::path(path).u8string();
	std::cout << "  [JSON] Export Success: "
		<< string{ utf8Path.begin(), utf8Path.end() } << "\n";
	return true;
}
