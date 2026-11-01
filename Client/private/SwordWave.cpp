#include "pch.h"
#include "SwordWave.h"

#include <Game.h>
#include <Model.h>
#include <OBBCollider.h>
#include <PartObject.h>
#include <Shader.h>
#include <Texture.h>

#include "Monster.h"
#include "Pl0000.h"
#include "Pl0000Parts.h"

namespace
{
	// SwordWave.model holds three meshes. Mesh 0, the trail, is ef0901 baked with XZ x12 (outer radius 1.36); it is flat
	// (Y +-0.00065), so only the XZ scale shows. The converter mirrors Z, so in model space the crescent's wide end lies
	// at -108.8 degrees about Y and its thin tail runs on toward +80. The wide end leads the swing; the tail lies along
	// the path behind it.
	const Vector3 WaveWideEnd{ -0.3223f, 0.f, -0.9466f };
	const Vector3 WaveTailSide{ 0.9466f, 0.f, -0.3223f };
	// The trail wpf000.eff/111 is sc(20, 12): drawn at 20/12 (radius 2.27 m, matching the ~2 m refraction arc in-game).
	constexpr Float TrailScale = 20.f / 12.f;
	// Mesh 1, the flying wave, is a thick glass crescent made to match the in-game captures (ef0901 with core.eff/595's
	// textures only read as thin brush strokes). It is a U: a half circle (outer radius 1.15) with its middle at +X,
	// convex side first, and straight legs running 0.7 m back (-X) from both ends; the wide lead end is at -Z.
	const Vector3 WaveArcMiddle{ 1.f, 0.f, 0.f };
	const Vector3 WaveTowardWide{ 0.f, 0.f, -1.f };
	// Hit box in model space around the U (x -0.7..1.15, z -1.15..1.15), thickened so it can hit.
	const Vector3 WaveBoxCenter{ 0.225f, 0.f, 0.f };
	const Vector3 WaveBoxExtents{ 0.925f, 0.35f, 1.15f };
	// Mesh 2 holds glass shards, no longer drawn: the torn slash crescents below took their place.
	constexpr uint32 TrailMesh = 0;
	constexpr uint32 WaveMesh = 1;

	// The swing turns about the chest; the plane is known once the tip has turned this far or the time is up.
	constexpr Float WavePivotHeight = 1.1f;
	constexpr Float WaveLaunchSweep = 0.9f;		// radians
	constexpr Float WaveMinSweep = 0.3f;
	constexpr Float WaveMaxTrackTime = 0.15f;

