#include "pch.h"
#include "AreaTrigger.h"
#include "ClientApp.h"
#include "DirectionalLight.h"
#include "Firework.h"
#include "Game.h"
#include "LevelGamePlay.h"
#include "OBBCollider.h"
#include "SoundEmitter.h"
#include "Transform.h"
#include "UIManager.h"
#include "VIBuffer_Particle_Point.h"
#include <Shader.h>
#include <SpdLogger.h>
#include <Texture.h>

AreaTrigger::AreaTrigger() : GameObject{} {}
AreaTrigger::AreaTrigger(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: GameObject{ device, context } {}
AreaTrigger::AreaTrigger(const AreaTrigger& rhs)
	: GameObject{ rhs }, m_LightOverride{ rhs.m_LightOverride }, m_LightColor{ rhs.m_LightColor },
	m_LightAmbient{ rhs.m_LightAmbient }, m_LightExposure{ rhs.m_LightExposure }, m_LightFogOpacity{ rhs.m_LightFogOpacity },
	m_NextLevel{ rhs.m_NextLevel }, m_Communication{ rhs.m_Communication }, m_CameraEvent{ rhs.m_CameraEvent } {}

HRESULT AreaTrigger::Initialize_Prototype()
{
	m_LayerMask.Set_Layer(L"Default");
	/* Only the player's movement collider counts (Is_Player). With the global Default mask a room-sized box touched every
	   bullet inside, filled the overlap buffer (64) and could drop the player from it, turning the area light off. */
	m_LayerMask.Set_UseCustomMask(true);
	m_LayerMask.Set_Mask({ L"PlayerPhysical" });
	return GameObject::Initialize_Prototype();
}

HRESULT AreaTrigger::Initialize(void* arg)
{
	if (FAILED(GameObject::Initialize(arg)))
	{
		LOG_ERROR(L"Failed to Initialize AreaTrigger {}", m_ObjectName);
		return E_FAIL;
	}

	/* A new trigger starts as a 2 m cube; the scene saves the authored Offset/Extents on the collider. */
	OBBCollider::OBB_COLLIDER_DESC boxDesc{};
	boxDesc.extents = Vector3{ 1.f, 1.f, 1.f };
	m_Box = Add_Component<OBBCollider>(ETOI(LEVEL::STATIC), &boxDesc);
	if (nullptr == m_Box)
	{
		LOG_ERROR(L"Failed to attach OBBCollider to AreaTrigger {}", m_ObjectName);
		return E_FAIL;
	}
	/* An ambience spot: the scene stores its SoundTag (empty = silent). */
	if (nullptr == Add_Component<SoundEmitter>(ETOI(LEVEL::STATIC)))
	{
		LOG_ERROR(L"Failed to attach SoundEmitter to AreaTrigger {}", m_ObjectName);
		return E_FAIL;
	}
	/* The floor mark of a NextLevel trigger. The scene sets NextLevel after this, so every trigger readies it: one point,
	   sized, laid down and placed by the world matrix. */
	Shader::SHADER_DESC shaderDesc{ VTXPARTICLE_POINT_DESC::Tag, VTXPARTICLE_POINT_DESC::Elements, VTXPARTICLE_POINT_DESC::numElements };
	m_Shader = Add_Component<Shader>(ETOI(LEVEL::STATIC), &shaderDesc);
	VIBuffer_Particle_Point::VIBUFFER_INSTANCE_POINT_DESC instanceDesc{};
	instanceDesc.numInstances = 1;
	instanceDesc.scale = Vector2{ 1.f, 1.f };
	instanceDesc.lifeTime = Vector2{ 1.f, 1.f };
	m_PortalBuffer = Add_Component<VIBuffer_Particle_Point>(ETOI(LEVEL::STATIC), &instanceDesc);
	Texture::TEXTURE_DESC textureDesc{ Get_LevelIndex(), L"Effect_PortalFrame" };
	m_PortalTexture = Add_Component<Texture>(Get_LevelIndex(), &textureDesc);
	if (!m_Shader || !m_PortalBuffer || !m_PortalTexture)
	{
		LOG_ERROR(L"Failed to ready the portal mark of AreaTrigger {}", m_ObjectName);
		return E_FAIL;
	}
	return S_OK;
}

void AreaTrigger::Update(Float timeDelta)
{
	m_Transform->Update_WorldMatrix();
	m_Box->Update(m_Transform->Get_WorldMatrix());
	if (m_LevelPending)
		Request_NextLevel();
	if (m_AreaLights.empty())
		return;
	if (!m_LightOverride)
	{
		Leave_AreaLight();
		return;
	}
	for (const Weak<DirectionalLight>& weak : m_AreaLights)
		if (const Shared<DirectionalLight> light = weak.lock())
			light->Set_AreaLight(m_LightColor, m_LightAmbient, m_LightExposure, m_LightFogOpacity);
}

void AreaTrigger::Submit_RenderGroup()
{
	if (m_NextLevel >= 0)
		GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::EMISSIVE, shared_from_this());
}

