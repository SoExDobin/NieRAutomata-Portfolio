#include "PhysicsManager.h"

#include "Game.h"
#include "Model.h"
#include "SpdLogger.h"
#include "String_Helper.h"

#include <physx/PxPhysicsAPI.h>

// vcpkg Jolt.lib가 이 정의로 빌드됐다(vcpkg_installed/x64-windows/share/Jolt/JoltConfig.cmake). 어긋나면 VerifyJoltVersionID가 잡는다.
#define JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
#define JPH_OBJECT_STREAM
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/SoftBody/SoftBodyCreationSettings.h>
#include <Jolt/Physics/SoftBody/SoftBodyMotionProperties.h>

#include <cstdarg>

using namespace physx;

namespace
{
	/* 소프트바디 시스템의 레이어. 소프트바디는 시스템에 넣지 않으므로 브로드페이즈에는 콜라이더만 있다.
	   소프트바디는 콜라이더와만 부딪히고, 키네마틱인 콜라이더끼리는 서로 밀지 않는다. */
	constexpr JPH::ObjectLayer SOFTBODY_LAYER{ 0 };
	constexpr JPH::ObjectLayer SOFTBODY_COLLIDER_LAYER{ 1 };
	// 한 프레임에 이보다 길게 돌리지 않는다. 히치 뒤에 소프트바디가 튀지 않게 한다.
	constexpr Float MAX_SOFTBODY_STEP{ 1.f / 30.f };

	class SoftBodyBroadPhaseLayers final : public JPH::BroadPhaseLayerInterface
	{
	public:
		JPH::uint GetNumBroadPhaseLayers() const override { return 1; }
		JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer) const override { return JPH::BroadPhaseLayer{ 0 }; }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
		const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer) const override { return "SoftBodyCollider"; }
#endif
	};

	class SoftBodyObjectVsBroadPhase final : public JPH::ObjectVsBroadPhaseLayerFilter
	{
	public:
		bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer) const override { return layer == SOFTBODY_LAYER; }
	};

	class SoftBodyObjectPairs final : public JPH::ObjectLayerPairFilter
	{
	public:
		bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override { return a != b; }
	};

	// 소프트바디와 콜라이더의 그룹 ID는 소프트바디 핸들이다. 자기 콜라이더(천이면 같은 clp의 clh)만 만난다. 레이어만으로는 모든 소프트바디가 모든 콜라이더를
	// 만나서, em3002 줄이 em3001 사슬의 머리 구(1.1 m) 안에서 시작해 매 프레임 밀려났다 돌아오며 떨렸다.
	class SoftBodyGroupFilter final : public JPH::GroupFilter
	{
	public:
		bool CanCollide(const JPH::CollisionGroup& a, const JPH::CollisionGroup& b) const override { return a.GetGroupID() == b.GetGroupID(); }
	};

	JPH::CollisionGroup SoftBody_Group(uint32 softBody)
	{
		// 몸체들이 참조를 들고 놓아도 지워지지 않게 임베디드로 둔다.
		static SoftBodyGroupFilter filter;
		static const bool embedded = (filter.SetEmbedded(), true);
		(void)embedded;
		return JPH::CollisionGroup{ &filter, softBody, 0 };
	}

	SoftBodyBroadPhaseLayers g_SoftBodyBroadPhaseLayers;
	SoftBodyObjectVsBroadPhase g_SoftBodyObjectVsBroadPhase;
	SoftBodyObjectPairs g_SoftBodyObjectPairs;

	void Trace_Jolt(const char* format, ...)
	{
		char message[512]{};
		va_list args;
		va_start(args, format);
		vsnprintf(message, sizeof(message), format, args);
		va_end(args);
		LOG_WARN(L"[Physics] Jolt: {}", Helper::To_wString(message));
	}

	// 행 벡터 행렬을 그대로 읽으면 Jolt(열 벡터)의 같은 변환이 된다.
	JPH::Mat44 To_Jolt(const Matrix& matrix) { return JPH::Mat44::sLoadFloat4x4(reinterpret_cast<const JPH::Float4*>(&matrix)); }
	JPH::Vec3 To_Jolt(const Vector3& vector) { return JPH::Vec3{ vector.x, vector.y, vector.z }; }
	Vector3 From_Jolt(JPH::Vec3Arg vector) { return Vector3{ vector.GetX(), vector.GetY(), vector.GetZ() }; }

	/// PhysX는 자기 할당자와 에러 콜백을 요구한다. 기본 구현으로 충분하되,
	/// 에러는 엔진 로그로 넘겨서 조용히 사라지지 않게 한다.
	PxDefaultAllocator g_Allocator;

	class EngineErrorCallback final : public PxErrorCallback
	{
	public:
		void reportError(PxErrorCode::Enum code, const char* message, const char* file, int line) override
		{
			LOG_ERROR(L"[Physics] PhysX error {}: {} ({}:{})",
				static_cast<int>(code),
				Helper::To_wString(message ? message : ""),
				Helper::To_wString(file ? file : ""),
				line);
		}
	};

	EngineErrorCallback g_ErrorCallback;

	/* 게임플레이 쿼리 도형 표식. 쿼리 필터 word3에 넣는다. overlap은 word0(레이어)만 쓰므로
	   이 값은 판정에 끼지 않고, 월드 쿼리가 이 도형을 빼는 데만 쓴다. */
	constexpr PxU32 QUERY_SHAPE_TAG{ 1u };

	/// 캐릭터 컨트롤러가 게임플레이 볼륨에 걸리지 않게 한다. 캡슐이 자기 공격 판정 구에 막히는 식이다.
	class WorldOnlyQueryFilter final : public PxQueryFilterCallback
	{
	public:
		PxQueryHitType::Enum preFilter(const PxFilterData&, const PxShape* shape, const PxRigidActor*, PxHitFlags&) override
		{
			return shape && shape->getQueryFilterData().word3 == QUERY_SHAPE_TAG ? PxQueryHitType::eNONE : PxQueryHitType::eBLOCK;
		}
		PxQueryHitType::Enum postFilter(const PxFilterData&, const PxQueryHit&, const PxShape*, const PxRigidActor*) override
		{
			return PxQueryHitType::eBLOCK;
		}
	};

	WorldOnlyQueryFilter g_WorldOnlyQueryFilter;

	/* 게임플레이 도형끼리의 LayerMask 필터. 기본 필터는 word끼리 AND만 해서 양방향 규칙
	   (A 마스크에 B 레이어, B 마스크에 A 레이어)을 표현할 수 없으므로 preFilter에서 한다. */
	class GameplayPairFilter final : public PxQueryFilterCallback
	{
	public:
		GameplayPairFilter(PxU32 layerBit, PxU32 maskBits) : m_LayerBit{ layerBit }, m_MaskBits{ maskBits } {}

		PxQueryHitType::Enum preFilter(const PxFilterData&, const PxShape* shape, const PxRigidActor*, PxHitFlags&) override
		{
			if (nullptr == shape)
				return PxQueryHitType::eNONE;
			const PxFilterData other = shape->getQueryFilterData();
			if (other.word3 != QUERY_SHAPE_TAG)
				return PxQueryHitType::eNONE;
			return (m_MaskBits & other.word0) && (other.word1 & m_LayerBit) ? PxQueryHitType::eTOUCH : PxQueryHitType::eNONE;
		}
		PxQueryHitType::Enum postFilter(const PxFilterData&, const PxQueryHit&, const PxShape*, const PxRigidActor*) override
		{
			return PxQueryHitType::eTOUCH;
		}

	private:
		PxU32 m_LayerBit;
		PxU32 m_MaskBits;
	};

	Bool Same_Geometry(const QUERY_SHAPE_DESC& a, const QUERY_SHAPE_DESC& b)
	{
		return a.type == b.type && a.extents == b.extents && a.radius == b.radius;
	}

	Bool Same_Pose(const QUERY_SHAPE_DESC& a, const QUERY_SHAPE_DESC& b)
	{
		return a.center == b.center && a.orientation == b.orientation;
	}

	/// PxBoxGeometry·PxSphereGeometry는 0 크기를 받지 않는다.
	constexpr Float MIN_QUERY_EXTENT{ 1e-4f };

	PxGeometryHolder Make_QueryGeometry(const QUERY_SHAPE_DESC& desc)
	{
		if (desc.type == COLLIDER_TYPE::SPHERE)
			return PxGeometryHolder{ PxSphereGeometry{ std::max(desc.radius, MIN_QUERY_EXTENT) } };

		return PxGeometryHolder{ PxBoxGeometry{
			std::max(desc.extents.x, MIN_QUERY_EXTENT),
			std::max(desc.extents.y, MIN_QUERY_EXTENT),
			std::max(desc.extents.z, MIN_QUERY_EXTENT) } };
	}

	PxTransform Make_QueryPose(const QUERY_SHAPE_DESC& desc)
	{
		PxQuat rotation{ PxIdentity };
		if (desc.type == COLLIDER_TYPE::OBB)
		{
			rotation = PxQuat{ desc.orientation.x, desc.orientation.y, desc.orientation.z, desc.orientation.w };
			if (!rotation.isSane())
				rotation = PxQuat{ PxIdentity };
			else
				rotation.normalize();
		}
		return PxTransform{ PxVec3{ desc.center.x, desc.center.y, desc.center.z }, rotation };
	}

	PxShape* Get_OnlyShape(const PxRigidActor& actor)
	{
		PxShape* shape = nullptr;
		actor.getShapes(&shape, 1);
		return shape;
	}
}

