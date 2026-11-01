#pragma once
#include "Em3000Parts.h"
#include "Bullet.h"
#include "Entity.h"

NS_BEGIN(Engine)
class SphereCollider;
class SoundEmitter;
NS_END

NS_BEGIN(Client)

class MonsterAOE;

class CLIENT_DLL Em3000Body final : public PartObject
{
public:
	typedef struct tagEm3000BodyDesc : public PARTOBJECT_DESC
	{

	} EM3000BODY_DESC;

public:
	explicit Em3000Body();
	explicit Em3000Body(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Em3000Body(const Em3000Body& rhs);
	~Em3000Body() override = default;

public:
	const TRANSFORM_FRAME& Get_ModelTransform() const;
	Shared<Model> Get_ModelComponent() { return m_Model; }
	virtual void Set_Animation(uint32 animIndex, Float blendDuration, Bool isLoop);
	uint32 Get_CurrentAnimationIndex() const;
	uint32 Get_NextAnimationIndex() const;
	Float Get_AnimationProgress() const;
	Bool Is_AnimationFinished() const;

public:
	// Danmak: PortShots bullets from each gun port every BarrageInterval until End_Barrage. Every fourth volley can't be cut.
	// STRAIGHT fires each port's pair crossed: from either side of the port toward each other, so the streams draw an X.
	void Begin_Barrage(Bullet::FLIGHT flight);
	void End_Barrage();
	// Danmak 3 (05c1 notifies): PortShots shells from each gun port, lobbed to the floor the boss stands on.
	void Fire_Howitzer(Float speed);
	// Danmak 2 homing lasers from the chest (StateEm3000_Range): one shot every HomingInterval, sweeping across a fan over
	// the front, until HomingCount are out.
	void Begin_Homing();
	// 0560 notify: seven rings out of the head one after another, the first landing just outside the skirt and the last
	// at the dome's wall.
	void Burst_Rings() const;
	// 1500 landing notify: one ring spreading over the floor from the boss.
	void Spread_StompRing() const;
	// The skirt's hit volume (Em3000CenterAoe). The 1500 landing switches it on grown to StompLandRadius; switched off,
	// it is back to its own size.
	void Set_BodyAttack(Bool active, Float radius = 0.f);
	// A sound held where the boss is while a looping clip runs, from its first lap to Stop_LoopSound (the blade mixer's
	// whirr, the dizzy chirp). A notify on the clip started it again every lap and piled up copies that went on long after
	// the clip: the whirr is 26 s on a 1.25 s lap, the chirp 6.6 s on a 0.37 s one.
	void Play_LoopSound(const wstring& soundTag);
	void Stop_LoopSound();
	// Stops everything an interrupted attack left running (phase change, death).
	void End_Attacks();
	// Death (StateEm3000_Dead): a blast with the common death boom on a random bone, or on the chest.
	void Burst(Float size, Bool atChest) const;
	// Her voice (em3000_vo_*, and the howl): VoiceVolume times the emitter's level, at full strength out to VoiceNear.
	void Play_Voice(const wstring& soundTag) const;
	// A hurt cry (em3000_vo_l_dmg) when a blow lands, at most once every HurtVoiceInterval.
	void Play_HurtVoice();
	Vector3 Get_ChestPosition() const;
	// The last blast: the pose freezes and the boss breaks into its bones' parts, which land on the floor 1 m below
	// center and dissolve (BoneShatter). The boss object is never removed.
	void Begin_Shatter(const Vector3& center);
	// Death: Pod shots and blades pass through from now on.
	void Off_Colliders() const;
	// Clip playback speed (1: as authored), e.g. the original seq's SpeedTrack; root motion follows it.
	void Set_AnimationRate(Float rate) { m_AnimationRate = rate; }

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	HRESULT Begin() override;
	void On_Destroy() override;

public:
	void Priority_Update(Float timeDelta) override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	void Fixed_Update(Float fixedDelta) override;
	HRESULT Render() override;
	void Submit_RenderGroup() override;

public:
	void OnCollisionEnter(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;
	void OnCollisionStay(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;
	void OnCollisionExit(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;

private:
	HRESULT Bind_ShaderResources();
	HRESULT Ready_Components();
	HRESULT Ready_AnimationNotify();
	void Fire_Barrage(Float timeDelta);
	void Fire_Homing(Float timeDelta);
	Entity::DAMAGE_INFO Make_BulletDamage(Float damage) const;
	// Gun port `index` in the world as the model stands now, and the way it faces (level, out from the drum).
	void Get_Muzzle(uint32 index, Vector3& position, Vector3& direction) const;

private:
	int32					m_RootBoneIndex{ -1 };
	int32					m_ChestBoneIndex{ -1 };
	int32					m_MuzzleBoneIndex{ -1 };
	Shared<Shader>			m_Shader{ nullptr };
	Shared<Model>			m_Model{ nullptr };
	Shared<SphereCollider>	m_HitBox{ nullptr };
	Shared<SoundEmitter>	m_SoundEmitter{ nullptr };
	Weak<MonsterAOE>		m_BodyAttack{};
	Float					m_BodyAttackRadius{};	// the skirt sphere's own radius while a landing has it grown
	uint32					m_LoopSound{};
	uint32					m_LoopLaps{};	// MIXING laps since Play_LoopSound, counted by its laugh notify
	Float					m_HurtVoiceCooldown{};
	BoneShatter				m_Shatter{};
	Float					m_AnimationRate{ 1.f };

	Bool					m_IsBarrage{ false };
	Bullet::FLIGHT			m_BarrageFlight{};
	Float					m_BarrageTimer{};
	uint32					m_BarrageVolley{};
	uint32					m_HomingLeft{};
	Float					m_HomingTimer{};

	// The gun ports are the thirteen round holes in the panels around the base drum (meshes HD_00..HD_12), skinned to
	// bone773 alone. These are their centres in bone773's space, read from em3000.model (bind vertex x offset matrix),
	// so a port turns and moves with the drum exactly as the mesh is drawn.
	static constexpr uint32 NumMuzzles = 13;
	static inline const Vector3 MuzzleOffsets[NumMuzzles] = {
		{ 0.200f, 0.168f, -2.452f }, { 1.296f, 0.168f, -2.086f }, { 2.103f, 0.168f, -1.242f }, { 2.419f, 0.168f, -0.123f },
		{ 2.176f, 0.168f, 1.010f }, { 1.434f, 0.168f, 1.899f }, { 0.360f, 0.168f, 2.340f }, { -0.787f, 0.168f, 2.231f },
		{ -1.748f, 0.168f, 1.608f }, { -2.316f, 0.168f, 0.602f }, { -2.356f, 0.168f, -0.532f }, { -1.860f, 0.168f, -1.575f },
		{ -0.940f, 0.168f, -2.266f },
	};
	// Slow balls as in the reference video, flying out to the dome's wall (about 25 m).
	static constexpr Float BarrageInterval = 0.2f;
	static constexpr Float BarrageSpeed = 4.f;
	static constexpr Float BarrageRange = 25.f;
	// Each port fires a pair (on request, 2026-10-22, twice the balls): half a port's gap apart, so the volley is even
	// all round.
	static constexpr uint32 PortShots = 2;
	static constexpr Float PortSpread = XM_2PI / NumMuzzles / 2.f;
	// STRAIGHT's crossed pair (on request, 2026-10-24): set off this far either side of the port, crossing this far out.
	static constexpr Float CrossHalfWidth = 0.9f;
	static constexpr Float CrossDistance = 5.f;
	static constexpr uint32 HomingCount = 8;
	static constexpr Float HomingInterval = 0.2f;
	static constexpr Float HomingSpread = 50.f;		// degrees across the front
	static constexpr uint32 HowitzerBurstSoundEvery = 8;	// both shells of every 8th port sound their burst (4 a volley)
	static constexpr Float HurtVoiceInterval = 1.5f;	// a blade combo lands several hits a second
	// The voices are recorded about 5 dB under the attacks' effects and were lost under them and the BGM at the
	// emitter's level, and the arena keeps the player well past the emitter's 10 m. Tuned by ear.
	static constexpr Float VoiceVolume = 3.f;
	static constexpr Float VoiceNear = 25.f;
	static constexpr uint32 MixerLaughEvery = 3;		// MIXING laps (76 frames each) between laughs over the 10 s spin
	static constexpr Float StompLandRadius = 6.5f;		// the 1500 landing's disc (seq AttackTrack, see the notify)

public:
	static Shared<Em3000Body> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;
	
};

NS_END
