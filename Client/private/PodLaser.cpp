#include "pch.h"
#include "PodLaser.h"

#include <Game.h>
#include <OBBCollider.h>
#include <PartObject.h>
#include <Shader.h>
#include <Texture.h>

#include "LaserEffect.h"
#include "Monster.h"
#include "Pl0000.h"

namespace
{
	// Beam timeline (seconds): it widens at once, holds, narrows into a thin line and fades.
	constexpr Float GrowTime = 0.06f;
	constexpr Float HoldEnd = 0.5f;
	constexpr Float ThinEnd = 0.9f;
	constexpr Float BeamLifetime = 1.1f;
	// Monsters are hit only while the beam is first drawn: a single shot, not a sustained beam.
	constexpr Float HitWindow = 0.12f;
	constexpr Float BeamWidth = 0.7f;
	constexpr Float MaxRange = 50.f;
	constexpr Float Damage = 750.f;
	constexpr uint32 VertexCount = 4;	// one camera-facing strip
}

PodLaser::PodLaser(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: GameObject{ device, context } {}
PodLaser::PodLaser(const PodLaser& rhs) : GameObject{ rhs } {}

HRESULT PodLaser::Initialize_Prototype()
{
	m_LayerMask.Set_Layer(L"PlayerWeapon");
	m_TagMask.Set_Tag({ L"PlayerWeapon" });
	return GameObject::Initialize_Prototype();
}

HRESULT PodLaser::Initialize(void* arg)
{
	if (!arg || FAILED(GameObject::Initialize(arg))) return E_FAIL;
	m_Impact = static_cast<POD_LASER_DESC*>(arg)->impact;

	Shader::SHADER_DESC shaderDesc{ L"VTX_Mesh_Effect", VTXMESH::Elements, VTXMESH::numElements };
	m_Shader = Add_Component<Shader>(ETOI(LEVEL::STATIC), &shaderDesc);
	Texture::TEXTURE_DESC textureDesc{ Get_LevelIndex(), L"Effect_PodLaserBeam" };
	m_Texture = Add_Component<Texture>(Get_LevelIndex(), &textureDesc);
	OBBCollider::OBB_COLLIDER_DESC colliderDesc{};
	m_Collider = Add_Component<OBBCollider>(ETOI(LEVEL::STATIC), &colliderDesc);
	if (!m_Shader || !m_Texture || !m_Collider) return E_FAIL;
	m_Collider->Set_Active(false);

	D3D11_BUFFER_DESC bufferDesc{};
	bufferDesc.ByteWidth = sizeof(VTXMESH) * VertexCount;
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	return m_Device->CreateBuffer(&bufferDesc, nullptr, m_VertexBuffer.GetAddressOf());
}

void PodLaser::Fire(const Vector3& origin, const Vector3& direction)
{
	Vector3 axis = direction;
	axis.Normalize();
	Vector3 hit{};
	const Bool hitWorld = GAME_INSTANCE->Raycast_Physics(origin, axis, MaxRange, hit);
	m_Start = origin;
	m_End = hitWorld ? hit : origin + axis * MaxRange;
	m_Age = 0.f;
	m_HitMonsters.clear();
	m_Collider->Set_Extents(Vector3{ BeamWidth * 0.5f, BeamWidth * 0.5f, Vector3::Distance(m_Start, m_End) * 0.5f });
	m_Collider->Set_Active(true);
	GAME_INSTANCE->PlaySoundFX(L"wp3000_lazer_shot", SOUNDCHANNEL::CHANNEL_15, 0.4f);

	// Only world collision explodes at the end of the beam; monsters explode where they are hit.
	if (hitWorld)
		Play_Impact(m_End);
}

void PodLaser::Play_Impact(const Vector3& position)
{
	if (auto impact = m_Impact.lock())
		impact->Play(position);
	GAME_INSTANCE->PlaySoundFX(L"core_pl_AS_small_bom_exp", SOUNDCHANNEL::CHANNEL_16, 0.4f, &position);
}

void PodLaser::Update(Float timeDelta)
{
	if (m_Age < 0.f) return;
	m_Age += timeDelta;
	if (m_Age >= HitWindow && m_Collider->Is_Active())
		m_Collider->Set_Active(false);
	if (m_Age >= BeamLifetime)
		m_Age = -1.f;
}

void PodLaser::Late_Update(Float)
{
	if (m_Age < 0.f) return;

	Vector3 axis = m_End - m_Start;
	const Float length = axis.Length();
	if (length < 0.01f) return;
	axis /= length;
	if (m_Collider->Is_Active())
	{
		const Vector3 up = std::abs(axis.y) > 0.99f ? Vector3::Forward : Vector3::Up;
		m_Collider->Update(Matrix::CreateWorld((m_Start + m_End) * 0.5f, axis, up));
	}

	Float width = BeamWidth;
	if (m_Age < GrowTime)
		width *= m_Age / GrowTime;
	else if (m_Age > HoldEnd)
		width *= 1.f - 0.88f * std::min((m_Age - HoldEnd) / (ThinEnd - HoldEnd), 1.f);
	const Float opacity = m_Age < ThinEnd ? 1.f : 1.f - (m_Age - ThinEnd) / (BeamLifetime - ThinEnd);

	// A strip around the beam axis that turns to face the camera.
	const Vector3 camera = GAME_INSTANCE->Get_Transform(D3DTS::VIEW).Invert().Translation();
	Vector3 side = axis.Cross(camera - (m_Start + m_End) * 0.5f);
	if (side.LengthSquared() < 1e-6f) side = axis.Cross(Vector3::Up);
	side.Normalize();

	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (FAILED(m_Context->Map(m_VertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;
	auto* vertices = static_cast<VTXMESH*>(mapped.pData);
	const Float halfWidth = width * 0.5f;
	const Vector3 corners[VertexCount] = { m_Start + side * halfWidth, m_Start - side * halfWidth,
		m_End + side * halfWidth, m_End - side * halfWidth };
	for (uint32 i = 0; i < VertexCount; ++i)
	{
		vertices[i] = {};
		vertices[i].position = corners[i];
		vertices[i].normal = Vector3{ 1.7f, 1.5f, 1.25f };
		vertices[i].texcoord = Vector2{ static_cast<Float>(i / 2), static_cast<Float>(i % 2) };
		vertices[i].texcoord1 = Vector2{ opacity, i < 2 ? 0.f : length };
	}
	m_Context->Unmap(m_VertexBuffer.Get(), 0);
}

HRESULT PodLaser::Render()
{
	if (m_Age < 0.f) return S_OK;
	const Matrix identity = Matrix::Identity;
	if (FAILED(m_Shader->Bind_Matrix(WorldMatrix, &identity)) ||
		FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ViewMatrix, D3DTS::VIEW)) ||
		FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ProjMatrix, D3DTS::PROJ)) ||
		FAILED(m_Texture->Bind_ShaderResourceView(m_Shader, "g_EffectMap", 0)) ||
		FAILED(m_Texture->Bind_ShaderResourceView(m_Shader, "g_EffectGlowMap", 1)) ||
		FAILED(m_Texture->Bind_ShaderResourceView(m_Shader, "g_EffectFlowMap", 2)) ||
		FAILED(m_Shader->Bind_RawValue("g_EffectTime", &m_Age, sizeof(Float)))) return E_FAIL;
	ID3D11Buffer* buffer = m_VertexBuffer.Get();
	const uint32 stride = sizeof(VTXMESH), offset = 0;
	m_Context->IASetVertexBuffers(0, 1, &buffer, &stride, &offset);
	m_Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	if (FAILED(m_Shader->Begin("LaserBeam"))) return E_FAIL;
	m_Context->Draw(VertexCount, 0);
	return S_OK;
}