PhysicsManager::PhysicsManager() {}

PhysicsManager::~PhysicsManager()
{
	Release_Physics();
}

HRESULT PhysicsManager::Initialize(void* arg)
{
	if (FAILED(EngineManager::Initialize(arg)))
		return E_FAIL;

	m_Foundation = PxCreateFoundation(PX_PHYSICS_VERSION, g_Allocator, g_ErrorCallback);
	if (nullptr == m_Foundation)
	{
		LOG_ERROR(L"[Physics] Failed to create PxFoundation");
		return E_FAIL;
	}

	m_Physics = PxCreatePhysics(PX_PHYSICS_VERSION, *m_Foundation, PxTolerancesScale{});
	if (nullptr == m_Physics)
	{
		LOG_ERROR(L"[Physics] Failed to create PxPhysics");
		Release_Physics();
		return E_FAIL;
	}

	// 정적 지형만 올리는 단계라 워커는 최소로 둔다. 동적 강체가 생기면 그때 늘린다.
	m_Dispatcher = PxDefaultCpuDispatcherCreate(1);
	if (nullptr == m_Dispatcher)
	{
		LOG_ERROR(L"[Physics] Failed to create CPU dispatcher");
		Release_Physics();
		return E_FAIL;
	}

	PxSceneDesc sceneDesc{ m_Physics->getTolerancesScale() };
	// 중력은 Movement가 직접 적분한다. 씬 중력을 켜면 같은 힘이 두 번 들어간다.
	sceneDesc.gravity = PxVec3{ 0.f, 0.f, 0.f };
	sceneDesc.cpuDispatcher = m_Dispatcher;
	sceneDesc.filterShader = PxDefaultSimulationFilterShader;

	m_Scene = m_Physics->createScene(sceneDesc);
	if (nullptr == m_Scene)
	{
		LOG_ERROR(L"[Physics] Failed to create PxScene");
		Release_Physics();
		return E_FAIL;
	}

	m_DefaultMaterial = m_Physics->createMaterial(0.5f, 0.5f, 0.f);
	if (nullptr == m_DefaultMaterial)
	{
		LOG_ERROR(L"[Physics] Failed to create the default material");
		Release_Physics();
		return E_FAIL;
	}

	m_ControllerManager = PxCreateControllerManager(*m_Scene);
	if (nullptr == m_ControllerManager)
	{
		LOG_ERROR(L"[Physics] Failed to create PxControllerManager");
		Release_Physics();
		return E_FAIL;
	}

	LOG_INFO(L"[Physics] PhysX ready");

	// 소프트바디가 없어도 게임은 돈다. 실패하면 소프트바디만 꺼진다.
	Initialize_SoftBodySystem();
	return S_OK;
}

Bool PhysicsManager::Initialize_SoftBodySystem()
{
	JPH::RegisterDefaultAllocator();
	JPH::Trace = Trace_Jolt;
	if (!JPH::VerifyJoltVersionID())
	{
		LOG_ERROR(L"[Physics] Jolt headers and Jolt.lib disagree on build settings; soft bodies are off");
		return false;
	}

	JPH::Factory::sInstance = new JPH::Factory();
	JPH::RegisterTypes();

	m_SoftBodyAllocator = new JPH::TempAllocatorImpl(1024 * 1024);
	m_SoftBodySystem = new JPH::PhysicsSystem();
	m_SoftBodySystem->Init(256, 0, 256, 256, g_SoftBodyBroadPhaseLayers, g_SoftBodyObjectVsBroadPhase, g_SoftBodyObjectPairs);

	LOG_INFO(L"[Physics] Jolt {}.{}.{} ready (soft body)", JPH_VERSION_MAJOR, JPH_VERSION_MINOR, JPH_VERSION_PATCH);
	return true;
}

