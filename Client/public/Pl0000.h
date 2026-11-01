#pragma once
#include "Entity.h"
#include "Pl0000Input.h"
#include "Pl0000Body.h"
#include "Pl0000MonsterChecker.h"

NS_BEGIN(Engine)
	class Shader;
    class Model;
    class SphereCollider;
}

NS_BEGIN(Client)
	class SheathWP0070Body;
	class SheathWP0220Body;

	class Pl0000MonsterChecker;
	class Pl0000EvadeChecker;

	class Pl0000Movement;
    class Pl0000StateMachine;
    class Pl0000Body;
    class WP0070Body;
    class WP0220Body;
    class Pl0000Parts;
    class SwordWave;
    class WeaponHalo;

class CLIENT_DLL Pl0000 final : public Entity
{
public:
	typedef struct tagPl0000Desc : public ENTITY_DESC {

	} PL0000_DESC;

public:
	explicit Pl0000();
	explicit Pl0000(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Pl0000(const Pl0000& rhs);
	~Pl0000() override = default;

public: /* pl0000 */
    Shared<Pl0000Body> Get_Body() const { return m_MainBody; }
    Shared<GameObject> Get_ClosestTarget() const { return m_MonsterChecker->Get_ClosestTarget(m_Transform->Get_Look(), nullptr, Pl0000MonsterChecker::AutoTargetRange); }
    vector<Shared<GameObject>> Get_MonstersAround(Float radius, Float height) const { return m_MonsterChecker->Get_MonstersAround(radius, height); }
    TRANSFORM_FRAME Get_BodyModelTransform() const { return m_MainBody->Get_ModelTransform(); }
    // The seventh light hit is still playing; the sword it threw stays where it landed till then.
    Bool Is_LightGround7Playing() const;

public:
    void Draw_LightWeapon();
    void Sheathe_LightWeapon();
    void Draw_HeavyWeapon();
    void Sheathe_HeavyWeapon();
	void Launch_FromLightGround6(Float height);

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
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

public:
    void TakeDamage(const DAMAGE_INFO& dmgInfo) override;
    void OnAttackHit(const Shared<GameObject>& target) override;
    Bool TryEvade(const Shared<GameObject>& attacker);

private:
    HRESULT Ready_PartObjects();
    HRESULT Ready_Components();

public:
    void Set_LockOnTarget(const Shared<GameObject>& target) { m_LockOnTarget = target; m_IsManualLock = false; }
    Weak<GameObject> Get_LockOnTarget() const { return m_LockOnTarget; }
    // The player chose the target with G (the camera follows it), not an attack or a dodge.
    Bool Is_ManualLockOn() const { return m_IsManualLock && !m_LockOnTarget.expired(); }

public:
    // Slows the whole game (Game time scale) and restores it after duration of real time.
    void Trigger_GlobalLag(Float timeScale, Float duration);
    void Set_InvincibleTime(Float duration) { m_InvincibleTimer = duration; }
    Bool Is_Invincible_Active() const { return m_IsInvincible || (m_InvincibleTimer > 0.f); }
    // A sword swing's attack window opened: throw a slash wave from it if one is free.
    void Swing_SwordWave(const Shared<Pl0000Parts>& weapon);

public:
    // Inspector: the ledge grab in the air (State2B_Ledge). A wall up to Reach ahead of the feet (m) whose top is between
    // Min and Max height above the feet is climbed; the climb clip rises 1.7 m.
    Float Get_LedgeReach() const { return m_LedgeReach; }
    void Set_LedgeReach(Float reach) { m_LedgeReach = reach; }
    Float Get_LedgeMinHeight() const { return m_LedgeMinHeight; }
    void Set_LedgeMinHeight(Float height) { m_LedgeMinHeight = height; }
    Float Get_LedgeMaxHeight() const { return m_LedgeMaxHeight; }
    void Set_LedgeMaxHeight(Float height) { m_LedgeMaxHeight = height; }

public:
    // The level's music (2D): the player starts it on its first update (in the Editor, once Play runs) and stops it when it
    // goes (the Editor's Stop, a level change). A boss fades it out for its fight and back in once it falls; faded out it
    // keeps running unheard.
    static constexpr SOUNDCHANNEL LevelBgmChannel = SOUNDCHANNEL::CHANNEL_24;
    static constexpr Float LevelBgmFadeOutTime = 2.f;	// as a boss's entrance starts
    static constexpr Float LevelBgmFadeInTime = 4.f;	// once the boss falls

private:
    void Start_LevelBgm();

private:
    Bool m_LevelBgmStarted{ false };
    Float m_LedgeReach{ 0.8f };
    Float m_LedgeMinHeight{ 0.9f };
    Float m_LedgeMaxHeight{ 2.f };

private:
    Weak<GameObject> m_LockOnTarget{};
    Bool m_IsManualLock{ false };

    Float m_GlobalLagTimer{ 0.f };
    Bool m_IsGlobalLagActive{ false };
    Float m_InvincibleTimer{ 0.f };
    Float m_PodLaserCooldown{ 0.f };
    uint32 m_BulletLayerIndex{};
    uint32 m_MonsterPhysicalLayerIndex{};

private:
    Shared<Pl0000Body> m_MainBody{ nullptr };
    Shared<Pl0000MonsterChecker> m_MonsterChecker{ nullptr };

    Shared<WP0070Body> m_LightWeapon{ nullptr };
    Shared<WP0220Body> m_HeavyWeapon{ nullptr };
    Shared<SheathWP0070Body> m_SheathLightWeapon{ nullptr };
    Shared<SheathWP0220Body> m_SheathHeavyWeapon{ nullptr };
    Shared<WeaponHalo> m_LightHalo{ nullptr };
    Shared<WeaponHalo> m_HeavyHalo{ nullptr };
    // Waves already flying keep going while the next swings throw new ones.
    array<Shared<SwordWave>, 3> m_SwordWaves{};

    Shared<Pl0000StateMachine> m_Pl0000States{ nullptr };
    Shared<Pl0000Input> m_Pl0000Input{ nullptr };
    Shared<Pl0000Movement> m_Pl0000Movement{ nullptr };
    Shared<SphereCollider> m_PhysicalZone{ nullptr };

public:
	static Shared<Pl0000> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;

public:
    enum class PL0000_STATE {
        // HEADER
        IDLE                = 899,
        RUN                 = 898,
        SPRINT              = 897,
        EVADE                = 896,
        JUMP                = 895,
        ATTACK_GROUND       = 894,
        ATTACK_AIR          = 893,
        HIT                 = 892,
        POD_GLIDE           = 891,
        POD_ATTACK          = 890,
        LEDGE               = 889,

        // IDLE
        IDLE_Neutral            = 46,
        IDLE_STAND_TO_Neutral   = 39,

        WALK_TO_RUN             = 7,

        // RUN
        RUN_CYCLE           = 2,
        RUN_STOP_L          = 3,
        RUN_STOP_R          = 4,

        RUN_LIGHT_CYCLE     = 590,
        RUN_HEAVY_CYCLE     = 649,
        
        // SPRINT
        SPRINT_CYCLE        = 5,
        SPRINT_STOP_R       = 6,
        
        // DASH
        STAND_TO_DASH_F     = 61, DASH_F              = 62, DASH_TO_STAND_F     = 63,
        STAND_TO_DASH_B     = 64, DASH_B              = 65, DASH_TO_STAND_B     = 66,
        STAND_TO_DASH_R     = 67, DASH_R              = 68, DASH_TO_STAND_R     = 69,
        STAND_TO_DASH_L     = 70, DASH_L              = 71, DASH_TO_STAND_L     = 72,

        DASH_F_TO_SPRINT    = 73, 
    	DASH_B_TO_SPRINT    = 74, 
    	DASH_R_TO_SPRINT    = 75, 
    	DASH_L_TO_SPRINT    = 76,

        // EVADE
        EVADE_FRONT         = 27,
        EVADE_BACKWARD      = 28,
        EVADE_RIGHT         = 29,
        EVADE_LEFT          = 30,
        
        // JUMP
        JUMP_ENTER          = 13,
        JUMP_HOLD           = 14,

        DOUBLE_JUMP1        = 16,
        DOUBLE_JUMP2        = 17,
        DOUBLE_JUMP3        = 18,
        DOUBLE_JUMP4        = 19,

        STAND_TO_JUMP       = 23,
        RUN_TO_JUMP         = 24,
        SPRINT_TO_JUMP      = 25,

        JUMP_TO_RUN         = 10,
        JUMP_TO_SPRINT      = 11,
        JUMP_TO_STAND       = 15,

        // POD GLIDE: frames 0~90 grab the pod, the rest hangs from it
        POD_GRAB            = 26, // pl0000_001f

        // LEDGE: frames 6~24 rise 1.7 m onto the ledge, the rest steps 1.53 m in and stands
        LEDGE_CLIMB         = 97, // pl0000_0080

        // POD LASER: 1200 fires and settles into a still pose, 1202 returns to idle
        POD_LASER_FIRE      = 431, // pl0000_1200
        POD_LASER_RETURN    = 433, // pl0000_1202

        // LIGHT ATTACK 0070
        LIGHT_GROUND1       = 147,
        LIGHT_GROUND2       = 148,
        LIGHT_GROUND3       = 149,
        LIGHT_GROUND4       = 150,
        LIGHT_GROUND5       = 151,
        LIGHT_GROUND6       = 152,
        LIGHT_GROUND7       = 153,

        LIGHT_GROUND_HOLD   = 340, // pl0000_0660
        LIGHT_GROUND_RUN    = 173, // pl0000_0180

        LIGHT_AIR1          = 161,
        LIGHT_AIR2          = 162,
        LIGHT_AIR3          = 161,
        LIGHT_AIR4          = 162,
        LIGHT_AIR5          = 163,

        LIGHT_AIR_DOWN_ENTER    = 175,
        LIGHT_AIR_DOWN_HOLD     = 176,
        LIGHT_AIR_DOWN_END      = 177,

        // HEAVY ATTACK 0220
        HEAVY_GROUND1       = 187,
        HEAVY_GROUND2       = 190,
        HEAVY_GROUND3       = 193,

        HEAVY_GROUND_HOLD_UNFULL       = 196,
        HEAVY_GROUND_HOLD_NO_CONTACT   = 198,
        HEAVY_GROUND_HOLD_CYCLE        = 197,
        HEAVY_GROUND_HOLD_FULL         = 199,
        HEAVY_GROUND_SPRINT            = 200,

        HEAVY_AIR_DOWN_ENTER    = 202,
        HEAVY_AIR_DOWN_HOLD     = 203,
        HEAVY_AIR_DOWN_END      = 204,

        // A2's right click (StateA2_AttackGround/AttackAir): the great sword's light combo 0200~0203 and the spinning rise
        HEAVY_LIGHT_GROUND1     = 183,
        HEAVY_LIGHT_GROUND2     = 184,
        HEAVY_LIGHT_GROUND3     = 185,
        HEAVY_LIGHT_GROUND4     = 186,
        HEAVY_AIR_UP            = 201, // pl0000_0220: frames 36~72 rise 3.2 m, settling at 3 m

        LIGHT_HEAVY_COMBO       = 329,

        // HIT
        HIT1 = 366, HIT2 = 367, HIT3 = 368, HIT4 = 369,
    };
};

NS_END

// --- 기본 이동 및 시스템 모션 ---
// pl0000_0000 Industry standard (pl0000_0000)
// pl0000_0001 Walk Cycle
// pl0000_0002 Run Cycle
// pl0000_0003 Run Stop L
// pl0000_0004 Run Stop R
// pl0000_0006 Sprint Cycle
// pl0000_0007 Sprint Stop R
// pl0000_0008 Walk to Run
// pl0000_0009 Run Stop R
// pl0000_000a Run Stop L
// pl0000_000b Landing Run
// pl0000_000c roll to sprint after jumping
// pl0000_000d Evade Left

// pl0000_0010 Jump 1/3, pl0000_0011 Jump 2/3, pl0000_0012 Jump 3/3
// pl0000_0014 Double Jump
// pl0000_0015 Double Jump (Start)
// pl0000_0016 Double Jump (Land)
// pl0000_0017 Double Jump (Loop/Other)
// pl0000_0018 Jump 1/3, pl0000_0019 Jump 2/3, pl0000_001a Jump 3/3

// pl0000_001b Stand to Jump
// pl0000_001c Run to Jump
// pl0000_001d Sprint to Jump
// pl0000_001f Pod grab

// pl0000_0024 Evade Front
// pl0000_0025 Evade Backward
// pl0000_0026 Evade Right
// pl0000_0027 Evade Left

// --- 아이들 및 상태 전환 ---
// pl0000_0028 Walk Stop, 
// pl0000_0029 Walk Stop, 
// pl0000_002a Walk Stop, 
// pl0000_002b Walk Stop, 
// pl0000_002c Walk Stop, 
// pl0000_002d Walk Stop, 
// pl0000_002e Walk Stop, 
// pl0000_002f Walk Stop

// pl0000_0030 Stand to Neutral Idle
// pl0000_0031 Neutral Idle Cycle
// pl0000_0032 Neutral Idle to Idle Crossed Arms
// pl0000_0033 Idle Crossed Arms Cycle
// pl0000_0034 Neutral Idle to Idle Crossed Arms
// pl0000_003a Stand to Neutral Idle
// pl0000_003b Stand to Neutral Idle
// pl0000_003f Neutral Idle Cycle

// --- 소형검 (Small Sword) 공격 ---
// pl0000_0100 Light Ground 1
// pl0000_0101 Light Ground 2
// pl0000_0102 Light Ground 3
// pl0000_0103 Light Ground 4
// pl0000_0104 Light Ground 5
// pl0000_0105 Light Ground 6
// pl0000_0106 Light Ground 7
// pl0000_0110 Heavy Ground 1
// pl0000_0111 Heavy Ground Hold (No Contact)
// pl0000_0113 Heavy Ground 2
// pl0000_0116 Heavy Ground 3
// pl0000_0119 Heavy Ground Hold (Contact)
// pl0000_0130 Light Air 1 & 3
// pl0000_0131 Light Air 2 & 4
// pl0000_0132 Light Air 5
// pl0000_0150 Heavy Air Up
// pl0000_0155 Heavy Air Down 1/3, pl0000_0156 Heavy Air Down 2/3, pl0000_0157 Heavy Air Down 3/3
// pl0000_0160 Heavy Sprint
// pl0000_0180 Light Run/Sprint
// pl0000_0188 Heavy Run
// pl0000_018d Heavy Air Down Directional 1/3, pl0000_018e Heavy Air Down Directional 2/3, pl0000_018f Heavy Air Down Directional 3/3
// pl0000_01e0 Counter, pl0000_01e1 Counter
// pl0000_01e8 Taunt Small Sword
// pl0000_01f0 Small Sword Sheathe ?

// --- 대형검 (Heavy Sword) 공격 ---
// pl0000_0200 Light Ground 1
// pl0000_0201 Light Ground 2
// pl0000_0202 Light Ground 3
// pl0000_0203 Light Ground 4
// pl0000_0205 Heavy Ground 1
// pl0000_0208 Heavy Ground 1 (Alt)
// pl0000_0209 Heavy Ground 2
// pl0000_020d Heavy Ground 3
// pl0000_0211 Heavy Ground Hold Unfinished
// pl0000_0212 Heavy Ground Hold Cycle
// pl0000_0213 Heavy Ground Hold No Contact
// pl0000_0214 Heavy Ground Hold Contact
// pl0000_021a Heavy Sprint
// pl0000_0220 Heavy Air Up
// pl0000_0225 Heavy Air Down 1/3, pl0000_0226 Heavy Air Down 2/3, pl0000_0227 Heavy Air Down 3/3
// pl0000_022a Light Air 1
// pl0000_0280 Light Run
// pl0000_028d Heavy Air Down Directional 1/3, pl0000_028e Heavy Air Down Directional 2/3, pl0000_028f Heavy Air Down Directional 3/3
// pl0000_02e0 Counter, pl0000_02e1 Counter
// pl0000_02e8 Taunt Heavy Sword
// pl0000_02f0 Heavy Sword Sheathe ?

// --- 대미지 및 특수 모션 ---
// pl0000_1065 Self-Destruct 2B
// pl0000_1067 Self-Destruct A2
// pl0000_1080 Stumble Right
// pl0000_1081 Stumble Left
// pl0000_1082 Stumble Front, pl0000_1083 Stumble Front
// pl0000_1085 Stumble Front to Walk
// pl0000_108a Stumble Front to Stand
// pl0000_aa0a Jump 1/3 ?, pl0000_aa0b Jump Cycle 2/3, pl0000_aa0c Jump Landing 3/3 ?
// pl0000_b010 Jump 1/3, pl0000_b011 Jump Cycle 2/3, pl0000_b012 Jump Landing 3/3
// pl0000_e090 Vomit Cycle
