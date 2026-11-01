#include "SoundEmitter.h"
#include "Game.h"
#include "GameObject.h"
#include "PartObject.h"
#include "SpdLogger.h"
#include "Transform.h"

SoundEmitter::SoundEmitter() : Component{} {}
SoundEmitter::SoundEmitter(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: Component{ device, context } {}
SoundEmitter::SoundEmitter(const SoundEmitter& rhs)
	: Component(rhs), m_SoundTag(rhs.m_SoundTag), m_Volume(rhs.m_Volume),
	m_NearDistance(rhs.m_NearDistance), m_FarDistance(rhs.m_FarDistance) {}

SoundEmitter::~SoundEmitter()
{
	if (m_LoopHandle)
		GAME_INSTANCE->StopLoopSound3D(m_LoopHandle);
}

HRESULT SoundEmitter::Initialize_Prototype()
{
	return Component::Initialize_Prototype();
}

HRESULT SoundEmitter::Initialize(void* arg)
{
	if (FAILED(Component::Initialize(arg)))
		return E_FAIL;

	if (arg != nullptr)
	{
		const auto& desc = *static_cast<SOUND_EMITTER_DESC*>(arg);
		m_SoundTag = desc.soundTag;
		m_Volume = desc.volume;
		m_NearDistance = desc.nearDistance;
		m_FarDistance = desc.farDistance;
	}
	return S_OK;
}

void SoundEmitter::Play(const wstring& soundTag) const
{
	GAME_INSTANCE->PlaySound3D(soundTag, m_Volume, Get_OwnerPosition(), m_NearDistance, m_FarDistance, false);
}

void SoundEmitter::Set_SoundTag(wstring soundTag)
{
	m_SoundTag = std::move(soundTag);
	Restart_Loop();
}

void SoundEmitter::Set_Volume(Float volume)
{
	m_Volume = volume;
	Restart_Loop();
}

void SoundEmitter::Set_NearDistance(Float distance)
{
	m_NearDistance = distance;
	Restart_Loop();
}

void SoundEmitter::Set_FarDistance(Float distance)
{
	m_FarDistance = distance;
	Restart_Loop();
}

/* The scene sets the properties and the owner's transform before this, so the loop starts at its placed position. */
HRESULT SoundEmitter::Post_Load()
{
	m_Loaded = true;
	Restart_Loop();
	return S_OK;
}

Vector3 SoundEmitter::Get_OwnerPosition() const
{
	const auto owner = Get_Owner();
	if (!owner)
		return Vector3{};
	if (const auto part = dynamic_pointer_cast<PartObject>(owner))
		return part->Get_CombinedWorldMatrix().Translation();
	return owner->Get_Transform()->Get_WorldMatrix().Translation();
}

void SoundEmitter::Restart_Loop()
{
	// Property writes while the scene loads come before the transform; Post_Load starts the loop once.
	if (!m_Loaded)
		return;
	if (m_LoopHandle)
	{
		GAME_INSTANCE->StopLoopSound3D(m_LoopHandle);
		m_LoopHandle = 0;
	}
	if (m_SoundTag.empty())
		return;
	m_LoopHandle = GAME_INSTANCE->PlaySound3D(m_SoundTag, m_Volume, Get_OwnerPosition(), m_NearDistance, m_FarDistance, true);
	if (0 == m_LoopHandle)
		LOG_WARN(L"SoundEmitter: sound {} is not loaded", m_SoundTag);
}

Shared<Component> SoundEmitter::Clone(void* arg)
{
	auto clone = make_shared<SoundEmitter>(*this);
	if (FAILED(clone->Initialize(arg))) return nullptr;
	return clone;
}

Shared<SoundEmitter> SoundEmitter::CreatePrototype()
{
	auto instance = make_shared<SoundEmitter>();
	if (FAILED(instance->Initialize_Prototype())) return nullptr;
	return instance;
}

Shared<SoundEmitter> SoundEmitter::Create(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context)
{
	auto instance = make_shared<SoundEmitter>(device, context);
	if (FAILED(instance->Initialize_Prototype())) return nullptr;
	return instance;
}