void PhysicsManager::Release_SoftBodySystem()
{
	if (m_SoftBodySystem)
	{
		JPH::BodyInterface& bodies = m_SoftBodySystem->GetBodyInterface();
		for (auto& [handle, softBody] : m_SoftBodies)
			if (softBody.body) bodies.DestroyBodyWithoutID(softBody.body);
		for (auto& [handle, id] : m_SoftBodyColliders)
		{
			bodies.RemoveBody(JPH::BodyID{ id });
			bodies.DestroyBody(JPH::BodyID{ id });
		}
	}
	m_SoftBodies.clear();
	m_SoftBodyColliders.clear();

	delete m_SoftBodySystem;
	m_SoftBodySystem = nullptr;
	delete m_SoftBodyAllocator;
	m_SoftBodyAllocator = nullptr;

	if (JPH::Factory::sInstance)
	{
		JPH::UnregisterTypes();
		delete JPH::Factory::sInstance;
		JPH::Factory::sInstance = nullptr;
	}
}

uint32 PhysicsManager::Create_SoftBody(const SOFTBODY_DESC& desc, const vector<Matrix>& jointMatrices)
{
	if (nullptr == m_SoftBodySystem || desc.vertices.empty())
		return 0;

	const uint32 numVertices = static_cast<uint32>(desc.vertices.size());
	for (const SOFTBODY_DESC::VERTEX& vertex : desc.vertices)
		if (vertex.joint >= jointMatrices.size())
		{
			LOG_ERROR(L"[Physics] SoftBody vertex joint {} out of {} joints", vertex.joint, jointMatrices.size());
			return 0;
		}
	for (const SOFTBODY_DESC::EDGE& edge : desc.edges)
		if (edge.vertex[0] >= numVertices || edge.vertex[1] >= numVertices)
		{
			LOG_ERROR(L"[Physics] SoftBody edge {}-{} out of {} vertices", edge.vertex[0], edge.vertex[1], numVertices);
			return 0;
		}

	JPH::Ref<JPH::SoftBodySharedSettings> settings = new JPH::SoftBodySharedSettings();
	settings->mVertices.reserve(numVertices);
	for (const SOFTBODY_DESC::VERTEX& vertex : desc.vertices)
	{
		const Vector3 position = Vector3::Transform(vertex.offset, jointMatrices[vertex.joint]);
		settings->mVertices.emplace_back(JPH::Float3{ position.x, position.y, position.z }, JPH::Float3{ 0.f, 0.f, 0.f }, vertex.invMass);
	}
	for (const SOFTBODY_DESC::EDGE& edge : desc.edges)
		settings->mEdgeConstraints.emplace_back(edge.vertex[0], edge.vertex[1], edge.compliance);
	settings->CalculateEdgeLengths();

	// 지금 관절 자세를 기준으로 둔다. 스키닝 자리 = 이번 관절 × 기준의 역 × 기준 정점 = 관절 공간 offset의 이번 자리.
	for (uint32 joint = 0; joint < jointMatrices.size(); ++joint)
		settings->mInvBindMatrices.emplace_back(joint, To_Jolt(jointMatrices[joint]).Inversed());
	for (uint32 i = 0; i < numVertices; ++i)
	{
		const SOFTBODY_DESC::VERTEX& vertex = desc.vertices[i];
		// 최대 거리 0이 관절에 박힌 정점이다. 역질량도 0이어야 Jolt가 움직이지 않는다.
		JPH::SoftBodySharedSettings::Skinned skinned{ i, vertex.invMass > 0.f ? vertex.maxDistance : 0.f, FLT_MAX, 0.f };
		skinned.mWeights[0] = JPH::SoftBodySharedSettings::SkinWeight{ vertex.joint, 1.f };
		settings->mSkinnedConstraints.push_back(skinned);
	}
	settings->Optimize();

	const uint32 handle = m_NextSoftBodyHandle;
	JPH::SoftBodyCreationSettings creation{ settings, JPH::RVec3::sZero(), JPH::Quat::sIdentity(), SOFTBODY_LAYER };
	creation.mCollisionGroup = SoftBody_Group(handle);
	creation.mNumIterations = std::max(desc.iterations, 1u);
	creation.mLinearDamping = desc.linearDamping;
	creation.mGravityFactor = desc.gravityFactor;
	creation.mVertexRadius = desc.vertexRadius;
	// 몸체를 원점에 두면 정점이 월드 좌표 그대로라 관절 행렬을 그대로 쓴다.
	creation.mUpdatePosition = false;
	creation.mAllowSleeping = false;
	JPH::Body* body = m_SoftBodySystem->GetBodyInterface().CreateSoftBodyWithoutID(creation);
	if (nullptr == body)
		return 0;

	++m_NextSoftBodyHandle;
	m_SoftBodies.emplace(handle, SOFTBODY{ body, desc.vertices, desc.linearDamping, desc.gravityFactor });
	Simulate_SoftBody(handle, jointMatrices, 0.f, true, SOFTBODY_TUNING{}, {});
	return handle;
}

