#include "pch.h"
#include "WP3000Body.h"

#include <SpdLogger.h>

#include "Bullet.h"
#include "Collider.h"
#include "FireFlashEffect.h"
#include "Game.h"
#include "Light.h"
#include "Model.h"
#include "Pl0000.h"
#include "Random_Helper.h"


namespace
{
	// Pod lamp: white light a little below the pod. Full in the dark, a faint glow under daylight; the darkness is
	// read from the active sun and ambient (their summed luma from LampDarkLuma down to LampBrightLuma).
	constexpr Float LampRange = 6.f;
	constexpr Float LampBelow = 0.3f;
	const Vector4 LampColor{ 0.9f, 0.9f, 0.85f, 1.f };
	constexpr Float LampDaylight = 0.26f;
	constexpr Float LampBrightLuma = 0.6f;
	constexpr Float LampDarkLuma = 0.15f;
}

NS_BEGIN(Client)
	WP3000Body::WP3000Body() : Pl0000Parts{} {}
WP3000Body::WP3000Body(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: Pl0000Parts{ device, context } {}
WP3000Body::WP3000Body(const WP3000Body& rhs)
	: Pl0000Parts{ rhs } {}

HRESULT WP3000Body::Initialize_Prototype()
{
	return Pl0000Parts::Initialize_Prototype();
}

HRESULT WP3000Body::Initialize(void* arg)
{
	if (FAILED(Pl0000Parts::Initialize(arg)))
		return E_FAIL;

	if (FAILED(Ready_Components()))
	{
		LOG_ERROR(L"Failed To Ready_Components : WP3000Body");
		return E_FAIL;
	}

    // POD 모델 루트 본 설정 (있는 경우)
	m_RootBoneIndex = m_Model->Get_BoneIndexByName("wp3000");

	if (m_RootBoneIndex != -1)
	{
		m_Model->Set_LocalRootNode(m_RootBoneIndex);
	}
	m_AntennaBoneIndex = m_Model->Get_BoneIndexByName("bone5");
	if (m_AntennaBoneIndex == -1)
		LOG_WARN(L"WP3000Body : wp3000 has no antenna bone (bone5); the communication light starts at the pod");

    m_Model->Set_Animation(0, 0.f);

	// Off until Late_Update first places it at the pod.
	LIGHT_DESC lampDesc{};
	lampDesc.type = LIGHT::POINT;
	lampDesc.range = LampRange;
	m_Lamp = Light::Create(lampDesc);
	if (nullptr == m_Lamp || FAILED(GAME_INSTANCE->Add_Light(m_Lamp)))
	{
		LOG_ERROR(L"Failed to register the lamp of WP3000Body {}", m_ObjectName);
		return E_FAIL;
	}
	m_Lamp->Set_Active(false);

	return S_OK;
}

void WP3000Body::On_Destroy()
{
	if (m_Lamp)
		GAME_INSTANCE->Remove_Light(m_Lamp);
	m_Lamp.reset();
	PartObject::On_Destroy();
}

void WP3000Body::On_Disable()
{
	Pl0000Parts::On_Disable();
	if (m_Lamp)
		m_Lamp->Set_Active(false);
}

void WP3000Body::Priority_Update(Float timeDelta)
{
}

void WP3000Body::Update(Float timeDelta)
{
	// Shift toggles shooting. It stops whenever the player's input is off (cinematics, input lock).
	const auto player = static_pointer_cast<Pl0000>(Get_Owner());
	const auto input = player ? player->Get_Component<Pl0000Input>() : nullptr;
	if (!input || !input->Is_Active() || !GAME_INSTANCE->Get_InputEnabled())
		m_ShootToggled = false;
	else if (input->Is_KeyDown(static_cast<uByte>(DIK_LSHIFT)))
		m_ShootToggled = !m_ShootToggled;

	if (m_PodState == POD_STATE::GLIDE)
	{
		m_Model->Update_ModelAnimation(timeDelta);
		return;
	}

	// The laser state drives the pod, so the shot key is ignored until the head has closed.
	if (m_PodState == POD_STATE::LASER_START && Is_AnimationFinished())
	{
		m_PodState = POD_STATE::LASER_LOOP;
		Set_Animation(ETOI(POD_STATE::LASER_LOOP), 0.1f, true);
	}
	else if (m_PodState == POD_STATE::LASER_END && Is_AnimationFinished())
	{
		m_PodState = POD_STATE::IDLE;
		Set_Animation(ETOI(POD_STATE::IDLE), 0.2f, false);
	}
	if (m_PodState == POD_STATE::LASER_START || m_PodState == POD_STATE::LASER_LOOP || m_PodState == POD_STATE::LASER_END)
	{
		m_Model->Update_ModelAnimation(timeDelta);
		return;
	}

	uint32 animIndex = Get_CurrentAnimationIndex();

	if (m_ShootToggled)
	{
		if (animIndex == ETOI(POD_STATE::IDLE))
		{
			m_PodState = POD_STATE::SHOOT_START;
			Set_Animation(ETOI(POD_STATE::SHOOT_START), 0.2f, false);
		}
		else if (animIndex == ETOI(POD_STATE::SHOOT_START) &&
			Is_AnimationFinished())
		{
			m_PodState = POD_STATE::SHOOT_LOOP;
			Set_Animation(ETOI(POD_STATE::SHOOT_LOOP), 0.2f, true);
		}
	}
	else
	{
		if (animIndex == ETOI(POD_STATE::SHOOT_LOOP))
		{
			m_PodState = POD_STATE::SHOOT_END;
			Set_Animation(ETOI(POD_STATE::SHOOT_END), 0.2f, false);
		}
		else if (animIndex == ETOI(POD_STATE::SHOOT_END) &&
			Is_AnimationFinished())
		{
			m_PodState = POD_STATE::IDLE;
			Set_Animation(ETOI(POD_STATE::IDLE), 0.2f, false);
		}
	}

	if (animIndex == ETOI(POD_STATE::SHOOT_LOOP))
		Pod_Fire(timeDelta);

	m_Model->Update_ModelAnimation(timeDelta);
}

void WP3000Body::Late_Update(Float timeDelta)
{
	if (!Get_Parent()) return;
	if (m_Pl0000Body.expired()) return;

	Float hoverOffset = 0.f;
	if (m_PodState == POD_STATE::IDLE)
	{
		m_HoverTime += timeDelta * m_HoverSpeed;
		hoverOffset = std::sin(m_HoverTime) * m_HoverAmplitude;
	}
	
	Vector3 targetOffset = m_TargetOffset;
	targetOffset.y += hoverOffset;
	Float blendStep = timeDelta / m_GripBlendTime;
	m_GripBlend = std::clamp(m_GripBlend + (m_PodState == POD_STATE::GLIDE ? blendStep : -blendStep), 0.f, 1.f);
	if (m_GripBlend > 0.f && m_GripBoneIndex != -1)
	{
		// The glide clip's root translation is stripped, so the pod is placed at the hand the player's clip raises.
		auto body = m_Pl0000Body.lock();
		Vector3 grip = body->Get_ModelComponent()->Get_BoneMatrix(m_GripBoneIndex).Translation();
		// Local, not world: the pod's own local position is already relative to the player.
		grip = Vector3::Transform(grip, body->Get_Transform()->Get_LocalMatrix());
		grip.y += m_GripHeight;
		Float t = m_GripBlend * m_GripBlend * (3.f - 2.f * m_GripBlend);
		targetOffset = Vector3::Lerp(targetOffset, grip, t);
	}
	m_Transform->Set_LocalPosition(targetOffset);

	m_Transform->Update_WorldMatrix();

	Update_CombineWorldMatrix(*m_Transform->Get_WorldMatrixPtr());

	if (m_PodState != POD_STATE::IDLE && m_PodState != POD_STATE::GLIDE)
	{
		const Shared<GameObject>& camera = GAME_INSTANCE->Get_MainCamera();
		if (camera != nullptr)
		{
			Vector3 scale, position;
			Quaternion rot;

			m_CombinedWorldMatrix.Decompose(scale, rot, position);
			
			Quaternion cameraRot = camera->Get_Transform()->Get_Quaternion();
			m_CombinedWorldMatrix = Matrix::CreateScale(scale)
				* Matrix::CreateFromQuaternion(cameraRot)
				* Matrix::CreateTranslation(position);
		}
	}

	// The muzzle sits ahead of the pod along its (camera-facing while shooting) look and slightly above it.
	Vector3 scale{}, position{};
	Quaternion facing{};
	m_CombinedWorldMatrix.Decompose(scale, facing, position);
	Vector3 look = m_CombinedWorldMatrix.Backward();
	look.Normalize();
	m_MuzzlePosition = position + look * 0.5f + Vector3{ 0.f, 0.25f, 0.f };
	m_AntennaPosition = m_AntennaBoneIndex == -1 ? position :
		Vector3::Transform(m_Model->Get_BoneMatrix(m_AntennaBoneIndex).Translation(), m_CombinedWorldMatrix);
	if (auto flash = m_FireFlash.lock())
	{
		const Bool firing = m_PodState != POD_STATE::IDLE && m_PodState != POD_STATE::GLIDE;
		flash->Set_Muzzle(m_MuzzlePosition, facing, firing);
	}
	Update_Lamp();
}

void WP3000Body::Update_Lamp()
{
	if (!m_Lamp)
		return;
	Float strength = 1.f;
	if (const LIGHT_DESC* sun = GAME_INSTANCE->Find_ActiveLight(LIGHT::DIRECTIONAL))
	{
		const Vector3 luma{ 0.2126f, 0.7152f, 0.0722f };
		const Float light = luma.Dot(Vector3{ sun->diffuse.x, sun->diffuse.y, sun->diffuse.z }) +
			luma.Dot(Vector3{ sun->ambient.x, sun->ambient.y, sun->ambient.z });
		const Float dark = std::clamp((LampBrightLuma - light) / (LampBrightLuma - LampDarkLuma), 0.f, 1.f);
		strength = LampDaylight + (1.f - LampDaylight) * dark * dark * (3.f - 2.f * dark);
	}
	LIGHT_DESC desc = m_Lamp->Get_LightDesc();
	const Vector3 position = m_CombinedWorldMatrix.Translation();
	desc.position = Vector4{ position.x, position.y - LampBelow, position.z, 1.f };
	desc.diffuse = LampColor * strength;
	desc.specular = desc.diffuse;
	m_Lamp->Set_LightDesc(desc);
	m_Lamp->Set_Active(Is_Active());
}

void WP3000Body::Fixed_Update(Float fixedDelta)
{
}

void WP3000Body::Set_Pl0000Body(const Shared<Pl0000Body>& body)
{
	m_Pl0000Body = body;
	// pl0000 hand bone13; its fingers are the highest bones while hanging from the pod in pl0000_001f.
	m_GripBoneIndex = body->Get_ModelComponent()->Get_BoneIndexByName("bone13");
	if (m_GripBoneIndex == -1)
		LOG_ERROR(L"WP3000Body : pl0000 has no grip bone (bone13)");
}

void WP3000Body::Set_FireFlash(const Shared<FireFlashEffect>& flash)
{
	m_FireFlash = flash;
}

void WP3000Body::Begin_Glide()
{
	m_PodState = POD_STATE::GLIDE;
	Set_Animation(ETOI(POD_STATE::GLIDE), 0.1f, false);
}

void WP3000Body::End_Glide()
{
	m_PodState = POD_STATE::IDLE;
	Set_Animation(ETOI(POD_STATE::IDLE), 0.2f, false);
}

void WP3000Body::Begin_Laser()
{
	m_PodState = POD_STATE::LASER_START;
	Set_Animation(ETOI(POD_STATE::LASER_START), 0.1f, false);
}

Vector3 WP3000Body::Get_AimDirection() const
{
	// Only the lock G took aims at its target; otherwise the beam goes where the bullets do (Pod_Fire): along the
	// camera, raised a little.
	if (const auto player = static_pointer_cast<Pl0000>(Get_Owner()); player && player->Is_ManualLockOn())
	{
		if (const auto target = player->Get_LockOnTarget().lock())
		{
			// A monster's body collider sits at its middle; its position is at its feet.
			const auto collider = target->Get_Component<Collider>();
			const Vector3 aim = collider ? collider->Get_Pivot() : target->Get_Transform()->Get_Position() + Vector3::UnitY;
			return aim - m_MuzzlePosition;
		}
	}
	const auto camera = GAME_INSTANCE->Get_MainCamera();
	Vector3 look = camera ? camera->Get_Transform()->Get_Look() : m_CombinedWorldMatrix.Backward();
	look.Normalize();
	return look + Vector3{ 0.f, 0.25f, 0.f };
}

void WP3000Body::End_Laser()
{
	if (m_PodState != POD_STATE::LASER_START && m_PodState != POD_STATE::LASER_LOOP)
		return;
	m_PodState = POD_STATE::LASER_END;
	Set_Animation(ETOI(POD_STATE::LASER_END), 0.1f, false);
}

HRESULT WP3000Body::Render()
{
	if (FAILED(Bind_ShaderResources()))
		return E_FAIL;

	size_t numMeshes = m_Model->Get_NumMeshes();
	for (uint32 i = 0; i < numMeshes; ++i)
	{
		if (FAILED(m_Model->Bind_BoneMatrices(m_Shader, BoneMatrices, i)))
			return E_FAIL;

		if (FAILED(m_Model->BindAndBeginMaterial(m_Shader, i)))
			return E_FAIL;

		if (FAILED(m_Model->Render(i))) return E_FAIL;
	}

	return S_OK;
}

void WP3000Body::Submit_RenderGroup()
{
	if (Is_Active())
		GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::NONBLEND, shared_from_this());
}

