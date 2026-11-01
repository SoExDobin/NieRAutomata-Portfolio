#include "pch.h"
#include "Nier_Fbx_Rule.h"

#include <cctype>
#include <nlohmann/json.hpp>

using namespace Tool;

namespace
{
	/* Tokens are compared case-insensitively after splitting the mesh name on '_', '-', '.', ' '.
	   Keep every naming rule in these tables; do not scatter name checks into Converter. */

	/* Collision. `HexGround` is the group name the NieR COL batches carry in the shipped
	   Entry export; it maps 1:1 onto the 401 batches of the raw g11021.col. */
	const vector<string> COL_TOKENS{ "COL", "COLLISION", "HEXGROUND" };

	/* Placement records. The shipped exports carry none of these yet (LAY data was dropped
	   on export), so this bucket stays empty until an export includes placements. */
	const vector<string> INSTANCE_TOKENS{ "INST", "INSTANCE", "LAY", "LAYASSETS", "LAYINSTANCES" };

	/* Shadow-only pass. In the source `.blend` these carry `mesh_group_props.lod_name = "SHADOW"`
	   with `lod_level = -1`, so they are a separate pass and must not fall into the Lod tiers.
	   Bridge, MainField and ManHole each ship 34-70 of them; Entry ships none. */
	const vector<string> SHADOW_TOKENS{ "SHADOW" };

	/* Materials that only shadow geometry uses (NieR shader CNS00). G11221 ships 85 meshes with
	   `Shadow_Mat3`; 77 are named SHADOW_*, the other 8 (`gareki.057`~`.064`) are not. */
	const vector<string> SHADOW_MATERIALS{ "SHADOW_MAT3" };

	/* Substrings checked when no whole token matched, for names that glue the marker on
	   without a delimiter (`lay_layInstances`, `g11021_HexGround001`). */
	const vector<string> COL_SUBSTRINGS{ "HEXGROUND" };
	const vector<string> INSTANCE_SUBSTRINGS{ "LAYASSETS", "LAYINSTANCES" };

	/* `materials.json` texture slots per aiTextureType, in priority order. Layered materials
	   (`mas_concretefloor03` and similar) carry only the numbered layer slots, so layer 1
	   stands in until the shader blends layers itself. Mask/Light/Env slots use NieR-specific
	   channel layouts and are left to the material shader work. */
	struct SOURCE_TEXTURE_SLOT
	{
		uint32				typeIndex;
		vector<string>		slotNames;
	};
	const vector<SOURCE_TEXTURE_SLOT> SOURCE_TEXTURE_SLOTS{
		{ aiTextureType_DIFFUSE, { "g_AlbedoMap", "g_AlbedoMap1" } },
		{ aiTextureType_NORMALS, { "g_NormalMap", "g_NormalMap1" } },
		/* NieR packed mask (R metal, G gloss, B cavity). No aiTextureType matches it, so it
		   rides in METALNESS; the runtime Material reads that slot back as g_MaskMap. */
		{ aiTextureType_METALNESS, { "g_MaskMap" } },
	};

	constexpr Char LOD_PREFIX[] = "LOD";
	constexpr size_t LOD_PREFIX_LENGTH = 3;

	/* Trailing `.001` style suffixes are Blender name-collision suffixes, not LOD levels
	   and not instance ids. They must never decide a role. */
	Bool Is_BlenderDuplicateSuffix(const string& token)
	{
		if (token.size() != 3)
			return false;

		for (const Char ch : token)
		{
			if (0 == std::isdigit(static_cast<unsigned char>(ch)))
				return false;
		}
		return true;
	}

	/* Returns the LOD level for a `LOD`, `LOD0`, `LOD1`... token, or -1 when the token is not LOD. */
	int32 Parse_LodToken(const string& upperToken)
	{
		if (upperToken.size() < LOD_PREFIX_LENGTH)
			return -1;

		if (0 != upperToken.compare(0, LOD_PREFIX_LENGTH, LOD_PREFIX))
			return -1;

		const string digits = upperToken.substr(LOD_PREFIX_LENGTH);
		if (digits.empty())
			return 1; /* bare `LOD` means "this is the reduced mesh" */

		for (const Char ch : digits)
		{
			if (0 == std::isdigit(static_cast<unsigned char>(ch)))
				return -1;
		}
		return std::stoi(digits);
	}
}

