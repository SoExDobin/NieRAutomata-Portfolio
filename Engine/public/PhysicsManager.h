#pragma once
#include "EngineManager.h"

/// PhysX 타입은 이 헤더로 새어나오지 않는다. Client/Editor는 Game 파사드만 쓰고
/// SDK 헤더를 직접 include 하지 않는다.
namespace physx
{
	class PxFoundation;
	class PxPhysics;
	class PxDefaultCpuDispatcher;
	class PxScene;
	class PxMaterial;
	class PxControllerManager;
	class PxTriangleMesh;
	class PxRigidStatic;
	class PxRigidDynamic;
	class PxController;
}

/// 소프트바디(천·머리 같은 본 사슬)는 Jolt로 돌린다. Jolt 타입도 이 헤더 밖으로 나가지 않는다.
namespace JPH
{
	class PhysicsSystem;
	class TempAllocator;
	class Body;
}

NS_BEGIN(Engine)

/// 캐릭터 컨트롤러 생성 인자. PhysX 타입 없이 Client가 채운다.
/// PhysX 캡슐의 전체 높이는 height + radius * 2 이다.
struct CHARACTER_CONTROLLER_DESC
{
	Float radius{ 0.35f };
	Float height{ 1.0f };
	Float slopeLimitDegrees{ 45.f };
	Float stepOffset{ 0.4f };
	Float contactOffset{ 0.1f };
};

/// Move 결과. 여러 비트가 동시에 설 수 있다.
enum CONTROLLER_COLLISION : uint32
{
	CONTROLLER_COLLISION_SIDES = 1u << 0,
	CONTROLLER_COLLISION_UP    = 1u << 1,
	CONTROLLER_COLLISION_DOWN  = 1u << 2,
};

/// 게임플레이 겹침 판정용 도형. 월드 좌표이며 AABB와 구는 회전을 쓰지 않는다.
struct QUERY_SHAPE_DESC
{
	COLLIDER_TYPE type{ COLLIDER_TYPE::SPHERE };
	Vector3 center{};
	Quaternion orientation{};
	Vector3 extents{};   // 상자의 반크기
	Float radius{};
};

/// 소프트바디(본 사슬) 생성 인자. 정점 하나가 관절 하나에 붙고, 관절은 Create_SoftBody·Simulate_SoftBody에 주는 월드 행렬 배열의 인덱스다.
struct SOFTBODY_DESC
{
	struct VERTEX
	{
		uint32  joint{};
		Vector3 offset{};						// 관절 공간 자리
		Float   invMass{ 1.f };					// 0이면 관절을 그대로 따라간다
		Float   maxDistance{ FLT_MAX };			// 애니메이션 자리에서 벗어날 수 있는 거리
		Float   followRate{};					// 1/60초마다 애니메이션 자리로 되돌아가는 비율
	};
	struct EDGE
	{
		uint32 vertex[2]{};
		Float  compliance{};					// 0이면 늘어나지 않는다
	};
	vector<VERTEX> vertices;
	vector<EDGE>   edges;
	Float  vertexRadius{};
	Float  linearDamping{ 0.1f };				// dv/dt = -linearDamping * v
	Float  gravityFactor{ 1.f };
	uint32 iterations{ 4 };
};

/// Inspector 배율. 생성 인자의 감쇠·중력·최대 거리·복원 비율에 곱한다.
struct SOFTBODY_TUNING
{
	Float damping{ 1.f };
	Float gravity{ 1.f };
	Float swing{ 1.f };
	Float follow{ 1.f };
};

/// 이번 프레임 바람. radius가 0보다 크면 from-to 선분에서 그 거리 안의 정점만, 아니면 모든 정점이 받는다.
struct WIND_VOLUME
{
	Vector3 from{};
	Vector3 to{};
	Float   radius{};
	Vector3 acceleration{};		// 월드 m/s²
	// from에서 정점까지 거리를 따라 세기가 물결친다(1 + sin(waveNumber·거리 − wavePhase), 0~2배). 0이면 고르다.
	Float   waveNumber{};		// 라디안/m
	Float   wavePhase{};		// 라디안
};