void PhysicsManager::Simulate_SoftBody(uint32 handle, const vector<Matrix>& jointMatrices, Float timeDelta, Bool reset,
	const SOFTBODY_TUNING& tuning, const vector<WIND_VOLUME>& winds)
{
	const auto it = m_SoftBodies.find(handle);
	if (it == m_SoftBodies.end())
		return;
	SOFTBODY& softBody = it->second;
	JPH::Body& body = *softBody.body;
	auto* motion = static_cast<JPH::SoftBodyMotionProperties*>(body.GetMotionProperties());

	// Jolt는 관절을 몸체 기준으로 받는다.
	const JPH::RMat44 centerOfMass = body.GetCenterOfMassTransform();
	const JPH::Mat44 toBody = centerOfMass.InversedRotationTranslation();
	JPH::Array<JPH::Mat44> joints;
	joints.reserve(jointMatrices.size());
	for (const Matrix& joint : jointMatrices)
		joints.push_back(toBody * To_Jolt(joint));

	motion->SetLinearDamping(softBody.linearDamping * std::max(tuning.damping, 0.f));
	motion->SetGravityFactor(softBody.gravityFactor * tuning.gravity);
	motion->SetSkinnedMaxDistanceMultiplier(std::max(tuning.swing, 0.f));
	motion->SkinVertices(centerOfMass, joints.data(), static_cast<JPH::uint>(joints.size()), reset, *m_SoftBodyAllocator);
	if (reset || timeDelta <= 0.f)
		return;

	// 원래 자세로 되돌아가는 비율(1/60초 기준). 클립이 정한 천 모양을 얼마나 지킬지다.
	// 스텝 전에 당긴다. 스텝 뒤에 당기면 몸 콜라이더 안에 있는 원래 자세(em3002 앞천, em3003 치마)로 다시 끌려 들어간 자리를
	// 그리고, 다음 스텝이 또 밀어내 떨린다. 스텝 전이면 그리는 자리는 언제나 충돌을 푼 뒤다.
	const Float step = std::min(timeDelta, MAX_SOFTBODY_STEP);
	auto& vertices = motion->GetVertices();
	const Float frames = step * 60.f;

	// 바람은 몸체 공간으로 옮겨 스텝 전에 속도에 더한다. 감쇠·최대 거리가 중력과 같이 받아 준다.
	// 영역 안 정점이 모두 같은 속도를 받으면 다발이 한 덩어리로 밀려 빳빳해 보여서, 세기를 거리 따라 물결치게 한다.
	struct BODY_WIND { JPH::Vec3 from, axis, velocity; Float axisLengthSq, radiusSq, waveNumber, wavePhase; };
	JPH::Array<BODY_WIND> bodyWinds;
	bodyWinds.reserve(winds.size());
	for (const WIND_VOLUME& wind : winds)
	{
		const JPH::Vec3 from = toBody * To_Jolt(wind.from);
		const JPH::Vec3 axis = toBody * To_Jolt(wind.to) - from;
		bodyWinds.push_back({ from, axis, toBody.Multiply3x3(To_Jolt(wind.acceleration)) * step,
			axis.LengthSq(), wind.radius > 0.f ? wind.radius * wind.radius : -1.f, wind.waveNumber, wind.wavePhase });
	}

	for (size_t i = 0; i < softBody.vertices.size(); ++i)
	{
		const SOFTBODY_DESC::VERTEX& source = softBody.vertices[i];
		if (source.invMass <= 0.f)
			continue;
		const Float rate = std::clamp(source.followRate * tuning.follow, 0.f, 1.f);
		if (rate > 0.f)
		{
			const Float keep = powf(1.f - rate, frames);
			const JPH::Vec3 target = joints[source.joint] * To_Jolt(source.offset);
			vertices[i].mPosition = target + (vertices[i].mPosition - target) * keep;
			vertices[i].mVelocity *= keep;
		}
		for (const BODY_WIND& wind : bodyWinds)
		{
			const JPH::Vec3 toVertex = vertices[i].mPosition - wind.from;
			if (wind.radiusSq > 0.f)
			{
				const Float along = wind.axisLengthSq > 1e-10f ? std::clamp(toVertex.Dot(wind.axis) / wind.axisLengthSq, 0.f, 1.f) : 0.f;
				if ((toVertex - wind.axis * along).LengthSq() > wind.radiusSq)
					continue;
			}
			const Float wave = wind.waveNumber > 0.f ? 1.f + sinf(wind.waveNumber * toVertex.Length() - wind.wavePhase) : 1.f;
			vertices[i].mVelocity += wind.velocity * wave;
		}
	}
	motion->CustomUpdate(step, body, *m_SoftBodySystem);
}

void PhysicsManager::Get_SoftBodyPositions(uint32 handle, vector<Vector3>& outPositions) const
{
	outPositions.clear();
	const auto it = m_SoftBodies.find(handle);
	if (it == m_SoftBodies.end())
		return;

	const JPH::Body& body = *it->second.body;
	const auto* motion = static_cast<const JPH::SoftBodyMotionProperties*>(body.GetMotionProperties());
	const JPH::RMat44 centerOfMass = body.GetCenterOfMassTransform();
	outPositions.reserve(motion->GetVertices().size());
	for (const auto& vertex : motion->GetVertices())
		outPositions.push_back(From_Jolt(centerOfMass * vertex.mPosition));
}

void PhysicsManager::Release_SoftBody(uint32 handle)
{
	const auto it = m_SoftBodies.find(handle);
	if (it == m_SoftBodies.end())
		return;
	if (m_SoftBodySystem && it->second.body)
		m_SoftBodySystem->GetBodyInterface().DestroyBodyWithoutID(it->second.body);
	m_SoftBodies.erase(it);
}

uint32 PhysicsManager::Add_SoftBodyCollider(uint32 softBody, Float radius, Float halfHeight)
{
	if (nullptr == m_SoftBodySystem || radius <= 0.f || !m_SoftBodies.contains(softBody))
		return 0;

	JPH::ShapeRefC shape = halfHeight > 0.f
		? JPH::ShapeRefC{ new JPH::CapsuleShape(halfHeight, radius) }
		: JPH::ShapeRefC{ new JPH::SphereShape(radius) };
	JPH::BodyCreationSettings settings{ shape, JPH::RVec3::sZero(), JPH::Quat::sIdentity(),
		JPH::EMotionType::Kinematic, SOFTBODY_COLLIDER_LAYER };
	settings.mCollisionGroup = SoftBody_Group(softBody);
	const JPH::BodyID id = m_SoftBodySystem->GetBodyInterface().CreateAndAddBody(settings, JPH::EActivation::DontActivate);
	if (id.IsInvalid())
		return 0;

	const uint32 handle = m_NextSoftBodyHandle++;
	m_SoftBodyColliders.emplace(handle, id.GetIndexAndSequenceNumber());
	return handle;
}

void PhysicsManager::Move_SoftBodyCollider(uint32 handle, const Vector3& from, const Vector3& to)
{
	const auto it = m_SoftBodyColliders.find(handle);
	if (it == m_SoftBodyColliders.end())
		return;

	// 캡슐은 Y축을 따라 선다.
	const JPH::Vec3 axis = To_Jolt(to - from);
	const JPH::Quat rotation = axis.LengthSq() > 1e-10f
		? JPH::Quat::sFromTo(JPH::Vec3::sAxisY(), axis.Normalized())
		: JPH::Quat::sIdentity();
	m_SoftBodySystem->GetBodyInterface().SetPositionAndRotation(JPH::BodyID{ it->second },
		To_Jolt((from + to) * 0.5f), rotation, JPH::EActivation::DontActivate);
}