Nier_Fbx_Rule::Classification Nier_Fbx_Rule::Classify(const string& meshName, const string& materialName)
{
	Classification result;
	result.baseName = meshName;

	const vector<string> tokens = Tokenize(meshName);
	const string upperName = ToUpper(meshName);

	/* 1. Collision */
	for (const string& token : tokens)
	{
		if (COL_TOKENS.end() == std::find(COL_TOKENS.begin(), COL_TOKENS.end(), ToUpper(token)))
			continue;

		result.role = NIER_FBX_ROLE::Col;
		result.matchedToken = token;
		result.baseName = Strip_Tokens(meshName, COL_TOKENS);
		return result;
	}
	for (const string& fragment : COL_SUBSTRINGS)
	{
		if (!Contains_Fold(upperName, fragment))
			continue;

		result.role = NIER_FBX_ROLE::Col;
		result.matchedToken = fragment;
		return result;
	}

	/* 2. Placement / instance */
	for (const string& token : tokens)
	{
		if (INSTANCE_TOKENS.end() == std::find(INSTANCE_TOKENS.begin(), INSTANCE_TOKENS.end(), ToUpper(token)))
			continue;

		result.role = NIER_FBX_ROLE::Instance;
		result.matchedToken = token;
		result.baseName = Strip_Tokens(meshName, INSTANCE_TOKENS);
		return result;
	}
	for (const string& fragment : INSTANCE_SUBSTRINGS)
	{
		if (!Contains_Fold(upperName, fragment))
			continue;

		result.role = NIER_FBX_ROLE::Instance;
		result.matchedToken = fragment;
		return result;
	}

	/* 3. Shadow pass. Checked before LOD because a shadow mesh is never an LOD tier. */
	for (const string& token : tokens)
	{
		if (SHADOW_TOKENS.end() == std::find(SHADOW_TOKENS.begin(), SHADOW_TOKENS.end(), ToUpper(token)))
			continue;

		result.role = NIER_FBX_ROLE::Shadow;
		result.lodLevel = -1;
		result.matchedToken = token;
		result.baseName = Strip_Tokens(meshName, SHADOW_TOKENS);
		return result;
	}
	if (SHADOW_MATERIALS.end() != std::find(SHADOW_MATERIALS.begin(), SHADOW_MATERIALS.end(), ToUpper(materialName)))
	{
		result.role = NIER_FBX_ROLE::Shadow;
		result.lodLevel = -1;
		result.matchedToken = materialName;
		return result;
	}

	/* 4. LOD. An explicit LOD0 is still the base visual mesh, so only level >= 1
	   moves the mesh out of the Mesh bucket. */
	for (const string& token : tokens)
	{
		if (Is_BlenderDuplicateSuffix(token))
			continue;

		const int32 level = Parse_LodToken(ToUpper(token));
		if (level < 0)
			continue;

		result.lodLevel = level;
		result.matchedToken = token;
		result.role = (level >= 1) ? NIER_FBX_ROLE::Lod : NIER_FBX_ROLE::Mesh;
		result.baseName = Strip_Tokens(meshName, { ToUpper(token) });
		return result;
	}

	/* 5. Default: visual render geometry. A map whose meshes carry no marker at all
	   converts as one ordinary Mesh asset, which is what an LOD-only export should be. */
	return result;
}

const Char* Nier_Fbx_Rule::Get_RoleName(NIER_FBX_ROLE role)
{
	switch (role)
	{
	case NIER_FBX_ROLE::Mesh:		return "Mesh";
	case NIER_FBX_ROLE::Lod:		return "Lod";
	case NIER_FBX_ROLE::Shadow:		return "Shadow";
	case NIER_FBX_ROLE::Col:		return "Col";
	case NIER_FBX_ROLE::Instance:	return "Instance";
	default:						return "Unknown";
	}
}

const Char* Nier_Fbx_Rule::Get_RoleSuffix(NIER_FBX_ROLE role)
{
	switch (role)
	{
	case NIER_FBX_ROLE::Mesh:		return "";
	case NIER_FBX_ROLE::Lod:		return "_LOD";
	case NIER_FBX_ROLE::Shadow:		return "_SHADOW";
	case NIER_FBX_ROLE::Col:		return "_COL";
	case NIER_FBX_ROLE::Instance:	return "_INST";
	default:						return "_UNKNOWN";
	}
}

vector<MODEL_ENTRY> Nier_Fbx_Rule::Resolve_SourceTextures(const nlohmann::json& sourceMaterial)
{
	vector<MODEL_ENTRY> entries;
	if (!sourceMaterial.is_object())
		return entries;

	const auto texturesIt = sourceMaterial.find("Textures");
	if (texturesIt == sourceMaterial.end() || !texturesIt->is_object())
		return entries;

	for (const SOURCE_TEXTURE_SLOT& slot : SOURCE_TEXTURE_SLOTS)
	{
		for (const string& slotName : slot.slotNames)
		{
			const auto hashIt = texturesIt->find(slotName);
			if (hashIt == texturesIt->end() || !hashIt->is_string() || hashIt->get<string>().empty())
				continue;

			/* Hashes are lowercase in the dump; the extracted files are named in uppercase. */
			entries.push_back({ slot.typeIndex, "Textures/" + ToUpper(hashIt->get<string>()) + ".dds" });
			break;
		}
	}
	return entries;
}

vector<string> Nier_Fbx_Rule::Tokenize(const string& meshName)
{
	vector<string> tokens;
	string current;

	for (const Char ch : meshName)
	{
		if ('_' == ch || '-' == ch || '.' == ch || ' ' == ch)
		{
			if (!current.empty())
			{
				tokens.push_back(current);
				current.clear();
			}
			continue;
		}
		current.push_back(ch);
	}

	if (!current.empty())
		tokens.push_back(current);

	return tokens;
}

string Nier_Fbx_Rule::ToUpper(const string& value)
{
	string upper = value;
	std::transform(upper.begin(), upper.end(), upper.begin(),
		[](Char ch) { return static_cast<Char>(std::toupper(static_cast<unsigned char>(ch))); });
	return upper;
}

Bool Nier_Fbx_Rule::Contains_Fold(const string& haystack, const string& needle)
{
	return string::npos != ToUpper(haystack).find(ToUpper(needle));
}

string Nier_Fbx_Rule::Strip_Tokens(const string& meshName, const vector<string>& tokens)
{
	string stripped;

	for (const string& token : Tokenize(meshName))
	{
		if (tokens.end() != std::find(tokens.begin(), tokens.end(), ToUpper(token)))
			continue;

		if (!stripped.empty())
			stripped.push_back('_');
		stripped.append(token);
	}

	return stripped.empty() ? meshName : stripped;
}