// The frame's line runs along the box's footprint, on the floor straight under its centre (the box's bottom where no
// floor is hit within PortalFloorProbe).
HRESULT AreaTrigger::Render()
{
	const BoundingOrientedBox& box = m_Box->Get_Bounding();
	const Quaternion orientation{ box.Orientation };
	const Vector3 center{ box.Center };
	Vector3 floor = center - Vector3::Transform(Vector3::UnitY, orientation) * box.Extents.y;
	Vector3 hit{};
	if (GAME_INSTANCE->Raycast_Physics(center, -Vector3::UnitY, box.Extents.y + PortalFloorProbe, hit))
		floor = hit;
	Matrix world{};
	world.Right(Vector3::Transform(Vector3::UnitX, orientation) * (2.f * box.Extents.x / PortalFrameSpan.x));
	world.Up(Vector3::Transform(Vector3::UnitZ, orientation) * (2.f * box.Extents.z / PortalFrameSpan.y));
	world.Translation(floor + Vector3{ 0.f, PortalFloorLift, 0.f });

	const uint32 glow = 0;
	const Float fade = 1.f;
	if (FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ViewMatrix, D3DTS::VIEW)) ||
		FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ProjMatrix, D3DTS::PROJ)) ||
		FAILED(GAME_INSTANCE->Bind_CameraPosition(m_Shader, CameraPosition)) ||
		FAILED(m_Shader->Bind_RawValue("g_QuadTint", &PortalTint, sizeof(Vector4))) ||
		FAILED(m_Shader->Bind_RawValue("g_QuadGlow", &glow, sizeof(uint32))) ||
		FAILED(m_Shader->Bind_Matrix(WorldMatrix, &world)) ||
		FAILED(m_Shader->Bind_RawValue("g_FadeAlpha", &fade, sizeof(Float))) ||
		FAILED(m_PortalTexture->Bind_ShaderResourceView(m_Shader, DiffuseMap, 0)) ||
		FAILED(m_Shader->Begin("Quad")) ||
		FAILED(m_PortalBuffer->Bind_Resources()))
		return E_FAIL;
	return m_PortalBuffer->Render();
}

/* The player's movement collider is on Pl0000 (PlayerPhysical); its Body (Player) would report the same entry twice. */
Bool AreaTrigger::Is_Player(const Shared<Collider>& targetCollider) const
{
	const Shared<GameObject> target = targetCollider ? targetCollider->Get_Owner() : nullptr;
	return target && L"PlayerPhysical" == target->Get_LayerMask().Get_LayerName();
}

void AreaTrigger::Enter_AreaLight()
{
	if (!m_AreaLights.empty())
		return;
	for (const auto& [id, object] : GAME_INSTANCE->Get_GameObjects(Get_LevelIndex()))
	{
		if (const auto firework = Cast<Firework>(object))
			firework->Set_Muted(true);
		const auto light = Cast<DirectionalLight>(object);
		if (!light)
			continue;
		light->Enter_AreaLight();
		light->Set_AreaLight(m_LightColor, m_LightAmbient, m_LightExposure, m_LightFogOpacity);
		m_AreaLights.push_back(light);
	}
	LOG_INFO(L"AreaTrigger {}: area light on", m_ObjectName);
}