void PhysicsManager::Remove_SoftBodyCollider(uint32 handle)
{
	const auto it = m_SoftBodyColliders.find(handle);
	if (it == m_SoftBodyColliders.end())
		return;
	if (m_SoftBodySystem)
	{
		m_SoftBodySystem->GetBodyInterface().RemoveBody(JPH::BodyID{ it->second });
		m_SoftBodySystem->GetBodyInterface().DestroyBody(JPH::BodyID{ it->second });
	}
	m_SoftBodyColliders.erase(it);
}

void PhysicsManager::Step(Float fixedDelta)
{
	if (!Is_Active() || nullptr == m_Scene || fixedDelta <= 0.f)
		return;

	m_Scene->simulate(fixedDelta);
	m_Scene->fetchResults(true);
}

Bool PhysicsManager::Ensure_StaticMesh(const wstring& modelTag)
{
	if (m_StaticMeshes.contains(modelTag))
		return true;

	if (nullptr == m_Physics)
		return false;

	const int32 levelIndex = GAME_INSTANCE->Get_ContainLevelByModelTag(modelTag);
	if (levelIndex < 0)
	{
		LOG_ERROR(L"[Physics] No level contains model {}", modelTag);
		return false;
	}

	const Shared<Model> model = GAME_INSTANCE->Get_Model(levelIndex, modelTag.c_str());
	if (nullptr == model)
	{
		LOG_ERROR(L"[Physics] Model prototype {} not found", modelTag);
		return false;
	}

	// 여기서 나오는 좌표에는 ModelSettings 사전 변환이 이미 적용돼 있다. 다시 변환하지 않는다.
	vector<Float> positions;
	vector<int32> indices;
	model->Extract_RawMeshData(positions, indices);

	if (positions.empty() || indices.empty())
	{
		LOG_ERROR(L"[Physics] Model {} has no geometry to cook", modelTag);
		return false;
	}

	PxTriangleMeshDesc meshDesc{};
	meshDesc.points.count = static_cast<PxU32>(positions.size() / 3);
	meshDesc.points.stride = sizeof(Float) * 3;
	meshDesc.points.data = positions.data();
	meshDesc.triangles.count = static_cast<PxU32>(indices.size() / 3);
	meshDesc.triangles.stride = sizeof(int32) * 3;
	meshDesc.triangles.data = indices.data();

	if (!meshDesc.isValid())
	{
		LOG_ERROR(L"[Physics] Triangle mesh desc for {} is invalid", modelTag);
		return false;
	}

	const PxCookingParams cookingParams{ m_Physics->getTolerancesScale() };
	PxTriangleMesh* mesh = PxCreateTriangleMesh(cookingParams, meshDesc,
		m_Physics->getPhysicsInsertionCallback());

	if (nullptr == mesh)
	{
		LOG_ERROR(L"[Physics] Failed to cook {} ({} triangles)", modelTag, meshDesc.triangles.count);
		return false;
	}

	m_StaticMeshes.emplace(modelTag, mesh);
	LOG_INFO(L"[Physics] Cooked {} : {} vertices, {} triangles",
		modelTag, meshDesc.points.count, meshDesc.triangles.count);
	return true;
}

Bool PhysicsManager::Get_StaticBounds(BoundingBox& outBounds) const
{
	PxBounds3 bounds = PxBounds3::empty();
	for (const auto& [handle, actor] : m_StaticActors)
		if (actor)
			bounds.include(actor->getWorldBounds());
	if (bounds.isEmpty())
		return false;
	const PxVec3 center = bounds.getCenter();
	const PxVec3 extents = bounds.getExtents();
	outBounds = BoundingBox{ XMFLOAT3{ center.x, center.y, center.z }, XMFLOAT3{ extents.x, extents.y, extents.z } };
	return true;
}

uint32 PhysicsManager::Add_StaticActor(const wstring& modelTag, const Matrix& worldMatrix)
{
	if (nullptr == m_Scene || !Ensure_StaticMesh(modelTag))
		return 0;

	Vector3 scale{}, translation{};
	Quaternion rotation{};
	Matrix decomposable = worldMatrix;   // SimpleMath의 Decompose는 const가 아니다.
	if (!decomposable.Decompose(scale, rotation, translation))
	{
		LOG_ERROR(L"[Physics] World matrix for {} is not decomposable", modelTag);
		return 0;
	}

	const PxTransform pose{
		PxVec3{ translation.x, translation.y, translation.z },
		PxQuat{ rotation.x, rotation.y, rotation.z, rotation.w } };

	// PxTransform은 스케일을 담지 못한다. 스케일은 지오메트리 쪽으로 넣는다.
	const PxTriangleMeshGeometry geometry{
		m_StaticMeshes.at(modelTag),
		PxMeshScale{ PxVec3{ scale.x, scale.y, scale.z } } };

	PxRigidStatic* actor = m_Physics->createRigidStatic(pose);
	if (nullptr == actor)
	{
		LOG_ERROR(L"[Physics] Failed to create static actor for {}", modelTag);
		return 0;
	}

	if (nullptr == PxRigidActorExt::createExclusiveShape(*actor, geometry, *m_DefaultMaterial))
	{
		actor->release();
		LOG_ERROR(L"[Physics] Failed to create shape for {}", modelTag);
		return 0;
	}

	m_Scene->addActor(*actor);

	const uint32 handle = m_NextActorHandle++;
	m_StaticActors.emplace(handle, actor);

	LOG_INFO(L"[Physics] Static actor {} registered for {} at ({:.2f}, {:.2f}, {:.2f})",
		handle, modelTag, translation.x, translation.y, translation.z);
	return handle;
}

void PhysicsManager::Remove_StaticActor(uint32 handle)
{
	const auto found = m_StaticActors.find(handle);
	if (found == m_StaticActors.end())
		return;

	if (found->second)
	{
		if (m_Scene)
			m_Scene->removeActor(*found->second);
		found->second->release();
	}

	m_StaticActors.erase(found);
#ifdef _DEBUG
	m_StaticDebugMeshes.erase(handle);
#endif
}

