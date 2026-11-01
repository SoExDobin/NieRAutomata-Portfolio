#pragma once
#include "Component.h"

NS_BEGIN(Engine)
class Model;

/* 치마·머리처럼 본 사슬을 Jolt 소프트바디로 흔든다. 모델 폴더의 <모델 태그>_softbody.json(NieR 천 clp·clh·clw를 옮긴 것)이
   사슬 마디와 몸 콜라이더를 정한다. 소유 파트가 매 프레임 애니메이션 포즈를 낸 뒤 Simulate를 부르면 결과가 그 본들의
   결합 행렬로 들어간다. 사슬 첫 마디는 애니메이션에 붙어 있고, 아래 마디는 마디마다 rotLimit 원뿔이 허락하는 거리 안에서 움직인다.
   - Simulation: 끄면 클립 포즈 그대로다.
   - Damping·Gravity·SwingLimit·Follow: 데이터의 감쇠·중력·흔들림 한계·원래 자세 복원에 곱하는 배율.
   - Wind: json winds(NieR clw)의 바람 세기에 곱하는 배율. 0이면 바람이 없다. */
class ENGINE_DLL SoftBody final : public Component
{
public:
	explicit SoftBody();
	explicit SoftBody(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit SoftBody(const SoftBody& prototype);
	~SoftBody() override;

public:
	COMPONENT_TYPE Get_ComponentType() const override { return COMPONENT_TYPE::SOFTBODY; }
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	void On_Destroy() override;

public:
	/// 애니메이션 포즈와 소유 파트의 월드 행렬이 정해진 뒤 부른다. 처음 부를 때 천을 만들고, 데이터가 없으면 아무것도 하지 않는다.
	/// 다른 모델에 매달린 파트(em3000 머리의 사슬·줄)는 몸 콜라이더가 그 모델의 뼈에 있다(json colliderModel).
	/// 그때 colliderModel과 그 월드 행렬을 넘기고, 넘기지 않으면 콜라이더도 model에서 찾는다.
	void Simulate(const Shared<Model>& model, const Matrix& worldMatrix, Float timeDelta,
		const Shared<Model>& colliderModel = nullptr, const Matrix& colliderWorldMatrix = Matrix::Identity);

	Bool Get_Simulation() const { return m_Simulation; }
	void Set_Simulation(Bool simulation);
	Float Get_Damping() const { return m_Damping; }
	void Set_Damping(Float damping) { m_Damping = damping; }
	Float Get_Gravity() const { return m_Gravity; }
	void Set_Gravity(Float gravity) { m_Gravity = gravity; }
	Float Get_SwingLimit() const { return m_SwingLimit; }
	void Set_SwingLimit(Float swingLimit) { m_SwingLimit = swingLimit; }
	Float Get_Follow() const { return m_Follow; }
	void Set_Follow(Float follow) { m_Follow = follow; }
	Float Get_Wind() const { return m_Wind; }
	void Set_Wind(Float wind) { m_Wind = wind; }

private:
	// 몸 콜라이더. capsule이면 bone[0]+offset[0]에서 두 점 사이 weight 지점까지, 아니면 그 지점의 구다.
	struct COLLIDER
	{
		uint32  handle{};
		uint32  bones[2]{};
		Vector3 offsets[2]{};
		Float   weight{};
		Bool    capsule{};
	};
	// clw 한 줄. 값은 모델 공간, 1/60초 단위다. 돌풍은 timer 프레임 동안 일고 0~delayMax 프레임 쉰다(timer 0이면 늘 분다).
	// 세기는 swingRate만큼 swingSpeed로, 방향은 Y축으로 rotMax만큼 rotSpeed로 사인파를 그린다.
	struct WIND
	{
		Vector3 from{};
		Vector3 to{};
		Vector3 direction{};	// json direction이 없으면 from → to
		Float   radius{};		// 0이면 모든 마디(type 1). 아니면 from-to 선분 둘레(type 0)
		Float   power{};
		Float   timer{};
		Float   swingRate{};
		Float   swingSpeed{};
		Float   delayMax{};
		Float   rotMax{};
		Float   rotSpeed{};
		Float   phase{};
		Float   gustTime{};
		Float   gustPause{};
	};
	// clp 하나. 관절 j = 마디 j이고 정점 j도 그 마디다. 끝 마디 뒤에는 offset 자리의 끝점 정점이 붙는다.
	struct CHAIN_SET
	{
		uint32 handle{};
		vector<uint32>  bones;			// 관절마다 모델 본
		vector<int32>   childVertex;	// 관절마다 방향을 정하는 정점(아래 마디나 끝점)
		vector<uint32>  vertexJoints;
		vector<Vector3> vertexOffsets;
		vector<COLLIDER> colliders;
		vector<WIND>     winds;
	};

	HRESULT Build(const Shared<Model>& model, const Matrix& worldMatrix, const Shared<Model>& colliderModel,
		const Matrix& colliderWorldMatrix);
	HRESULT Build_Set(const Shared<Model>& model, const Matrix& worldMatrix, const Shared<Model>& colliderModel,
		const Matrix& colliderWorldMatrix, const nlohmann::json& setJson);
	void Get_ColliderSegment(const Shared<Model>& model, const Matrix& worldMatrix, const COLLIDER& collider,
		_Out_ Vector3& outFrom, _Out_ Vector3& outTo) const;
	void Update_Winds(CHAIN_SET& chain, const Matrix& worldMatrix, Float frames);
	void Release();

private:
	Bool  m_Simulation{ true };
	Float m_Damping{ 1.f };
	Float m_Gravity{ 1.f };
	Float m_SwingLimit{ 1.f };
	Float m_Follow{ 1.f };
	Float m_Wind{ 1.f };

	Bool m_Built{ false };
	vector<CHAIN_SET> m_Sets;
	Vector3 m_LastPosition{};
	Float   m_WindClock{};		// 만든 뒤 흐른 프레임. 바람 사인파의 시각
	// 매 프레임 다시 쓰는 버퍼
	vector<Matrix>  m_Joints;
	vector<Vector3> m_Positions;
	vector<uint32>  m_OverrideBones;
	vector<Matrix>  m_OverrideMatrices;
	vector<WIND_VOLUME> m_Winds;

public:
	Shared<Component> Clone(void* arg = nullptr) override;
	static Shared<SoftBody> CreatePrototype();
	static Shared<SoftBody> Create(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context);
};

NS_END