HRESULT WP3000Body::Bind_ShaderResources()
{
	if (FAILED(m_Shader->Bind_Matrix(WorldMatrix, &m_CombinedWorldMatrix)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ViewMatrix, D3DTS::VIEW)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ProjMatrix, D3DTS::PROJ)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Bind_CameraPosition(m_Shader, CameraPosition)))
		return E_FAIL;

	const LIGHT_DESC* lightDesc = GAME_INSTANCE->Get_LightDesc(0);
	if (nullptr == lightDesc)
		return E_FAIL;

	if (FAILED(m_Shader->Bind_RawValue(DirectionLight, &lightDesc->direction, sizeof(Float4))))
		return E_FAIL;
	if (FAILED(m_Shader->Bind_RawValue(DiffuseLight, &lightDesc->diffuse, sizeof(Float4))))
		return E_FAIL;
	if (FAILED(m_Shader->Bind_RawValue(AmbientLight, &lightDesc->ambient, sizeof(Float4))))
		return E_FAIL;
	if (FAILED(m_Shader->Bind_RawValue(SpecularLight, &lightDesc->specular, sizeof(Float4))))
		return E_FAIL;

	return S_OK;
}

HRESULT WP3000Body::Ready_Components()
{
	Shader::SHADER_DESC shaderDesc{ WeaponShaderTag,  VTXANIMMESH::Elements, VTXANIMMESH::numElements };
	m_Shader = Add_Component<Shader>(ETOI(LEVEL::STATIC), &shaderDesc);
	if (nullptr == m_Shader)
		return E_FAIL;

	Model::MODEL_DESC modelDesc{ L"wp3000" };
	m_Model = Add_Component<Model>(ETOI(LEVEL::STATIC), &modelDesc);
	if (nullptr == m_Model)
		return E_FAIL;
	if (FAILED(m_Model->Validate_MaterialBindings(m_Shader))) return E_FAIL;

	return S_OK;
}