Bool PhysicsManager::Raycast(const Vector3& origin, const Vector3& direction,
	Float distance, Vector3& outHit, Vector3* outNormal) const
{
	if (nullptr == m_Scene || distance <= 0.f)
		return false;

	Vector3 unitDirection = direction;
	unitDirection.Normalize();

	PxRaycastBuffer hit;
	// eMESH_BOTH_SIDES: 삼각형 winding이 뒤집혀 있어도 맞힌다. DirectX(왼손)와
	// PhysX(오른손)가 만나는 지점이라, 지형이 있는데 레이가 통과하는 사고를 막는다.
	const PxHitFlags hitFlags = PxHitFlag::eDEFAULT | PxHitFlag::eMESH_BOTH_SIDES;
	// 정적 액터는 COL 월드 메시뿐이다. 동적(캐릭터 캡슐·게임플레이 볼륨)까지 보면 캡슐 안에서
	// 쏜 카메라 레이가 거리 0에서 맞고, 지면 레이가 다른 캐릭터 머리를 바닥으로 잡는다.
	const PxQueryFilterData filterData{ PxQueryFlag::eSTATIC };

	if (!m_Scene->raycast(
		PxVec3{ origin.x, origin.y, origin.z },
		PxVec3{ unitDirection.x, unitDirection.y, unitDirection.z },
		distance, hit, hitFlags, filterData))
		return false;

	if (!hit.hasBlock)
		return false;

	outHit = Vector3{ hit.block.position.x, hit.block.position.y, hit.block.position.z };
	if (outNormal)
		*outNormal = Vector3{ hit.block.normal.x, hit.block.normal.y, hit.block.normal.z };
	return true;
}

uint32 PhysicsManager::Create_Controller(const CHARACTER_CONTROLLER_DESC& desc, const Vector3& footPosition)
{
	if (nullptr == m_ControllerManager || nullptr == m_DefaultMaterial)
		return 0;

	PxCapsuleControllerDesc capsuleDesc{};
	capsuleDesc.radius = desc.radius;
	capsuleDesc.height = desc.height;
	capsuleDesc.upDirection = PxVec3{ 0.f, 1.f, 0.f };
	capsuleDesc.slopeLimit = cosf(XMConvertToRadians(desc.slopeLimitDegrees));
	capsuleDesc.stepOffset = desc.stepOffset;
	capsuleDesc.contactOffset = desc.contactOffset;
	capsuleDesc.material = m_DefaultMaterial;
	capsuleDesc.climbingMode = PxCapsuleClimbingMode::eCONSTRAINED;
	// desc.position은 캡슐 중심이다. 생성 후 발 위치로 다시 맞추므로 여기서는 대략만 둔다.
	capsuleDesc.position = PxExtendedVec3{ footPosition.x, footPosition.y + desc.radius + desc.height * 0.5f, footPosition.z };

	if (!capsuleDesc.isValid())
	{
		LOG_ERROR(L"[Physics] Character controller desc is invalid (radius {:.2f}, height {:.2f}, step {:.2f})",
			desc.radius, desc.height, desc.stepOffset);
		return 0;
	}

	PxController* controller = m_ControllerManager->createController(capsuleDesc);
	if (nullptr == controller)
	{
		LOG_ERROR(L"[Physics] Failed to create character controller");
		return 0;
	}

	controller->setFootPosition(PxExtendedVec3{ footPosition.x, footPosition.y, footPosition.z });

	const uint32 handle = m_NextActorHandle++;
	m_Controllers.emplace(handle, controller);

	LOG_INFO(L"[Physics] Character controller {} at foot ({:.2f}, {:.2f}, {:.2f}), radius {:.2f}, total height {:.2f}",
		handle, footPosition.x, footPosition.y, footPosition.z, desc.radius, desc.height + desc.radius * 2.f);
	return handle;
}

uint32 PhysicsManager::Move_Controller(uint32 handle, const Vector3& displacement, Float timeDelta)
{
	const auto found = m_Controllers.find(handle);
	if (found == m_Controllers.end() || nullptr == found->second)
		return 0;

	const PxControllerFilters filters{ nullptr, &g_WorldOnlyQueryFilter };
	const PxControllerCollisionFlags flags = found->second->move(
		PxVec3{ displacement.x, displacement.y, displacement.z }, 0.001f, timeDelta, filters);

	uint32 result = 0;
	if (flags & PxControllerCollisionFlag::eCOLLISION_SIDES) result |= CONTROLLER_COLLISION_SIDES;
	if (flags & PxControllerCollisionFlag::eCOLLISION_UP)    result |= CONTROLLER_COLLISION_UP;
	if (flags & PxControllerCollisionFlag::eCOLLISION_DOWN)  result |= CONTROLLER_COLLISION_DOWN;
	return result;
}

void PhysicsManager::Set_ControllerFootPosition(uint32 handle, const Vector3& footPosition)
{
	const auto found = m_Controllers.find(handle);
	if (found == m_Controllers.end() || nullptr == found->second)
		return;

	found->second->setFootPosition(PxExtendedVec3{ footPosition.x, footPosition.y, footPosition.z });
}

Vector3 PhysicsManager::Get_ControllerFootPosition(uint32 handle) const
{
	const auto found = m_Controllers.find(handle);
	if (found == m_Controllers.end() || nullptr == found->second)
		return Vector3::Zero;

	const PxExtendedVec3 foot = found->second->getFootPosition();
	return Vector3{ static_cast<Float>(foot.x), static_cast<Float>(foot.y), static_cast<Float>(foot.z) };
}

void PhysicsManager::Release_Controller(uint32 handle)
{
	const auto found = m_Controllers.find(handle);
	if (found == m_Controllers.end())
		return;

	if (found->second)
		found->second->release();
	m_Controllers.erase(found);
}