/// 월드 충돌과 캐릭터 이동을 소유한다. 현재 단계는 정적 씬만 만들고 고정 스텝에서 돌린다.
class PhysicsManager final : public EngineManager
{
	NO_COPY(PhysicsManager)
public:
	explicit PhysicsManager();
	~PhysicsManager() override;

public:
	HRESULT Initialize(void* arg) override;
	void On_Destroy() override;
	void On_Disable() override;
	void On_Enable() override;
	void Set_Active(Bool isActive) override;

public:
	/// 고정 스텝 1회. 씬이 없거나 비활성이면 아무것도 하지 않는다.
	void Step(Float fixedDelta);

	Bool Is_Ready() const { return nullptr != m_Scene; }

public:
	/// 모델 태그의 지오메트리를 구워 캐시에 넣는다. 이미 있으면 그대로 쓴다.
	Bool Ensure_StaticMesh(const wstring& modelTag);

	/// 구운 메시를 월드 행렬 자리에 정적 액터로 올린다. 0이면 실패.
	uint32 Add_StaticActor(const wstring& modelTag, const Matrix& worldMatrix);
	void   Remove_StaticActor(uint32 handle);

	/// 정적 월드 충돌(COL)만 맞힌다. 캐릭터 캡슐과 게임플레이 볼륨은 보지 않는다. 지면 확인·카메라 충돌용.
	Bool Raycast(const Vector3& origin, const Vector3& direction, Float distance,
		_Out_ Vector3& outHit, _Out_opt_ Vector3* outNormal = nullptr) const;

	/// 정적 충돌이 하나라도 올라와 있는지. 없으면 캐릭터 컨트롤러를 써도 설 곳이 없다.
	Bool Has_StaticCollision() const { return !m_StaticActors.empty(); }
	/// 정적 충돌 액터 전체의 월드 경계. 없으면 false.
	Bool Get_StaticBounds(_Out_ BoundingBox& outBounds) const;

public:
	/// 발 위치에 캡슐 컨트롤러를 만든다. 0이면 실패.
	uint32  Create_Controller(const CHARACTER_CONTROLLER_DESC& desc, const Vector3& footPosition);
	/// 변위만큼 쓸어서 옮기고 CONTROLLER_COLLISION 비트를 돌려준다.
	uint32  Move_Controller(uint32 handle, const Vector3& displacement, Float timeDelta);
	void    Set_ControllerFootPosition(uint32 handle, const Vector3& footPosition);
	Vector3 Get_ControllerFootPosition(uint32 handle) const;
	void    Release_Controller(uint32 handle);

public:
	/* 게임플레이 콜라이더(공격·시야·회피 볼륨)의 쿼리 전용 도형. 시뮬레이션에는 끼지 않고
	   overlap으로만 찾는다. 지면 레이와 캐릭터 컨트롤러는 이 도형을 보지 않는다. */
	/// layerBit·maskBits는 LayerMask 규칙 그대로다. 두 도형이 서로의 마스크에 들 때만 겹침으로 본다.
	uint32 Add_QueryShape(const QUERY_SHAPE_DESC& desc, uint32 layerBit, uint32 maskBits);
	/// 바뀐 것만 PhysX에 넘긴다. 가만히 있는 볼륨은 쿼리 트리를 건드리지 않는다.
	void   Update_QueryShape(uint32 handle, const QUERY_SHAPE_DESC& desc, uint32 layerBit, uint32 maskBits);
	void   Remove_QueryShape(uint32 handle);
	/// handle 도형과 겹치고 레이어 필터를 양방향으로 통과하는 다른 쿼리 도형의 핸들을 모은다.
	void   Overlap_QueryShape(uint32 handle, _Out_ vector<uint32>& outHandles) const;

public:
	/* 소프트바디. 물리 씬 스텝에 넣지 않고, 소유 오브젝트가 이번 프레임 포즈를 낸 직후 Simulate_SoftBody로 바로 돌린다.
	   그래서 몸보다 한 프레임 늦지 않고 프레임레이트와도 어긋나지 않는다. 부딪히는 것은 소프트바디 콜라이더뿐이다. */
	/// jointMatrices가 정점의 기준 자세다. 0이면 실패.
	uint32 Create_SoftBody(const SOFTBODY_DESC& desc, const vector<Matrix>& jointMatrices);
	/// 관절을 이번 포즈로 옮기고 timeDelta만큼 돌린다. reset이면 모든 정점을 애니메이션 자리에 멈춰 둔다(첫 프레임, 순간이동).
	void   Simulate_SoftBody(uint32 handle, const vector<Matrix>& jointMatrices, Float timeDelta, Bool reset, const SOFTBODY_TUNING& tuning,
		const vector<WIND_VOLUME>& winds);
	/// 정점의 월드 위치를 SOFTBODY_DESC::vertices 순서로 채운다.
	void   Get_SoftBodyPositions(uint32 handle, _Out_ vector<Vector3>& outPositions) const;
	void   Release_SoftBody(uint32 handle);