void WP3000Body::Pod_Fire(Float timeDelta)
{
	m_FireRateTimer += timeDelta;
	// Shift로 사격을 켜 둔 동안

	if (m_ShootToggled)
	{
		if (m_FireRateTimer >= m_FireRate)
		{
			m_FireRateTimer = 0.f;
			// 총알 매개변수 세팅
			Entity::DAMAGE_INFO dmgInfo{};
			dmgInfo.attacker = Get_Owner();
			dmgInfo.attackType = ATK_TYPE::POD;
			dmgInfo.damage = static_cast<Float>(Helper::Random_Double(10.f, 20.f));
			dmgInfo.groggyWeight = 0;
			dmgInfo.knockbackForce = 1.f;

			Bullet::BULLET_DESC desc{};
			desc.damageInfo = dmgInfo;
			desc.resourceTag = L"candy";
			desc.targetLayer = L"Monster";
			desc.isPermanent = false;
			desc.speed = 30.0f; // 탄속
			desc.maxDistance = 100.f; 
			desc.initialPosition = m_MuzzlePosition;
			desc.direction = m_CombinedWorldMatrix.Backward() + Vector3{ 0.f, 0.25f, 0.f };

			if (auto flash = m_FireFlash.lock())
				flash->Flash();
			GAME_INSTANCE->Instantiate<Bullet>(L"Bullet", Get_LevelIndex(), &desc);
			GAME_INSTANCE->StopSound(SOUNDCHANNEL::CHANNEL_14);
			GAME_INSTANCE->PlaySoundFXOnce(L"Wp3000_Shot", SOUNDCHANNEL::CHANNEL_14, 0.4f);
		}
	}
}

Shared<WP3000Body> WP3000Body::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<WP3000Body>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : WP3000Body");
		return nullptr;
	}

	return prototype;
}

Shared<GameObject> WP3000Body::Clone(void* arg)
{
	auto instance = make_shared<WP3000Body>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : WP3000Body");
		return nullptr;
	}

	return instance;
}

NS_END