uint32 PhysicsManager::Add_QueryShape(const QUERY_SHAPE_DESC& desc, uint32 layerBit, uint32 maskBits)
{
	if (nullptr == m_Scene || nullptr == m_DefaultMaterial)
		return 0;

	// 매 프레임 옮기는 도형이라 정적 액터가 아니라 키네마틱으로 둔다. 시뮬레이션 도형이 없어 스텝에는 끼지 않는다.
	PxRigidDynamic* actor = m_Physics->createRigidDynamic(Make_QueryPose(desc));
	if (nullptr == actor)
	{
		LOG_ERROR(L"[Physics] Failed to create query actor");
		return 0;
	}
	actor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);

	PxShape* shape = PxRigidActorExt::createExclusiveShape(*actor, Make_QueryGeometry(desc).any(),
		*m_DefaultMaterial, PxShapeFlag::eSCENE_QUERY_SHAPE);
	if (nullptr == shape)
	{
		actor->release();
		LOG_ERROR(L"[Physics] Failed to create query shape");
		return 0;
	}
	shape->setQueryFilterData(PxFilterData{ layerBit, maskBits, 0, QUERY_SHAPE_TAG });

	const uint32 handle = m_NextActorHandle++;
	// overlap 결과를 핸들로 되돌릴 때 쓴다.
	actor->userData = reinterpret_cast<void*>(static_cast<uintptr_t>(handle));
	m_Scene->addActor(*actor);
	m_QueryActors.emplace(handle, QUERY_ACTOR{ actor, desc, layerBit, maskBits });
	return handle;
}

void PhysicsManager::Update_QueryShape(uint32 handle, const QUERY_SHAPE_DESC& desc, uint32 layerBit, uint32 maskBits)
{
	const auto found = m_QueryActors.find(handle);
	if (found == m_QueryActors.end() || nullptr == found->second.actor)
		return;

	QUERY_ACTOR& entry = found->second;
	PxShape* shape = Get_OnlyShape(*entry.actor);
	if (nullptr == shape)
		return;

	// 크기는 소유 오브젝트의 스케일과 에디터 수정으로 바뀔 수 있다.
	if (!Same_Geometry(entry.desc, desc))
		shape->setGeometry(Make_QueryGeometry(desc).any());
	if (entry.layerBit != layerBit || entry.maskBits != maskBits)
		shape->setQueryFilterData(PxFilterData{ layerBit, maskBits, 0, QUERY_SHAPE_TAG });
	if (!Same_Pose(entry.desc, desc))
		entry.actor->setGlobalPose(Make_QueryPose(desc));

	entry.desc = desc;
	entry.layerBit = layerBit;
	entry.maskBits = maskBits;
}

void PhysicsManager::Remove_QueryShape(uint32 handle)
{
	const auto found = m_QueryActors.find(handle);
	if (found == m_QueryActors.end())
		return;

	if (found->second.actor)
	{
		if (m_Scene)
			m_Scene->removeActor(*found->second.actor);
		found->second.actor->release();
	}
	m_QueryActors.erase(found);
}

void PhysicsManager::Overlap_QueryShape(uint32 handle, vector<uint32>& outHandles) const
{
	outHandles.clear();

	const auto found = m_QueryActors.find(handle);
	if (nullptr == m_Scene || found == m_QueryActors.end() || nullptr == found->second.actor)
		return;

	const PxRigidDynamic* self = found->second.actor;
	const PxShape* shape = Get_OnlyShape(*self);
	if (nullptr == shape)
		return;

	// 쿼리 필터 데이터는 비워 두고(기본 word AND 필터를 끄려고) 레이어 규칙은 preFilter가 맡는다.
	// 월드 메시와 캡슐은 표식(word3)이 없어 preFilter에서 빠진다.
	constexpr PxU32 MAX_HITS{ 64 };
	PxOverlapHit hits[MAX_HITS];
	PxOverlapBuffer buffer{ hits, MAX_HITS };
	const PxQueryFilterData filterData{ PxQueryFlag::eDYNAMIC | PxQueryFlag::ePREFILTER | PxQueryFlag::eNO_BLOCK };
	GameplayPairFilter pairFilter{ found->second.layerBit, found->second.maskBits };

	m_Scene->overlap(shape->getGeometry(), self->getGlobalPose(), buffer, filterData, &pairFilter);

	if (buffer.getNbTouches() == MAX_HITS)
		LOG_WARN(L"[Physics] Query shape {} hit the overlap buffer limit ({})", handle, MAX_HITS);

	for (PxU32 i = 0; i < buffer.getNbTouches(); ++i)
	{
		const PxRigidActor* other = buffer.getTouch(i).actor;
		if (nullptr == other || other == self)
			continue;
		outHandles.push_back(static_cast<uint32>(reinterpret_cast<uintptr_t>(other->userData)));
	}
}

#ifdef _DEBUG
namespace
{
	// 정적 충돌 디버그 칸의 한 변(m). 타일(약 150m)을 이 크기로 나눠 화면에 든 칸만 그린다.
	constexpr Float STATIC_DEBUG_CELL_SIZE{ 25.f };
}