void PodLaser::Submit_RenderGroup()
{
	// After the map's transparent pass, like WeaponHalo; depth-tested against the opaque scene.
	if (m_Age >= 0.f) GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::WORLDUI, shared_from_this());
}

void PodLaser::OnCollisionStay(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
	if (!m_Collider->Is_Active()) return;
	const auto target = targetCollider->Get_Owner();
	if (!target || target->Get_GameObjectType() != GAMEOBJECTTYPE::PART) return;
	if (target->Get_LayerMask().Get_LayerName() != L"Monster") return;
	// A monster has several parts on this layer; it still takes the shot once.
	const auto monster = static_pointer_cast<Monster>(static_pointer_cast<PartObject>(target)->Get_Owner());
	if (!monster || !m_HitMonsters.insert(monster->Get_RuntimeObjectId()).second) return;

	Vector3 axis = m_End - m_Start;
	axis.Normalize();

	const auto attacker = static_pointer_cast<Pl0000>(Get_Parent());
	Entity::DAMAGE_INFO damageInfo{};
	damageInfo.attacker = attacker;
	damageInfo.attackType = ATK_TYPE::POD;
	damageInfo.damage = Damage;
	damageInfo.groggyWeight = 100;
	damageInfo.knockbackForce = 2.f;
	damageInfo.blowAway = true;
	// The surface point facing the Pod, where the beam goes in. The beam point nearest the collider's centre lies inside
	// a big body (Em3000's skirt), where the burst would be hidden.
	damageInfo.hitPosition = targetCollider->ClosestPoint(m_Start);
	// Sparks bounce back along the beam.
	damageInfo.hitRotation = Quaternion::FromToRotation(Vector3::UnitY, -axis);
	monster->TakeDamage(damageInfo);
	monster->OnAttackHit(shared_from_this());
	// The global hit lag once per shot; every pierced monster would otherwise stretch it.
	if (attacker && m_HitMonsters.size() == 1)
		attacker->Trigger_GlobalLag(0.2f, 0.05f);
	Play_Impact(damageInfo.hitPosition);
}

Shared<PodLaser> PodLaser::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<PodLaser>(device, context);
	return SUCCEEDED(prototype->Initialize_Prototype()) ? prototype : nullptr;
}

Shared<GameObject> PodLaser::Clone(void* arg)
{
	auto instance = make_shared<PodLaser>(*this);
	return SUCCEEDED(instance->Initialize(arg)) ? instance : nullptr;
}