	/// 소프트바디 softBody가 부딪히는 몸 도형. 다른 소프트바디는 이 도형을 지나간다(천이면 clp마다 짝인 clh만 쓴다).
	/// halfHeight가 0이면 구, 아니면 캡슐이다. 0이면 실패.
	uint32 Add_SoftBodyCollider(uint32 softBody, Float radius, Float halfHeight);
	/// 캡슐 축이 from→to를 잇게(구는 from에) 옮긴다.
	void   Move_SoftBodyCollider(uint32 handle, const Vector3& from, const Vector3& to);
	void   Remove_SoftBodyCollider(uint32 handle);

#ifdef _DEBUG
public:
	/// F2: 정적 충돌 삼각형을 면 방향으로 칠한다(바닥 파랑, 벽 주황, 천장 보라, 기준은 컨트롤러 경사 한계).
	/// 지금 View에 든 칸만 그린다. 장면을 그린 뒤 그 깊이가 묶인 채로 부른다.
	void Render_StaticDebug();

private:
	struct STATIC_DEBUG_CELL
	{
		BoundingBox bounds{};
		uint32 startVertex{};
		uint32 vertexCount{};
	};
	struct STATIC_DEBUG_MESH
	{
		ComPtr<ID3D11Buffer> vertexBuffer{ nullptr };
		vector<STATIC_DEBUG_CELL> cells;
	};
	void Build_StaticDebugMesh(physx::PxRigidStatic* actor, _Out_ STATIC_DEBUG_MESH& outMesh) const;

	// 정적 액터 핸들 → 처음 화면에 들 때 만든 칸별 정점. 액터를 내리면 같이 지운다.
	unordered_map<uint32, STATIC_DEBUG_MESH> m_StaticDebugMeshes;
#endif

private:
	void Release_Physics();
	Bool Initialize_SoftBodySystem();
	void Release_SoftBodySystem();

private:
	physx::PxFoundation* m_Foundation{ nullptr };
	physx::PxPhysics* m_Physics{ nullptr };
	physx::PxDefaultCpuDispatcher* m_Dispatcher{ nullptr };
	physx::PxScene* m_Scene{ nullptr };
	physx::PxMaterial* m_DefaultMaterial{ nullptr };
	physx::PxControllerManager* m_ControllerManager{ nullptr };

	// 구운 메시는 태그로 공유하고, 액터는 올린 오브젝트마다 하나씩 잡는다.
	unordered_map<wstring, physx::PxTriangleMesh*> m_StaticMeshes;
	unordered_map<uint32, physx::PxRigidStatic*> m_StaticActors;
	unordered_map<uint32, physx::PxController*> m_Controllers;
	struct QUERY_ACTOR
	{
		physx::PxRigidDynamic* actor{ nullptr };
		// 마지막으로 PhysX에 넘긴 값. 같으면 다시 넘기지 않는다.
		QUERY_SHAPE_DESC desc{};
		uint32 layerBit{};
		uint32 maskBits{};
	};
	unordered_map<uint32, QUERY_ACTOR> m_QueryActors;
	// 정적 액터·컨트롤러·쿼리 도형이 한 카운터를 나눠 써서 핸들이 서로 겹치지 않는다.
	uint32 m_NextActorHandle{ 1 };

	// 소프트바디 시스템은 콜라이더만 올려 두고 스텝하지 않는다. 소프트바디는 시스템 밖에 두고 저마다 돌린다.
	JPH::PhysicsSystem* m_SoftBodySystem{ nullptr };
	JPH::TempAllocator* m_SoftBodyAllocator{ nullptr };
	struct SOFTBODY
	{
		JPH::Body* body{ nullptr };
		vector<SOFTBODY_DESC::VERTEX> vertices;
		Float linearDamping{};
		Float gravityFactor{};
	};
	unordered_map<uint32, SOFTBODY> m_SoftBodies;
	unordered_map<uint32, uint32> m_SoftBodyColliders;	// 핸들 → Jolt BodyID
	uint32 m_NextSoftBodyHandle{ 1 };

public:
	static Unique<PhysicsManager> Create();
};

NS_END
