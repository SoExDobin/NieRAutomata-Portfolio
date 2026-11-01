#pragma once
#include "Em0000.h"

NS_BEGIN(Engine)
class Light;
NS_END

NS_BEGIN(Client)

/* The golden rabbit (emb001, an em0000 variant holding a trumpet), three times a man's height in the scene. It stands as a
   statue in its trumpet pose and only takes hits until their damage adds up to WakeDamage; then it plays the fanfare and
   fights: up close a headbutt, a one-handed punch or a step back, further out the trumpet, blowing a stream of orbs at
   the player. It never staggers, is never launched and is never pushed (static: the player is pushed out of it). */
class CLIENT_DLL Emb001 final : public Em0000
{
public:
	explicit Emb001();
	explicit Emb001(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Emb001(const Emb001& rhs);
	~Emb001() override = default;

public:
	HRESULT Initialize(void* arg) override;
	void On_Destroy() override;
	void Update(Float timeDelta) override;

public:
	EM0000_STATE Choose_Attack(Float distance) const override;
	Bool Is_Dormant() const override { return m_Dormant; }
	EM0000_STATE Get_ChaseClip() const override { return EM0000_STATE::WALK_F; }
	// AttackSpeed while attacking: the rest keeps the slower cadence of its size.
	Float Get_AnimationRate() const override;
	void TakeDamage(const DAMAGE_INFO& dmgInfo) override;
	void OnDeath() override;
	void Launch_Up(Float height) override {}
	void Slam_Down(const Shared<GameObject>& plunger) override {}

public:
	Float Get_MaxHp() const { return m_MaxHp; }
	void Set_MaxHp(Float maxHp) { m_MaxHp = maxHp; m_Hp = maxHp; }
	Float Get_WakeDamage() const { return m_WakeDamage; }
	void Set_WakeDamage(Float wakeDamage) { m_WakeDamage = wakeDamage; }
	Float Get_AttackSpeed() const { return m_AttackSpeed; }
	void Set_AttackSpeed(Float attackSpeed) { m_AttackSpeed = (std::max)(attackSpeed, 0.1f); }

private:
	// One orb from the trumpet's bell every TrumpetInterval while the attack blows it (05b1 in ATTACK), each at a random
	// heading inside a cone of OrbSpread around the line to the player.
	void Blow_Trumpet(Float timeDelta);
	/* The fountain's floor lights turn spots onto the statue the first time the camera sees it up close (within
	   SpotLookDistance and in view): one spot from each of FountainLights, aimed at its chest. They stay on until it dies. */
	void Light_FountainSpots();
	void Clear_FountainSpots();

private:
	Bool m_Dormant{ true };
	Float m_WakeDamage{ 300.f };
	Float m_DormantDamage{};	// taken while a statue
	// The attack clips' playback rate. sqrt(BodyScale) undoes the size cadence, so they play at the original's own
	// speed (emb001.dat sets no motion speed of its own).
	Float m_AttackSpeed{ 1.732f };
	int32 m_TrumpetBoneIndex{ -1 };
	Float m_TrumpetTimer{};
	uint32 m_TrumpetVolley{};
	vector<Shared<Light>> m_FountainSpots;
	Bool m_FountainSpotsLit{ false };

	static constexpr Float BodyScale = 3.f;		// the reference video's rabbit stands about three times A2's height
	// The bell in bone307 space: the far rim of the vertices skinned to it, 0.64 m out at the body's own size.
	static inline const Vector3 TrumpetBell{ -0.07f, -0.04f, -0.63f };
	static constexpr Float TrumpetInterval = 0.1f;	// 30 orbs over the 3 s blow
	static constexpr Float OrbSpread = 15.f;		// the cone's half angle (degrees)
	static constexpr Float OrbSpeed = 12.f;
	static constexpr Float OrbRange = 40.f;
	static constexpr Float OrbDamage = 15.f;
	static constexpr Float SpotLookDistance = 25.f;
	// The six light fixtures on the basin floor around the rabbit's pedestal (part of Level 4's world tile, not scene objects).
	static inline const Vector3 FountainLights[]{ { 776.219f, 0.049f, 225.028f }, { 780.625f, 0.049f, 218.314f },
		{ 788.080f, 0.049f, 218.425f }, { 791.772f, 0.049f, 225.163f }, { 787.938f, 0.049f, 231.849f },
		{ 780.017f, 0.049f, 232.015f } };
	static constexpr Float SpotAimHeight = 3.f;		// the chest of a body three times A2's height
	static inline const Color SpotColor{ 1.f, .83f, .58f, 1.f };	// warm white, the lamps' tone at about 40% (closer up, 2.4 was glaring)
	// From 8 m out and below, the cones (half angles, degrees) take in the body from the pedestal up, and the range
	// ends a little past the rabbit so the beams stop above it.
	static constexpr Float SpotInner = 10.f;
	static constexpr Float SpotOuter = 20.f;
	static constexpr Float SpotRange = 18.f;
	static constexpr Float SpotBeam = 0.02f;		// beam scattering (1/m), as em3000's stage spots

public:
	static Shared<Emb001> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;
};

NS_END