void PhysicsManager::Build_StaticDebugMesh(PxRigidStatic* actor, STATIC_DEBUG_MESH& outMesh) const
{
	PxShape* shape{ nullptr };
	if (1 != actor->getShapes(&shape, 1) || PxGeometryType::eTRIANGLEMESH != shape->getGeometry().getType())
		return;
	const PxTriangleMeshGeometry& geometry = static_cast<const PxTriangleMeshGeometry&>(shape->getGeometry());
	const PxTriangleMesh* mesh = geometry.triangleMesh;
	const PxTransform pose = actor->getGlobalPose() * shape->getLocalPose();
	const PxVec3* points = mesh->getVertices();
	const void* triangles = mesh->getTriangles();
	const Bool index16 = mesh->getTriangleMeshFlags() & PxTriangleMeshFlag::e16_BIT_INDICES;
	// 음수 스케일은 감는 방향을 뒤집으므로 PhysX가 보는 면 방향에 맞춰 노멀도 뒤집는다.
	const PxVec3& scale = geometry.scale.scale;
	const Float facing = scale.x * scale.y * scale.z < 0.f ? -1.f : 1.f;
	const Float floorCos = cosf(XMConvertToRadians(CHARACTER_CONTROLLER_DESC{}.slopeLimitDegrees));
	const PxVec3 lightDirection = PxVec3{ 0.4f, 0.8f, 0.3f }.getNormalized();

	map<pair<int32, int32>, vector<VertexPositionColor>> cellVertices;
	for (PxU32 triangle = 0; triangle < mesh->getNbTriangles(); ++triangle)
	{
		PxVec3 p[3];
		for (uint32 corner = 0; corner < 3; ++corner)
		{
			const PxU32 index = index16 ? static_cast<const PxU16*>(triangles)[triangle * 3 + corner]
				: static_cast<const PxU32*>(triangles)[triangle * 3 + corner];
			p[corner] = pose.transform(geometry.scale.transform(points[index]));
		}
		PxVec3 normal = (p[1] - p[0]).cross(p[2] - p[0]) * facing;
		if (normal.normalize() <= 0.f)
			continue;

		// 컨트롤러가 서는 바닥, 막히는 벽, 머리를 찧는 천장. 콜라이더 선(초록·빨강)과 헷갈리지 않게 파랑·주황·보라로 칠한다.
		// 밝기는 면 모양이 보이게 한 방향 빛으로 준다.
		Vector3 color = normal.y >= floorCos ? Vector3{ 0.2f, 0.45f, 1.f }
			: normal.y <= -floorCos ? Vector3{ 0.7f, 0.3f, 0.95f } : Vector3{ 1.f, 0.55f, 0.1f };
		color *= 0.55f + 0.45f * (std::max)(0.f, normal.dot(lightDirection));
		const XMFLOAT4 vertexColor{ color.x, color.y, color.z, 0.45f };

		const PxVec3 centroid = (p[0] + p[1] + p[2]) / 3.f;
		vector<VertexPositionColor>& vertices = cellVertices[{ static_cast<int32>(floorf(centroid.x / STATIC_DEBUG_CELL_SIZE)),
			static_cast<int32>(floorf(centroid.z / STATIC_DEBUG_CELL_SIZE)) }];
		for (const PxVec3& point : p)
			vertices.emplace_back(XMFLOAT3{ point.x, point.y, point.z }, vertexColor);
	}

	vector<VertexPositionColor> allVertices;
	for (const auto& [cell, vertices] : cellVertices)
	{
		BoundingBox bounds{};
		BoundingBox::CreateFromPoints(bounds, vertices.size(), &vertices[0].position, sizeof(VertexPositionColor));
		outMesh.cells.push_back({ bounds, static_cast<uint32>(allVertices.size()), static_cast<uint32>(vertices.size()) });
		allVertices.insert(allVertices.end(), vertices.begin(), vertices.end());
	}
	if (allVertices.empty())
		return;

	D3D11_BUFFER_DESC bufferDesc{};
	bufferDesc.ByteWidth = static_cast<UINT>(allVertices.size() * sizeof(VertexPositionColor));
	bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	const D3D11_SUBRESOURCE_DATA initialData{ allVertices.data() };
	if (FAILED(GAME_INSTANCE->Get_Device()->CreateBuffer(&bufferDesc, &initialData, outMesh.vertexBuffer.GetAddressOf())))
		outMesh.cells.clear();
}

void PhysicsManager::Render_StaticDebug()
{
	DebugRenderer* debug = GAME_INSTANCE->Get_DebugRenderer();
	if (m_StaticActors.empty() || !debug)
		return;

	debug->Begin_Faces(GAME_INSTANCE->Get_Transform(D3DTS::VIEW), GAME_INSTANCE->Get_Transform(D3DTS::PROJ));
	for (const auto& [handle, actor] : m_StaticActors)
	{
		if (!actor)
			continue;
		const PxBounds3 actorBounds = actor->getWorldBounds();
		const PxVec3 center = actorBounds.getCenter();
		const PxVec3 extents = actorBounds.getExtents();
		if (!GAME_INSTANCE->Is_Visible(BoundingBox{ XMFLOAT3{ center.x, center.y, center.z }, XMFLOAT3{ extents.x, extents.y, extents.z } }))
			continue;

		auto found = m_StaticDebugMeshes.find(handle);
		if (found == m_StaticDebugMeshes.end())
		{
			found = m_StaticDebugMeshes.emplace(handle, STATIC_DEBUG_MESH{}).first;
			Build_StaticDebugMesh(actor, found->second);
		}
		const STATIC_DEBUG_MESH& mesh = found->second;
		if (!mesh.vertexBuffer)
			continue;

		for (const STATIC_DEBUG_CELL& cell : mesh.cells)
			if (GAME_INSTANCE->Is_Visible(cell.bounds))
				debug->Draw_Faces(mesh.vertexBuffer.Get(), cell.startVertex, cell.vertexCount);
	}
	debug->End_Faces();
}
#endif

void PhysicsManager::Release_Physics()
{
	Release_SoftBodySystem();

	// 컨트롤러는 자기 캡슐 액터를 씬에 두고 있으므로 가장 먼저 정리한다.
	for (auto& [handle, controller] : m_Controllers)
		if (controller) controller->release();
	m_Controllers.clear();

	for (auto& [handle, entry] : m_QueryActors)
	{
		if (!entry.actor) continue;
		if (m_Scene)
			m_Scene->removeActor(*entry.actor);
		entry.actor->release();
	}
	m_QueryActors.clear();

	// 액터와 메시를 씬보다 먼저 정리한다. 순서가 뒤집히면 종료 시 죽는다.
	for (auto& [handle, actor] : m_StaticActors)
	{
		if (!actor) continue;
		if (m_Scene)
			m_Scene->removeActor(*actor);
		actor->release();
	}
	m_StaticActors.clear();
#ifdef _DEBUG
	m_StaticDebugMeshes.clear();
#endif

	for (auto& [tag, mesh] : m_StaticMeshes)
		if (mesh) mesh->release();
	m_StaticMeshes.clear();

	// 생성의 역순으로 해제한다. PhysX는 소유 관계를 직접 관리하므로 순서가 곧 계약이다.
	if (m_ControllerManager) { m_ControllerManager->release(); m_ControllerManager = nullptr; }
	if (m_DefaultMaterial) { m_DefaultMaterial->release(); m_DefaultMaterial = nullptr; }
	if (m_Scene) { m_Scene->release(); m_Scene = nullptr; }
	if (m_Dispatcher) { m_Dispatcher->release(); m_Dispatcher = nullptr; }
	if (m_Physics) { m_Physics->release(); m_Physics = nullptr; }
	if (m_Foundation) { m_Foundation->release(); m_Foundation = nullptr; }
}

void PhysicsManager::On_Destroy() { Release_Physics(); }
void PhysicsManager::On_Disable() { EngineManager::On_Disable(); }
void PhysicsManager::On_Enable() { EngineManager::On_Enable(); }
void PhysicsManager::Set_Active(Bool isActive) { EngineManager::Set_Active(isActive); }

Unique<PhysicsManager> PhysicsManager::Create()
{
	auto manager = make_unique<PhysicsManager>();

	if (FAILED(manager->Initialize(nullptr)))
	{
		LOG_ERROR(L"[Physics] Failed to Initialize PhysicsManager");
		return nullptr;
	}

	return manager;
}