	constexpr Float WaveLifetime = 0.6f;
	constexpr Float WaveFadeInTime = 0.04f;
	constexpr Float WaveFadeOutFrom = 0.85f;	// of the lifetime
	constexpr Float WaveWearFrom = 0.3f;		// of the lifetime: from here the glass wears away until none is left
	constexpr Float WaveHitEndTime = 0.5f;
	constexpr Float WaveSpeed = 32.f;
	constexpr Float WaveLaunchAhead = 1.5f;		// it leaves this far in front of the chest, its legs clear of 2B
	constexpr Float WaveGrowth = 0.35f;			// extra scale over the flight
	constexpr Float WaveWhite = 0.75f;			// GlassWave: scale of the crumpled white (rim, creases, faces)
	constexpr Float WaveRefraction = 2.5f;		// GlassWave: scale of how far it bends the scene behind it
	constexpr Float WaveEmissive = 0.85f;		// GlassGlow: strength of the light added over it
	constexpr Float WaveGlowSpread = 0.35f;		// GlassGlow: metres the glowing shell swells past the band's edges
	// SlashArcLine: the white streaks of coreeff 007/5 along the wave's convex side, its thick end toward the lead, in a
	// soft shimmering haze of 007/6 that makes it dreamlike.
	constexpr Float WaveLineTurn = 35.f;		// degrees
	constexpr Float WaveLine = 1.f;
	constexpr Float WaveHaze = 0.45f;
	// What the wave sheds (SlashArc): torn sheets of coreeff 007/3 and 007/4 (in turn), one about every EchoSpacing
	// seconds of its flight (jittered) until it starts to fade. Each goes on at EchoDrift of the wave's speed, so they
	// trail it as a short tunnel, swelling by EchoSpread, rising or sinking a little, tearing further and softening from
	// a grey sheet with white torn edges into faint smoke, and is gone after EchoLifetime. EchoTurns (degrees) lay each
	// image's arc along the U, its thick end toward the lead, each a little differently.
	constexpr Float EchoSpacing = 0.03f;
	constexpr Float EchoSpawnEnd = WaveLifetime * 0.75f;
	constexpr Float EchoLifetime = 0.35f;
	constexpr Float EchoFadeInTime = 0.03f;
	constexpr Float EchoDrift = 0.1f;
	constexpr Float EchoSpread = 0.7f;
	constexpr Float EchoRise = 0.6f;			// metres per second, up or down by sheet
	constexpr Float EchoTurns[] = { 80.f, 140.f, 68.f, 122.f, 92.f, 134.f, 74.f };
	constexpr Float EchoWearFrom = 0.3f;
	constexpr Float EchoWearTo = 0.9f;
	constexpr Float EchoRefraction = 0.8f;
	constexpr Float EchoInk = 0.2f;
	constexpr Float EchoWhite = 1.4f;
	// The swing's trail, wpf000.eff/111: the same crescent left where it was swung, refraction only (TSN 004/2,
	// distortion 1.1), fading out.
	constexpr Float TrailLifetime = 0.35f;
	constexpr Float TrailRefraction = 1.1f;
	constexpr Float TrailSmudge = 0.35f;		// the trail's refraction dims the scene up to this much
	constexpr Float SlashLifetime = std::max({ WaveLifetime, TrailLifetime, EchoSpawnEnd + EchoLifetime });
}

