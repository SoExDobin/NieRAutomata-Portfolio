#pragma once
#include <string>
#include <vector>
#include <nlohmann/json_fwd.hpp>

NS_BEGIN(Tool)

/* Role a single FBX mesh plays inside a NieR world export.
   The shipped world FBX files flatten every role into one flat mesh list with no
   hierarchy and no Blender custom properties, so the mesh name is the only signal. */
enum class NIER_FBX_ROLE : uint32
{
	Mesh,		/* visual render geometry (also LOD0) */
	Lod,		/* reduced visual geometry, level >= 1 */
	Shadow,		/* shadow-only pass geometry; lod_level -1 in the source .blend, not an LOD tier */
	Col,		/* collision geometry */
	Instance,	/* placement / instance record, not render geometry */
	Count
};

class Nier_Fbx_Rule final
{
public:
	struct Classification
	{
		NIER_FBX_ROLE	role{ NIER_FBX_ROLE::Mesh };
		int32			lodLevel{ -1 };	/* 0 for an explicit LOD0, >=1 for Lod role, -1 when the name says nothing */
		string			baseName;		/* mesh name with every matched role token removed */
		string			matchedToken;	/* the token that decided the role, for logging */
	};

public:
	/* Evaluation order is fixed: Col -> Instance -> Shadow -> Lod -> Mesh.
	   The first table that matches wins, so a name carrying two role tokens is
	   resolved deterministically instead of by table iteration order.
	   The material name only decides Shadow: a mesh drawn with the shadow material is shadow geometry
	   even when its name lost the SHADOW token. */
	static Classification	Classify(const string& meshName, const string& materialName = {});

	static const Char*		Get_RoleName(NIER_FBX_ROLE role);
	/* Output filename suffix for the role: Mesh writes the bare stem, the rest are suffixed. */
	static const Char*		Get_RoleSuffix(NIER_FBX_ROLE role);

	/* Texture entries for one `materials.json` material (the WMB material dump shipped next to the FBX).
	   Nier2Blender routes albedo and normal through Mix/Group nodes that Blender's FBX exporter cannot
	   follow, so the FBX keeps almost no texture bindings; the dump keeps them as texture hashes.
	   Returns aiTextureType-indexed entries pointing at `Textures/<HASH>.dds`. */
	static vector<MODEL_ENTRY>	Resolve_SourceTextures(const nlohmann::json& sourceMaterial);

private:
	static vector<string>	Tokenize(const string& meshName);
	static string			ToUpper(const string& value);
	static Bool				Contains_Fold(const string& haystack, const string& needle);
	static string			Strip_Tokens(const string& meshName, const vector<string>& tokens);
};

NS_END
