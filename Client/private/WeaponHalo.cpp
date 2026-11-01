#include "pch.h"
#include "WeaponHalo.h"

#include "Pl0000Parts.h"
#include "Random_Helper.h"

#include <Game.h>
#include <Model.h>
#include <Shader.h>
#include <Texture.h>

namespace
{
	// core.eff/093, one pass per mesh of Halo_core093.model. The rings turn at FVWK base speed to base + range
	// (taken as rad/s, picked per halo).
	struct HALO_LAYER
	{
		const Char* pass;
		Float minSpeed;
		Float maxSpeed;
	};
	constexpr HALO_LAYER Layers[3] = {
		{ "Circuit", 1.f, 3.f },
		{ "Lettering", 1.f, 2.f },
		{ "Streak", 1.f, 1.f },
	};
	constexpr Float TransitionDuration = 0.82f;
	constexpr Float BladeFlashDuration = 0.38f;
	constexpr uint32 TransitionGlintCount = 64;
}

WeaponHalo::WeaponHalo(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: GameObject{ device, context } {}
WeaponHalo::WeaponHalo(const WeaponHalo& rhs) : GameObject{ rhs } {}

HRESULT WeaponHalo::Initialize_Prototype()
{
	m_LayerMask.Set_Layer(L"Default");
	return GameObject::Initialize_Prototype();
}

HRESULT WeaponHalo::Initialize(void* arg)
{
	if (!arg || FAILED(GameObject::Initialize(arg))) return E_FAIL;
	const auto& desc = *static_cast<WEAPON_HALO_DESC*>(arg);
	m_Anchor = desc.anchor;
	m_HandAnchor = desc.handAnchor;
	m_LocalMatrix = desc.localMatrix;
	m_BladeStart = desc.bladeStart;
	m_BladeEnd = desc.bladeEnd;
	auto anchor = m_Anchor.lock();
	auto hand = m_HandAnchor.lock();
	if (!anchor || !hand) return E_FAIL;
	const int32 boneIndex = anchor->Get_ModelComponent()->Get_BoneIndexByName("bone0");
	if (boneIndex < 0) return E_FAIL;
	m_BoneIndex = static_cast<uint32>(boneIndex);
	const int32 handBoneIndex = hand->Get_ModelComponent()->Get_BoneIndexByName("bone0");
	if (handBoneIndex < 0) return E_FAIL;
	m_HandBoneIndex = static_cast<uint32>(handBoneIndex);
	Shader::SHADER_DESC shaderDesc{ L"VTX_Mesh_Halo", VTXMESH::Elements, VTXMESH::numElements };
	m_Shader = Add_Component<Shader>(ETOI(LEVEL::STATIC), &shaderDesc);
	Model::MODEL_DESC modelDesc{ L"halo" };
	m_Model = Add_Component<Model>(Get_LevelIndex(), &modelDesc);
	// One Texture per object (components are keyed by type): halo_%d.dds holds the four coreeff images.
	Texture::TEXTURE_DESC textureDesc{ Get_LevelIndex(), L"Effect_WeaponHalo" };
	m_Texture = Add_Component<Texture>(Get_LevelIndex(), &textureDesc);
	if (!m_Shader || !m_Model || !m_Texture || m_Model->Get_NumMeshes() != size(Layers)) return E_FAIL;

	for (uint32 i = 0; i < size(Layers); ++i)
	{
		m_Spin[i] = static_cast<Float>(Helper::Random_Double(0.0, XM_2PI));
		m_SpinSpeed[i] = static_cast<Float>(Helper::Random_Double(Layers[i].minSpeed, Layers[i].maxSpeed));
	}

	// A unit quad; the shader turns it to the camera at the halo centre.
	VTXMESH quad[4]{};
	for (uint32 i = 0; i < 4; ++i)
	{
		quad[i].position = Vector3{ (i / 2) ? 0.5f : -0.5f, (i % 2) ? -0.5f : 0.5f, 0.f };
		quad[i].texcoord = Vector2{ static_cast<Float>(i / 2), static_cast<Float>(i % 2) };
	}
	D3D11_BUFFER_DESC bufferDesc{};
	bufferDesc.ByteWidth = sizeof(quad);
	bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA initialData{ quad };
	return m_Device->CreateBuffer(&bufferDesc, &initialData, m_GlowQuad.GetAddressOf());
}

void WeaponHalo::Update(Float timeDelta)
{
	if (m_Anchor.expired() || m_HandAnchor.expired())
	{
		GAME_INSTANCE->Destroy(Get_ObjectGuid());
		return;
	}
	for (uint32 i = 0; i < size(Layers); ++i)
		m_Spin[i] = std::fmod(m_Spin[i] + timeDelta * m_SpinSpeed[i], XM_2PI);
	if (m_TransitionTime < TransitionDuration)
	{
		m_TransitionTime = std::min(TransitionDuration, m_TransitionTime + timeDelta);
		const Float revealTime = m_ToSheath ? 0.08f : 0.22f;
		if (!m_Revealed && m_TransitionTime >= revealTime)
		{
			if (auto target = (m_ToSheath ? m_Anchor : m_HandAnchor).lock()) target->Set_Visible(true);
			m_Revealed = true;
		}
	}
}

void WeaponHalo::Begin_Transition(Bool toSheath)
{
	// Keep gameplay activation and collision timing in Pl0000. Only the destination mesh waits for the flash.
	if (auto sheath = m_Anchor.lock()) sheath->Set_Visible(true);
	if (auto hand = m_HandAnchor.lock()) hand->Set_Visible(true);
	m_ToSheath = toSheath;
	m_TransitionTime = 0.f;
	m_Revealed = false;
	if (auto target = (toSheath ? m_Anchor : m_HandAnchor).lock()) target->Set_Visible(false);
}

void WeaponHalo::Late_Update(Float)
{
	if (auto anchor = m_Anchor.lock())
	{
		// The rings of Halo_core093.model lie around Z, the blade axis; each layer turns about it in the shader.
		const Matrix blade = anchor->Get_ModelComponent()->Get_BoneMatrix(m_BoneIndex) * anchor->Get_CombinedWorldMatrix();
		m_WorldMatrix = m_LocalMatrix * blade;
	}
	if (auto target = (m_ToSheath ? m_Anchor : m_HandAnchor).lock())
	{
		const uint32 bone = m_ToSheath ? m_BoneIndex : m_HandBoneIndex;
		m_TransitionMatrix = target->Get_ModelComponent()->Get_BoneMatrix(bone) * target->Get_CombinedWorldMatrix();
	}
}

HRESULT WeaponHalo::Render()
{
	// The Halo belongs only in the late forward pass, after transparent map meshes.
	if (GAME_INSTANCE->Get_DrawingGroup() != RENDERGROUP::WORLDUI) return S_OK;
	auto anchor = m_Anchor.lock();
	if (!anchor) return S_OK;
	if (FAILED(m_Shader->Bind_Matrix(WorldMatrix, &m_WorldMatrix)) ||
		FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ViewMatrix, D3DTS::VIEW)) ||
		FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ProjMatrix, D3DTS::PROJ)) ||
		FAILED(m_Texture->Bind_ShaderResourceView(m_Shader, "g_HaloLetteringMap", 0)) ||
		FAILED(m_Texture->Bind_ShaderResourceView(m_Shader, "g_HaloCircuitMap", 1)) ||
		FAILED(m_Texture->Bind_ShaderResourceView(m_Shader, "g_HaloStreakMap", 2)) ||
		FAILED(m_Texture->Bind_ShaderResourceView(m_Shader, "g_HaloGlowMap", 3))) return E_FAIL;
	if (anchor->Is_Active())
		for (uint32 i = 0; i < size(Layers); ++i)
			if (FAILED(m_Shader->Bind_RawValue("g_HaloSpin", &m_Spin[i], sizeof(Float))) ||
				FAILED(m_Shader->Begin(Layers[i].pass)) || FAILED(m_Model->Render(i))) return E_FAIL;

	ID3D11Buffer* buffer = m_GlowQuad.Get();
	const uint32 stride = sizeof(VTXMESH), offset = 0;
	m_Context->IASetVertexBuffers(0, 1, &buffer, &stride, &offset);
	m_Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	if (anchor->Is_Active())
	{
		if (FAILED(m_Shader->Begin("Glow"))) return E_FAIL;
		m_Context->Draw(4, 0);
	}
	if (m_TransitionTime >= TransitionDuration) return S_OK;
	const Float progress = m_TransitionTime / TransitionDuration;
	const Float reveal = m_ToSheath ? 0.08f : 0.22f;
	const Float flash = std::max(0.f, 1.f - std::abs(m_TransitionTime - reveal) / 0.09f);
	if (m_TransitionTime < BladeFlashDuration)
	{
		const Float bladeProgress = m_TransitionTime / BladeFlashDuration;
		const Float bladeIntensity = (1.f - bladeProgress) * 1.4f + flash * 3.5f;
		const Float width = m_ToSheath ? 0.17f : 0.21f;
		if (FAILED(m_Shader->Bind_Matrix(WorldMatrix, &m_TransitionMatrix)) ||
			FAILED(m_Shader->Bind_RawValue("g_BladeStart", &m_BladeStart, sizeof(Float))) ||
			FAILED(m_Shader->Bind_RawValue("g_BladeEnd", &m_BladeEnd, sizeof(Float))) ||
			FAILED(m_Shader->Bind_RawValue("g_TransitionWidth", &width, sizeof(Float))) ||
			FAILED(m_Shader->Bind_RawValue("g_TransitionIntensity", &bladeIntensity, sizeof(Float))) ||
			FAILED(m_Shader->Begin("TransitionBlade"))) return E_FAIL;
		m_Context->Draw(4, 0);
	}
	// Small glints keep travelling after the bright blade flash has cleared.
	const Float glintIntensity = 1.15f * std::pow(1.f - progress, 0.7f) + flash * 2.f;
	if (FAILED(m_Shader->Bind_RawValue("g_TransitionIntensity", &glintIntensity, sizeof(Float)))) return E_FAIL;
	for (uint32 i = 0; i < TransitionGlintCount; ++i)
	{
		const Float seed = static_cast<Float>(i);
		const Float along = std::fmod(seed * 0.618034f, 1.f);
		const Float angle = seed * 2.399963f;
		const Float radius = (m_ToSheath ? progress : 1.f - progress) * (0.35f + 0.27f * std::fmod(seed * 0.37f, 1.f));
		const Vector3 offset{ std::cos(angle) * radius, std::sin(angle) * radius,
			m_BladeStart + (m_BladeEnd - m_BladeStart) * along + std::sin(angle * 1.7f) * radius * 0.4f };
		const Float scale = (0.055f + 0.06f * std::fmod(seed * 0.73f, 1.f)) * (1.f - progress * 0.5f);
		const Matrix particle = Matrix::CreateScale(scale) * Matrix::CreateTranslation(offset) * m_TransitionMatrix;
		if (FAILED(m_Shader->Bind_Matrix(WorldMatrix, &particle)) ||
			FAILED(m_Shader->Begin("TransitionGlint"))) return E_FAIL;
		m_Context->Draw(4, 0);
	}
	return S_OK;
}

void WeaponHalo::Submit_RenderGroup()
{
	// Draw after the map's transparent BLEND pass. Its alpha cannot cover the Halo,
	// while the Halo shader still tests depth against opaque scene geometry.
	GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::WORLDUI, shared_from_this());
}

Shared<WeaponHalo> WeaponHalo::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<WeaponHalo>(device, context);
	return SUCCEEDED(prototype->Initialize_Prototype()) ? prototype : nullptr;
}

Shared<GameObject> WeaponHalo::Clone(void* arg)
{
	auto instance = make_shared<WeaponHalo>(*this);
	return SUCCEEDED(instance->Initialize(arg)) ? instance : nullptr;
}