SwordWave::SwordWave(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: GameObject{ device, context } {}
SwordWave::SwordWave(const SwordWave& rhs) : GameObject{ rhs } {}

HRESULT SwordWave::Initialize_Prototype()
{
	m_LayerMask.Set_Layer(L"PlayerWeapon");
	m_TagMask.Set_Tag({ L"PlayerWeapon" });
	return GameObject::Initialize_Prototype();
}

HRESULT SwordWave::Initialize(void* arg)
{
	if (!arg || FAILED(GameObject::Initialize(arg))) return E_FAIL;
	Shader::SHADER_DESC shaderDesc{ L"VTX_Mesh_Effect", VTXMESH::Elements, VTXMESH::numElements };
	m_Shader = Add_Component<Shader>(ETOI(LEVEL::STATIC), &shaderDesc);
	Model::MODEL_DESC modelDesc{ L"SwordWave" };
	m_Model = Add_Component<Model>(Get_LevelIndex(), &modelDesc);
	Texture::TEXTURE_DESC textureDesc{ Get_LevelIndex(), L"Effect_Slash" };
	m_Texture = Add_Component<Texture>(Get_LevelIndex(), &textureDesc);
	OBBCollider::OBB_COLLIDER_DESC colliderDesc{};
	m_Collider = Add_Component<OBBCollider>(ETOI(LEVEL::STATIC), &colliderDesc);
	if (!m_Shader || !m_Model || !m_Texture || !m_Collider || m_Model->Get_NumMeshes() != 3) return E_FAIL;
	m_Collider->Set_Active(false);
	return S_OK;
}

void SwordWave::Swing(const Shared<Pl0000Parts>& weapon, const Vector3& tipOffset, ATK_TYPE attackType, const optional<Float>& roll)
{
	m_BoneIndex = weapon->Get_ModelComponent()->Get_BoneIndexByName("bone0");
	if (m_BoneIndex < 0) return;
	m_Weapon = weapon;
	m_TipOffset = tipOffset;
	m_AttackType = attackType;
	m_Roll = roll;
	m_LastTip = Vector3::Transform(tipOffset,
		weapon->Get_ModelComponent()->Get_BoneMatrix(m_BoneIndex) * weapon->Get_CombinedWorldMatrix());
	m_Spin = Vector3::Zero;
	m_Swept = 0.f;
	m_Age = 0.f;
	m_Phase = PHASE::TRACKING;
}

void SwordWave::Track(Float timeDelta)
{
	auto weapon = m_Weapon.lock();
	auto owner = Get_Parent();
	if (!weapon || !owner)
	{
		m_Phase = PHASE::IDLE;
		return;
	}
	m_Age += timeDelta;
	const Vector3 tip = Vector3::Transform(m_TipOffset,
		weapon->Get_ModelComponent()->Get_BoneMatrix(m_BoneIndex) * weapon->Get_CombinedWorldMatrix());
	const Vector3 pivot = owner->Get_Transform()->Get_Position() + Vector3{ 0.f, WavePivotHeight, 0.f };
	const Vector3 reach = tip - pivot;
	const Vector3 step = tip - m_LastTip;
	m_Spin += reach.Cross(step);
	m_Swept += step.Length() / std::max(reach.Length(), 0.3f);
	m_LastTip = tip;
	if (m_Swept >= WaveLaunchSweep || (m_Age >= WaveMaxTrackTime && m_Swept >= WaveMinSweep))
		Launch(pivot, tip);
	else if (m_Age >= WaveMaxTrackTime)
		m_Phase = PHASE::IDLE;	// the blade hardly moved: no swing to throw
}

void SwordWave::Launch(const Vector3& pivot, const Vector3& tip)
{
	m_Phase = PHASE::IDLE;
	if (m_Spin.LengthSquared() < 1e-8f) return;
	Vector3 normal = m_Spin;
	normal.Normalize();
	Vector3 facing = Get_Parent()->Get_Transform()->Get_Look();
	facing.y = 0.f;
	if (facing.LengthSquared() < 1e-6f) return;
	facing.Normalize();
	// Rows map model axes (an arc direction, the way along it toward the wide end, their Y) onto a plane's; both sets
	// are right-handed alike. In the swing plane the blade moves along n x direction, toward the wide end.
	const auto toPlane = [](const Vector3& modelArc, const Vector3& modelLead, const Vector3& direction, const Vector3& planeNormal) {
		const Vector3 lead = planeNormal.Cross(direction);
		return Matrix{ modelArc, modelLead, modelLead.Cross(modelArc) }.Transpose() * Matrix{ direction, lead, lead.Cross(direction) };
	};
	// The trail: the wide end where the blade is now, the tail back along the way it came.
	Vector3 blade = tip - pivot;
	blade -= normal * blade.Dot(normal);
	if (blade.LengthSquared() < 1e-6f) return;
	blade.Normalize();
	m_TrailMatrix = Matrix::CreateScale(TrailScale) * toPlane(WaveWideEnd, -WaveTailSide, blade, normal) * Matrix::CreateTranslation(pivot);
	// The wave lies across its flight as the swing that threw it: a horizontal swing throws it flat with its arc's
	// middle to the front, and the swing's data roll turns it about the facing (diagonal swings lean it). Without data
	// it lies in the measured swing plane, its middle toward the facing as seen in that plane.
	if (m_Roll)
		m_Orientation = toPlane(WaveArcMiddle, WaveTowardWide, facing, Vector3::UnitY) *
			Matrix::CreateFromAxisAngle(facing, XMConvertToRadians(*m_Roll));
	else
	{
		Vector3 front = facing - normal * facing.Dot(normal);
		if (front.LengthSquared() < 0.1f) front = blade;
		front.Normalize();
		m_Orientation = toPlane(WaveArcMiddle, WaveTowardWide, front, normal);
	}
	m_Start = pivot + facing * WaveLaunchAhead;
	m_Velocity = facing * WaveSpeed;
	m_Age = 0.f;
	m_HitMonsters.clear();
	m_Collider->Set_Extents(WaveBoxExtents);
	m_Collider->Set_Active(true);
	m_Phase = PHASE::FLYING;
}

void SwordWave::Late_Update(Float timeDelta)
{
	if (m_Phase == PHASE::TRACKING)
		Track(timeDelta);
	if (m_Phase != PHASE::FLYING) return;

	m_Age += timeDelta;
	if (m_Age >= SlashLifetime)
	{
		m_Phase = PHASE::IDLE;
		m_Collider->Set_Active(false);
		return;
	}
	const Float trail = m_Age / TrailLifetime;
	m_TrailOpacity = trail < 1.f ? 1.f - trail * trail : 0.f;
	const Float t = std::min(m_Age / WaveLifetime, 1.f);
	m_WorldMatrix = WaveMatrix(m_Age);
	const Float fadeOut = std::clamp((t - WaveFadeOutFrom) / (1.f - WaveFadeOutFrom), 0.f, 1.f);
	m_Opacity = std::min(m_Age / WaveFadeInTime, 1.f) * (1.f - fadeOut * fadeOut * (3.f - 2.f * fadeOut));
	if (m_Age >= WaveHitEndTime)
		m_Collider->Set_Active(false);
	else
		m_Collider->Update(Matrix::CreateTranslation(WaveBoxCenter) * m_WorldMatrix);
}

Matrix SwordWave::WaveMatrix(Float age) const
{
	return Matrix::CreateScale(1.f + WaveGrowth * std::min(age / WaveLifetime, 1.f)) * m_Orientation *
		Matrix::CreateTranslation(m_Start + m_Velocity * age);
}

HRESULT SwordWave::Draw(uint32 mesh, const char* pass, const Matrix& world, const Vector4& color)
{
	if (color.w <= 0.f) return S_OK;
	if (FAILED(m_Shader->Bind_Matrix(WorldMatrix, &world)) ||
		FAILED(m_Shader->Bind_RawValue("g_EffectColor", &color, sizeof(color))) ||
		FAILED(m_Shader->Begin(pass)))
		return E_FAIL;
	return m_Model->Render(mesh);
}

HRESULT SwordWave::Render()
{
	if (m_Phase != PHASE::FLYING) return S_OK;
	if (FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ViewMatrix, D3DTS::VIEW)) ||
		FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ProjMatrix, D3DTS::PROJ)) ||
		FAILED(m_Texture->Bind_ShaderResourceView(m_Shader, "g_SlashColorMap", 0)) ||
		FAILED(m_Texture->Bind_ShaderResourceView(m_Shader, "g_SlashMaskMap", 1)) ||
		FAILED(m_Texture->Bind_ShaderResourceView(m_Shader, "g_SlashNormalMap", 2)) ||
		FAILED(m_Texture->Bind_ShaderResourceView(m_Shader, "g_SlashArcMap0", 3)) ||
		FAILED(m_Texture->Bind_ShaderResourceView(m_Shader, "g_SlashArcMap1", 4)) ||
		FAILED(m_Texture->Bind_ShaderResourceView(m_Shader, "g_SlashArcLineMap", 5)) ||
		FAILED(m_Texture->Bind_ShaderResourceView(m_Shader, "g_SlashArcMap2", 6)) ||
		FAILED(GAME_INSTANCE->Bind_RenderTarget_ShaderResource(m_Shader, SceneMap, RT_SCENE)) ||
		FAILED(m_Shader->Bind_RawValue("g_EffectTime", &m_Age, sizeof(Float)))) return E_FAIL;
	// All draw over the scene copy: the trail's refraction, then the glass wave (refraction and white in one pass), its
	// emissive light added over a swollen shell of it, its streaks and haze, and the torn sheets it sheds blended over
	// it (the newest, nearest the wave, first). The glass wears away over the flight.
	const Float t = std::min(m_Age / WaveLifetime, 1.f);
	const Float wear = std::clamp((t - WaveWearFrom) / (1.f - WaveWearFrom), 0.f, 1.f);
	if (FAILED(Draw(TrailMesh, "SlashDistortion", m_TrailMatrix, Vector4{ TrailRefraction, TrailSmudge, 0.f, m_TrailOpacity })) ||
		FAILED(Draw(WaveMesh, "GlassWave", m_WorldMatrix, Vector4{ WaveWhite, WaveRefraction, wear, m_Opacity })))
		return E_FAIL;
	if (FAILED(Draw(WaveMesh, "GlassGlow", m_WorldMatrix, Vector4{ WaveEmissive, 0.f, WaveGlowSpread, m_Opacity * (1.f - wear) })))
		return E_FAIL;
	const Vector4 line{ XMConvertToRadians(WaveLineTurn), WaveHaze, 0.f, 0.f };
	if (FAILED(m_Shader->Bind_RawValue("g_SlashArc", &line, sizeof(line))) ||
		FAILED(Draw(WaveMesh, "SlashArcLine", m_WorldMatrix, Vector4{ WaveLine, 0.f, 0.f, m_Opacity })))
		return E_FAIL;
	for (int32 k = static_cast<int32>(EchoSpawnEnd / EchoSpacing); k >= 0; --k)
	{
		// Irregular: each sheet comes off up to 30% of the spacing early or late.
		const Float born = EchoSpacing * (k + Float((k * 7) % 5) * 0.15f - 0.3f);
		const Float elapsed = m_Age - born;
		const Float life = elapsed / EchoLifetime;
		if (born < 0.f || life <= 0.f || life >= 1.f) continue;
		const Float rise = EchoRise * (Float((k * 3) % 5) / 2.f - 1.f);
		const Matrix world = Matrix::CreateScale(1.f + EchoSpread * life) * WaveMatrix(born) *
			Matrix::CreateTranslation(m_Velocity * (EchoDrift * elapsed) + Vector3{ 0.f, rise * elapsed, 0.f });
		const Vector4 arc{ XMConvertToRadians(EchoTurns[k % std::size(EchoTurns)]), Float(k % 2),
			EchoWearFrom + (EchoWearTo - EchoWearFrom) * life, 0.37f * k + 0.1f + 0.3f * life };
		const Float opacity = std::min(elapsed / EchoFadeInTime, 1.f) * (1.f - life * life);
		if (FAILED(m_Shader->Bind_RawValue("g_SlashArc", &arc, sizeof(arc))) ||
			FAILED(Draw(WaveMesh, "SlashArc", world,
				Vector4{ EchoRefraction, EchoInk, EchoWhite * (1.f - life) * (1.f - life), opacity })))
			return E_FAIL;
	}
	return S_OK;
}

