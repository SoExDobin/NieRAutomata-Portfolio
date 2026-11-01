#pragma once
#include "ParticleEffect.h"

NS_BEGIN(Engine)
class Model;
class PartObject;
class Shader;
class Texture;
class VIBuffer_Particle_Point;
NS_END

NS_BEGIN(Client)

// A machine's eyes and its tell before it attacks, shared by every machine body (Em0000, Em0010, Em8000).
// The eyes glow red while the machine is awake: Render_Eyes draws the body's own skinned mesh again with the EM_Eye pass
// into RT_EMISSIVE (RENDERGROUP::EMISSIVE), lit where the material's g_EmissiveMap marks the eyes, so the glow sits on
// the eyes as they move and the bloom spreads it. The tell (attack clips call Play from their first frame, so the blow
// can be read and dodged): the warning sound (Em_Tele), the eyes flaring for TellTime (Eye_Color with the tell's age),
// and this object for the same time: a thin horizontal streak through the eyes as wide as the screen, as in the
// reference (youtube Kq656K8XVsM 1:34). The streak is coreeff 001/7 (Eye_Flash) on the eye bone (bone16 on the em
// rigs), or eyeReach on past it out from the head's centre (bone3) where bone16 sits inside the head, read when it
// renders so it follows the head that frame, pulled toward the camera so the head doesn't hide it.
class CLIENT_DLL EmTelegraph final : public ParticleEffect
{
public:
	typedef struct tagEmTelegraphDesc : public PARTICLE_EFFECT_DESC
	{
		Weak<PartObject> body{};
		Weak<Model> model{};
		int32 eyeBone{ -1 };
		int32 headBone{ -1 };
		Float eyeReach{};		// model units from bone16 on out of bone3 to the eyes' surface
		Float size{ 1.f };		// the body's scale: a bigger machine lifts the streak further toward the camera
	} EM_TELEGRAPH_DESC;

public:
	explicit EmTelegraph() = default;
	explicit EmTelegraph(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit EmTelegraph(const EmTelegraph& rhs);
	~EmTelegraph() override = default;

public:
	// Plays the warning (soundTag; Em_Tele picks one of the common two) and the streak through the eyes of `body` (its
	// model's bone16, eyeReach further out from bone3). The body starts its own tell age.
	static void Play(const Shared<PartObject>& body, const Shared<Model>& model, Float size = 1.f,
		const wstring& soundTag = L"Em_Tele", Float eyeReach = 0.f);
	// The eyes' HDR colour: steady while awake, flaring for TellTime from a Play (tellAge < 0: no tell running).
	static Vector4 Eye_Color(Float tellAge);
	// Draws `model` with EM_Eye into the EMISSIVE group's target. The body bound its world, view and projection.
	static HRESULT Render_Eyes(const Shared<Model>& model, const Shared<Shader>& shader, Float tellAge);
	// Advances a body's tell age by timeDelta; back to -1 once the tell is over.
	static void Advance_Tell(Float& tellAge, Float timeDelta);

	static constexpr Float TellTime = 0.35f;

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	void Update(Float timeDelta) override;
	void Submit_RenderGroup() override;
	HRESULT Render() override;

private:
	HRESULT Ready_Components();

private:
	Shared<Shader> m_Shader{ nullptr };
	Shared<Texture> m_Texture{ nullptr };
	Shared<VIBuffer_Particle_Point> m_Buffer{ nullptr };
	EM_TELEGRAPH_DESC m_Desc{};
	Float m_Age{};

	static constexpr Float StreakWidth = 4.4f;		// screen widths, centred on the eyes, so it crosses the screen from any spot
	static constexpr Float StreakHeight = 0.05f;	// screen heights; the streak frame fades out well inside it
	static constexpr Float CameraLift = 0.6f;		// m at the body's own size, clear of the face in front of the eyes
	static inline const Vector4 StreakTint{ 4.f, 0.3f, 0.18f, 2.f };
	static inline const Vector4 EyeGlow{ 3.f, 0.3f, 0.15f, 1.f };	// steady red, hotter than the screen holds where the mask is full
	static constexpr Float EyeFlare = 3.f;			// the eyes' extra brightness at the tell's peak, times EyeGlow

public:
	static Shared<EmTelegraph> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;
};

NS_END