void AreaTrigger::Leave_AreaLight()
{
	if (m_LightOverride)
		for (const auto& [id, object] : GAME_INSTANCE->Get_GameObjects(Get_LevelIndex()))
			if (const auto firework = Cast<Firework>(object))
				firework->Set_Muted(false);
	if (m_AreaLights.empty())
		return;
	for (const Weak<DirectionalLight>& weak : m_AreaLights)
		if (const Shared<DirectionalLight> light = weak.lock())
			light->Clear_AreaLight();
	m_AreaLights.clear();
	LOG_INFO(L"AreaTrigger {}: area light off", m_ObjectName);
}

void AreaTrigger::OnCollisionEnter(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
	if (!Is_Player(targetCollider))
		return;

	if (m_LightOverride)
		Enter_AreaLight();

	if (m_CameraEvent >= 0 && m_CameraEvent < ETOI(CAMERA_EVENT::END))
		if (const auto client = ClientApp::GetInstance())
			client->Play_CameraEvent(static_cast<CAMERA_EVENT>(m_CameraEvent));

	// UIManager plays each cue once per level visit, so re-entering the box does not repeat it.
	if (m_Communication >= 0)
	{
		if (m_Communication >= ETOI(COMMUNICATION_CUE::END))
			LOG_WARN(L"AreaTrigger {}: Communication {} is not a cue", m_ObjectName, m_Communication);
		else if (const auto client = ClientApp::GetInstance())
			client->Get_UIManager()->Queue_Scenario(static_cast<COMMUNICATION_CUE>(m_Communication));
	}

	if (m_NextLevel < 0 || m_LevelRequested)
		return;
	if (m_NextLevel <= ETOI(LEVEL::LOADING) || m_NextLevel >= ETOI(LEVEL::LEVEL_END))
	{
		LOG_WARN(L"AreaTrigger {}: NextLevel {} is not a playable level", m_ObjectName, m_NextLevel);
		return;
	}
	m_LevelPending = true;
}

// The fade starts once the call this trigger queued (or one still playing) has ended; the level change cancels any.
void AreaTrigger::Request_NextLevel()
{
	if (const auto client = ClientApp::GetInstance(); client && client->Get_UIManager()->Is_CommunicationPlaying())
		return;
	m_LevelPending = false;
	// Loading builds a LevelGamePlay for every gameplay level index, so the index is the type check.
	const uint32 levelIndex = GAME_INSTANCE->Get_CurrentLevelIndex();
	const auto level = levelIndex == ETOI(LEVEL::GAMEPLAY) || levelIndex == ETOI(LEVEL::AMUSEMENT_PARK)
		? static_pointer_cast<LevelGamePlay>(GAME_INSTANCE->Get_CurrentLevel()) : nullptr;
	if (level)
        m_LevelRequested = level->Request_LevelChange(static_cast<LEVEL>(m_NextLevel));
    else
        LOG_WARN(L"AreaTrigger {}: current level does not own a gameplay transition", m_ObjectName);
    if (m_LevelRequested) LOG_INFO(L"AreaTrigger {}: fading to loading level {}", m_ObjectName, m_NextLevel);
}

void AreaTrigger::OnCollisionExit(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
	if (Is_Player(targetCollider))
		Leave_AreaLight();
}

Shared<AreaTrigger> AreaTrigger::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<AreaTrigger>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		LOG_ERROR(L"Failed to Created : AreaTrigger");
		MSG_BOX("Failed to Created : AreaTrigger");
	}
	return prototype;
}

Shared<GameObject> AreaTrigger::Clone(void* arg)
{
	auto instance = make_shared<AreaTrigger>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		LOG_ERROR(L"Failed to Cloned : AreaTrigger");
		MSG_BOX("Failed to Cloned : AreaTrigger");
	}
	return instance;
}
