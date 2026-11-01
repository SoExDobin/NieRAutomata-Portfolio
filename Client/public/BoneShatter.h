#pragma once

NS_BEGIN(Engine)
class Model;
class Shader;
NS_END

NS_BEGIN(Client)

// Em3000's death (the body and em3001's chains): the pose freezes and the model breaks into its bones' parts. Every
// vertex goes with the bone that weighs most on it, so a mesh splits into as many pieces as it has such bones, and one
// bone under two meshes makes two pieces. They fly out from the blast, land on the floor 1 m below its centre, scrape to
// a stop and dissolve (Shader_VtxAnimMesh pass Shatter, as the small machines' Em0010Body). The owner stops posing its model,
// calls Bind with its other shader values, and draws mesh i through Begin_Mesh after the mesh's material. Shared by
// Em3000Body and Em3000Parts.
class CLIENT_DLL BoneShatter final
{
public:
	// center: the blast, 1 m above the floor the pieces land on.
	void Begin(const Shared<Model>& model, const Vector3& center);
	void Update(Float timeDelta) { if (Is_Running()) m_Time += timeDelta; }
	Bool Is_Running() const { return m_Time >= 0.f; }
	Bool Is_Gone() const { return m_Time >= Duration; }
	HRESULT Bind(const Shared<Shader>& shader) const;
	// Re-begins the bound material with the Shatter pass. S_FALSE: the mesh has no parts and is skipped.
	HRESULT Begin_Mesh(const Shared<Shader>& shader, uint32 meshIndex) const;

private:
	Float m_Time{ -1.f };
	Vector3 m_Center{};
	vector<vector<Vector4>> m_PartCenters;	// per mesh, Model::Get_MeshPartCenters (Shader g_PartCenters[256])

	// g_ShatterDuration: gone 3 s after the blast (on request). A head 7 m up takes about 1.5 s to land and scrape to a
	// stop; the pieces dissolve from 0.6 of it (1.8 s).
	static constexpr Float Duration = 3.f;
};

NS_END
