#pragma once
#include "Projectile.h"
#include "Entity.h"

NS_BEGIN(Engine)
class SphereCollider;
class Model;
class Shader;
class Texture;
NS_END

NS_BEGIN(Client)

class Entity;

class CLIENT_DLL Bullet final : public Projectile
{
public:
    // STRAIGHT flies along direction; WAVE (Em3000 danmak 1) also bobs and rises slowly;
    // ARC (Em3000 howitzer) is lobbed under gravity and bursts when it comes down to groundY;
    // RISE (Em3000 rising barrage) climbs ever faster as it flies out.
    enum class FLIGHT : uint8 { STRAIGHT, WAVE, ARC, RISE };

    typedef struct tagBulletDesc : public PROJECTILE_DESC
    {
        FLIGHT flight{ FLIGHT::STRAIGHT };
        Float gravity{};
        Float groundY{};
        Entity::DAMAGE_INFO damageInfo{};
        wstring resourceTag{};
        wstring targetLayer{};
        Bool  isPermanent{ false };
        Float maxDistance{};       
        Float damage{};           
        wstring burstSound{};       // ARC: played where it bursts (empty: silent)
    } BULLET_DESC;

public:
    explicit Bullet();
    explicit Bullet(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
    explicit Bullet(const Bullet& rhs);
    ~Bullet() override = default;

public:
    Bool Is_Permanent() const { return m_Desc.isPermanent; }
    wstring Get_TargetLayerName() const { return m_Desc.targetLayer; }
    // The bosses' volleys (Em3000, Em8000; on request, 2026-10-22): `count` copies of `desc` turned about the up axis
    // evenly over `spread` radians, centred on desc.direction; a whole turn (XM_2PI) is a ring with desc.direction first
    // and no doubled shot. Each sets off `reach` out from desc.initialPosition along its own way.
    static void Fire_Spread(const BULLET_DESC& desc, uint32 count, Float spread, Float reach, uint32 levelIndex);

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void Update(Float timeDelta) override;
    void Late_Update(Float timeDelta) override;
    HRESULT Render() override;
    void Submit_RenderGroup() override;
    void On_Disable() override;
    void On_Destroy() override;

    // 충돌 처리 (상쇄 로직 핵심)
    void OnCollisionEnter(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;

private:
    HRESULT Ready_Components();
    HRESULT Bind_ShaderResources();
    // Glances off `surface` (Em3000's skirt): bounces away harmless with a ping and a spark that bends the screen around
    // it, and flies on to the end of its range.
    void Ricochet(const Shared<Collider>& surface);

private:
    BULLET_DESC                 m_Desc{};
    Entity::DAMAGE_INFO	        m_DamageInfo{};
    Float                       m_TravelDistance{};
    Float                       m_Age{};                // seconds since fired, from a random start; drives the patterns
    Float                       m_FlightTime{};
    Vector3                     m_Velocity{};
    Bool                        m_IsSpent{ false };     // it has hit or glanced off something and hits nothing more

    Shared<SphereCollider>      m_Collider{ nullptr };
    Shared<Model>               m_Model{ nullptr };
    Shared<Shader>              m_Shader{ nullptr };
    Shared<Texture>             m_Texture{ nullptr };

private:
    /* Bullets are drawn in batches, one instanced draw per kind (Model::Set_Instances): the bullets gather their world
       matrix and age (params.x) in Submit_RenderGroup, once a frame, and one leader (the first bullet to submit after the
       last draw) is the only one in RENDERGROUP::WORLDUI and draws every batch in each View's Render. Owned by the class,
       for the level: the batches are refilled every frame and the leader is dropped when it is disabled or destroyed. */
    enum class BATCH_KIND : uint8 { POD, SOLID, BALL, LIGHTNING };
    struct BATCH_KEY
    {
        wstring resourceTag;
        BATCH_KIND kind;
        uint32 image;           // LIGHTNING: the Effect_Bullet image (5..10)
        auto operator<=>(const BATCH_KEY&) const = default;
    };
    struct BATCH
    {
        Weak<Model> model;      // the model of the batch's first bullet this frame (any bullet of that resourceTag)
        vector<INSTANCE_DATA> instances;
    };
    static map<BATCH_KEY, BATCH> s_Batches;
    static Weak<GameObject> s_Leader;
    static Bool s_Drawn;        // the leader drew since the last submit: the next submit starts a new frame
    void Add_ToBatch(BATCH_KIND kind, uint32 image, const INSTANCE_DATA& instance);

public:
    static Shared<Bullet> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
    Shared<GameObject> Clone(void* arg = nullptr) override;
};
NS_END