void SwordWave::Submit_RenderGroup()
{
	if (m_Phase == PHASE::FLYING)
		GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::DISTORTION, shared_from_this());
}

void SwordWave::OnCollisionStay(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
	if (!m_Collider->Is_Active()) return;
	const auto target = targetCollider->Get_Owner();
	if (!target || target->Get_GameObjectType() != GAMEOBJECTTYPE::PART) return;
	if (target->Get_LayerMask().Get_LayerName() != L"Monster") return;
	// A monster has several parts on this layer; the wave still hits it once.
	const auto monster = static_pointer_cast<Monster>(static_pointer_cast<PartObject>(target)->Get_Owner());
	if (!monster || !m_HitMonsters.insert(monster->Get_RuntimeObjectId()).second) return;

	const auto attacker = static_pointer_cast<Pl0000>(Get_Parent());
	Entity::DAMAGE_INFO damageInfo{};
	damageInfo.attacker = attacker;
	damageInfo.attackType = m_AttackType;
	damageInfo.damage = m_AttackType == ATK_TYPE::HEAVY ? 120.f : 60.f;
	damageInfo.groggyWeight = 50;
	damageInfo.knockbackForce = 1.f;
	damageInfo.hitPosition = targetCollider->ClosestPoint(ownCollider->Get_Pivot());
	// The cut and its sparks run along the wave the way the swing that threw it moved: across the flight, toward the
	// wide end (the blade's sweep, as a sword hit's).
	Vector3 sweep = Vector3::TransformNormal(WaveTowardWide, m_Orientation);
	if (sweep.LengthSquared() > 0.000001f)
	{
		sweep.Normalize();
		damageInfo.hitRotation = Quaternion::FromToRotation(Vector3::UnitY, sweep);
	}
	monster->TakeDamage(damageInfo);
	monster->OnAttackHit(shared_from_this());
	if (attacker)
		attacker->OnAttackHit(monster);
}

Shared<SwordWave> SwordWave::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<SwordWave>(device, context);
	return SUCCEEDED(prototype->Initialize_Prototype()) ? prototype : nullptr;
}

Shared<GameObject> SwordWave::Clone(void* arg)
{
	auto instance = make_shared<SwordWave>(*this);
	return SUCCEEDED(instance->Initialize(arg)) ? instance : nullptr;
}
