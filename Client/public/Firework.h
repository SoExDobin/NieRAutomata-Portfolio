#pragma once
#include "GameObject.h"
#include <chrono>

NS_BEGIN(Engine)
	class Shader;
	class VIBuffer_Particle_Point;
NS_END

NS_BEGIN(Client)

/* Scene-placed firework display (the amusement park's sky over the castle): bursts go off about every Interval
   seconds at random points within Area meters of the Transform's position, each Radius meters across give or take.
   The schedule is a function of the clock (burst k's time, place, style and colour hash from k), and the stars
   move in the shader (Shader_VtxParticlePoint Firework pass), so nothing is simulated per frame and the display
   also runs in the Editor. Drawn in the WORLDUI pass after the grade, so the level fog does not swallow the sky
   bursts; the scene depth still hides them behind buildings. Interval, Radius and Area are reflected.
   Launch adds a shell fired from the ground (a park machine's gun salute) to the same sky. */
class CLIENT_DLL Firework final : public GameObject
{
public:
	explicit Firework();
	explicit Firework(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Firework(const Firework& rhs);
	virtual ~Firework() override = default;

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;

public:
	void Priority_Update(Float timeDelta) override {}
	void Update(Float timeDelta) override {}
	void Late_Update(Float timeDelta) override {}
	void Fixed_Update(Float fixedDelta) override {}
	HRESULT Render() override;
	void Submit_RenderGroup() override;

public:
	Float Get_Interval() const { return m_Interval; }
	void Set_Interval(Float interval) { m_Interval = interval; }
	Float Get_Radius() const { return m_Radius; }
	void Set_Radius(Float radius) { m_Radius = radius; }
	Float Get_Area() const { return m_Area; }
	void Set_Area(Float area) { m_Area = area; }

	// A shell fired from muzzle along way: one spark climbs (the Spray pass) and bursts ShellRise seconds later, a
	// small chrysanthemum or ball cluster ShellRadius across. Drawn until it dies; nothing else is kept.
	void Launch(const Vector3& muzzle, const Vector3& way);
	// Indoors the sky is not heard: an interior AreaTrigger (LightOverride) silences the loop while the player is inside.
	// The bursts keep drawing.
	void Set_Muted(Bool muted);

private:
	// Stars per burst: the chrysanthemum's streaks; a ball cluster draws the first BallStars of them.
	static constexpr uint32 MaxStars = 160;
	static constexpr uint32 BallStars = 30;
	static constexpr Float ShellRise = 0.9f;
	static constexpr Float ShellRadius = 7.f;

	struct SHELL
	{
		Vector3 muzzle;
		Vector3 way;
		Float launched;		// seconds since m_Start
		int32 seed;
	};
	HRESULT Render_Shell(const SHELL& shell, Float now);
	HRESULT Render_Burst(const Vector4& burst, const Vector4& shape, const Vector4& tint, uint32 stars);

	Shared<Shader> m_Shader{ nullptr };
	Shared<VIBuffer_Particle_Point> m_Buffer{ nullptr };
	Float m_Interval{ 1.2f };
	Float m_Radius{ 30.f };
	Float m_Area{ 40.f };
	std::chrono::steady_clock::time_point m_Start{};
	vector<SHELL> m_Shells;
	Float m_UnmutedVolume{ -1.f };	// the SoundEmitter's Volume while muted, -1 = not muted

public:
	static Shared<Firework> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;
};

NS_END
