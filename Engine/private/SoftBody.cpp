#include "SoftBody.h"
#include "Game.h"
#include "Model.h"
#include "Random_Helper.h"
#include "SpdLogger.h"
#include "String_Helper.h"

#include <fstream>

namespace
{
	// 한 프레임에 이만큼 넘게 옮겨지면(컷 전환, 리스폰) 천을 끌고 가지 않고 새 자리에 다시 둔다.
	constexpr Float TELEPORT_DISTANCE{ 3.f };
	// NieR 천 값은 60fps 한 프레임 기준이다(속도 유지율, 프레임² 당 중력).
	constexpr Float SOURCE_FPS{ 60.f };
	constexpr Float GRAVITY{ 9.81f };
	// clw 바람 세기에 곱한다. 원작 값 그대로면 머리카락이 붕 떠서 절반으로 줄였다(2026-10-30 사용자 판정). Inspector Wind는 이 위에 곱한다.
	constexpr Float WIND_RATE{ 0.5f };
	// 바람 세기가 영역 출발점에서 바깥으로 이 파장(m)·속도(m/s)로 물결쳐 지나간다. 다발 안에서도 세기가 달라 빳빳하게 밀리지 않는다.
	constexpr Float WIND_WAVE_LENGTH{ 0.5f };
	constexpr Float WIND_WAVE_SPEED{ 0.8f };
	// 옆 마디 사이(치마 둘레)는 m_Stretchy만큼 늘어난다. 사슬 방향은 늘어나지 않는다.
	constexpr Float SIDE_COMPLIANCE_PER_STRETCH{ 1e-4f };
	constexpr uint32 SOLVER_ITERATIONS{ 5 };

	Vector3 Read_Vector3(const nlohmann::json& value)
	{
		return Vector3{ value.at(0).get<Float>(), value.at(1).get<Float>(), value.at(2).get<Float>() };
	}

	// 마디가 rotLimit 원뿔 안에서 돌 때 끝이 원래 자리에서 벗어나는 거리.
	Float Swing_Distance(Float length, Float rotLimit)
	{
		return 2.f * length * sinf(std::clamp(rotLimit, 0.f, XM_PIDIV2) * 0.5f);
	}
}

SoftBody::SoftBody() : Component{} {}
SoftBody::SoftBody(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: Component{ device, context } {}
SoftBody::SoftBody(const SoftBody& rhs)
	: Component(rhs), m_Simulation(rhs.m_Simulation), m_Damping(rhs.m_Damping), m_Gravity(rhs.m_Gravity),
	m_SwingLimit(rhs.m_SwingLimit), m_Follow(rhs.m_Follow), m_Wind(rhs.m_Wind) {}

SoftBody::~SoftBody()
{
	Release();
}

HRESULT SoftBody::Initialize_Prototype()
{
	return Component::Initialize_Prototype();
}

HRESULT SoftBody::Initialize(void* arg)
{
	return Component::Initialize(arg);
}

void SoftBody::On_Destroy()
{
	Release();
	Component::On_Destroy();
}

void SoftBody::Set_Simulation(Bool simulation)
{
	m_Simulation = simulation;
	// 다시 켜면 다음 Simulate가 지금 포즈에서 새로 만든다.
	Release();
	m_Built = false;
}

void SoftBody::Simulate(const Shared<Model>& model, const Matrix& worldMatrix, Float timeDelta,
	const Shared<Model>& colliderModel, const Matrix& colliderWorldMatrix)
{
	if (!m_Simulation || nullptr == model)
		return;

	const Shared<Model>& bodyModel = colliderModel ? colliderModel : model;
	const Matrix& bodyWorldMatrix = colliderModel ? colliderWorldMatrix : worldMatrix;
	if (!m_Built)
	{
		m_Built = true;
		if (FAILED(Build(model, worldMatrix, bodyModel, bodyWorldMatrix)))
			Release();
		m_LastPosition = worldMatrix.Translation();
		return;
	}
	if (m_Sets.empty())
		return;

	const Bool reset = Vector3::Distance(worldMatrix.Translation(), m_LastPosition) > TELEPORT_DISTANCE;
	m_LastPosition = worldMatrix.Translation();

	const SOFTBODY_TUNING tuning{ m_Damping, m_Gravity, m_SwingLimit, m_Follow };
	const Matrix toModel = worldMatrix.Invert();
	const Float frames = std::max(timeDelta, 0.f) * SOURCE_FPS;
	m_WindClock += frames;
	m_OverrideBones.clear();
	m_OverrideMatrices.clear();
	for (CHAIN_SET& chain : m_Sets)
	{
		m_Joints.resize(chain.bones.size());
		for (size_t joint = 0; joint < chain.bones.size(); ++joint)
			m_Joints[joint] = model->Get_BoneMatrix(chain.bones[joint]) * worldMatrix;
		for (const COLLIDER& collider : chain.colliders)
		{
			Vector3 from{}, to{};
			Get_ColliderSegment(bodyModel, bodyWorldMatrix, collider, from, to);
			GAME_INSTANCE->Move_SoftBodyCollider(collider.handle, from, to);
		}

		Update_Winds(chain, worldMatrix, frames);
		GAME_INSTANCE->Simulate_SoftBody(chain.handle, m_Joints, timeDelta, reset, tuning, m_Winds);
		GAME_INSTANCE->Get_SoftBodyPositions(chain.handle, m_Positions);
		if (m_Positions.size() != chain.vertexJoints.size())
			continue;

		// 마디는 시뮬레이션 자리로 옮기고, 클립에서 다음 정점을 향하던 축을 시뮬레이션의 다음 정점 쪽으로 돌린다.
		for (size_t joint = 0; joint < chain.bones.size(); ++joint)
		{
			Matrix world = m_Joints[joint];
			const Vector3 position = m_Positions[joint];
			const int32 child = chain.childVertex[joint];
			if (child >= 0)
			{
				Vector3 animated = Vector3::Transform(chain.vertexOffsets[child], m_Joints[chain.vertexJoints[child]]) - world.Translation();
				Vector3 simulated = m_Positions[child] - position;
				if (animated.LengthSquared() > 1e-10f && simulated.LengthSquared() > 1e-10f)
				{
					animated.Normalize();
					simulated.Normalize();
					world *= Matrix::CreateFromQuaternion(Quaternion::FromToRotation(animated, simulated));
				}
			}
			world.Translation(position);
			m_OverrideBones.push_back(chain.bones[joint]);
			m_OverrideMatrices.push_back(world * toModel);
		}
	}
	model->Override_BoneMatrices(m_OverrideBones, m_OverrideMatrices);
}

HRESULT SoftBody::Build(const Shared<Model>& model, const Matrix& worldMatrix, const Shared<Model>& colliderModel,
	const Matrix& colliderWorldMatrix)
{
	const filesystem::path path = filesystem::path(model->Get_ModelFilePath()).parent_path() /
		(model->Get_ModelTag() + L"_softbody.json");
	if (!filesystem::exists(path))
	{
		LOG_INFO(L"[SoftBody] {} has no soft body data ({})", model->Get_ModelTag(), path.filename().wstring());
		return E_FAIL;
	}

	nlohmann::json document;
	try
	{
		std::ifstream in(path);
		document = nlohmann::json::parse(in);
		// 콜라이더 뼈 이름은 json이 말하는 모델의 것이라, 다른 모델의 같은 이름 뼈에 붙지 않게 막는다.
		const wstring expected = Helper::To_wString(document.value("colliderModel", Helper::To_String(model->Get_ModelTag())));
		if (expected != colliderModel->Get_ModelTag())
		{
			LOG_ERROR(L"[SoftBody] {}: colliders are on {} but {} was given", path.filename().wstring(), expected,
				colliderModel->Get_ModelTag());
			return E_FAIL;
		}
		for (const nlohmann::json& setJson : document.at("sets"))
			if (FAILED(Build_Set(model, worldMatrix, colliderModel, colliderWorldMatrix, setJson)))
				return E_FAIL;
	}
	catch (const nlohmann::json::exception& exception)
	{
		LOG_ERROR(L"[SoftBody] {}: {}", path.filename().wstring(), Helper::To_wString(exception.what()));
		return E_FAIL;
	}

	size_t nodes = 0, colliders = 0;
	for (const CHAIN_SET& chain : m_Sets)
	{
		nodes += chain.bones.size();
		colliders += chain.colliders.size();
	}
	LOG_INFO(L"[SoftBody] {}: {} sets, {} nodes, {} colliders", model->Get_ModelTag(), m_Sets.size(), nodes, colliders);
	return S_OK;
}

HRESULT SoftBody::Build_Set(const Shared<Model>& model, const Matrix& worldMatrix, const Shared<Model>& colliderModel,
	const Matrix& colliderWorldMatrix, const nlohmann::json& setJson)
{
	const nlohmann::json& nodes = setJson.at("nodes");
	const uint32 numNodes = static_cast<uint32>(nodes.size());
	const wstring setName = Helper::To_wString(setJson.value("name", string{}));

	CHAIN_SET chain{};
	vector<Matrix> joints;
	vector<int32> ups, downs, sides;
	vector<Float> rotLimits;
	for (const nlohmann::json& node : nodes)
	{
		const string boneName = node.at("bone").get<string>();
		const int32 bone = model->Get_BoneIndexByName(boneName);
		if (bone < 0)
		{
			LOG_ERROR(L"[SoftBody] {}: bone {} not in model {}", setName, Helper::To_wString(boneName), model->Get_ModelTag());
			return E_FAIL;
		}
		chain.bones.push_back(static_cast<uint32>(bone));
		joints.push_back(model->Get_BoneMatrix(static_cast<uint32>(bone)) * worldMatrix);
		ups.push_back(node.at("up").get<int32>());
		downs.push_back(node.at("down").get<int32>());
		sides.push_back(node.at("side").get<int32>());
		rotLimits.push_back(node.at("rotLimit").get<Float>());
	}
	for (uint32 i = 0; i < numNodes; ++i)
		if (ups[i] >= static_cast<int32>(numNodes) || downs[i] >= static_cast<int32>(numNodes) || sides[i] >= static_cast<int32>(numNodes))
		{
			LOG_ERROR(L"[SoftBody] {}: node {} links out of {} nodes", setName, i, numNodes);
			return E_FAIL;
		}

	const Float headerFollow = setJson.value("originalRate", 0.f);
	SOFTBODY_DESC desc{};
	desc.vertexRadius = setJson.value("thickness", 0.f);
	desc.linearDamping = -SOURCE_FPS * logf(std::clamp(setJson.value("speedRate", 0.9f), 0.01f, 1.f));
	desc.gravityFactor = Read_Vector3(setJson.at("gravity")).Length() * SOURCE_FPS * SOURCE_FPS / GRAVITY;
	desc.iterations = SOLVER_ITERATIONS;

	// 마디 정점. 사슬 첫 마디는 관절에 붙고, 아래 마디의 한계 거리는 위 마디들의 원뿔을 따라 쌓인다.
	vector<Float> reach(numNodes, -1.f);
	std::function<Float(uint32)> Reach = [&](uint32 i) -> Float {
		if (reach[i] >= 0.f)
			return reach[i];
		reach[i] = 0.f;	// 고리가 있어도 끝난다
		const int32 up = ups[i];
		if (up >= 0)
			reach[i] = Reach(static_cast<uint32>(up)) +
				Swing_Distance(Vector3::Distance(joints[up].Translation(), joints[i].Translation()), rotLimits[up]);
		return reach[i];
	};
	for (uint32 i = 0; i < numNodes; ++i)
	{
		const nlohmann::json& node = nodes[i];
		const Float follow = node.value("originalRate", 0.f);
		SOFTBODY_DESC::VERTEX vertex{};
		vertex.joint = i;
		vertex.invMass = ups[i] < 0 ? 0.f : 1.f;
		vertex.maxDistance = Reach(i);
		vertex.followRate = follow > 0.f ? follow : headerFollow;
		desc.vertices.push_back(vertex);
		chain.vertexJoints.push_back(i);
		chain.vertexOffsets.push_back(Vector3::Zero);
		if (ups[i] >= 0)
			desc.edges.push_back({ { static_cast<uint32>(ups[i]), i }, 0.f });
	}

	// 끝 마디 뒤 끝점. 끝 마디가 어느 쪽을 향할지 정한다.
	chain.childVertex.assign(numNodes, -1);
	for (uint32 i = 0; i < numNodes; ++i)
	{
		if (downs[i] >= 0)
		{
			chain.childVertex[i] = downs[i];
			continue;
		}
		const Vector3 offset = Read_Vector3(nodes[i].at("offset"));
		const Float length = offset.Length();
		if (length <= 0.f)
			continue;
		SOFTBODY_DESC::VERTEX tip{ desc.vertices[i] };
		tip.offset = offset;
		tip.invMass = 1.f;
		tip.maxDistance = reach[i] + Swing_Distance(length, rotLimits[i]);
		const uint32 tipIndex = static_cast<uint32>(desc.vertices.size());
		desc.vertices.push_back(tip);
		desc.edges.push_back({ { i, tipIndex }, 0.f });
		chain.vertexJoints.push_back(i);
		chain.vertexOffsets.push_back(offset);
		chain.childVertex[i] = static_cast<int32>(tipIndex);
	}

	// 옆 마디(치마 둘레). 양쪽에서 같은 짝을 가리켜도 한 번만 잇는다.
	const Float sideCompliance = setJson.value("stretchy", 0.f) * SIDE_COMPLIANCE_PER_STRETCH;
	set<pair<uint32, uint32>> sidePairs;
	for (uint32 i = 0; i < numNodes; ++i)
		if (sides[i] >= 0 && sides[i] != static_cast<int32>(i) &&
			sidePairs.emplace(std::min<uint32>(i, sides[i]), std::max<uint32>(i, sides[i])).second)
			desc.edges.push_back({ { i, static_cast<uint32>(sides[i]) }, sideCompliance });

	chain.handle = GAME_INSTANCE->Create_SoftBody(desc, joints);
	if (0 == chain.handle)
	{
		LOG_ERROR(L"[SoftBody] {}: Jolt soft body was not created", setName);
		return E_FAIL;
	}

	for (const nlohmann::json& colliderJson : setJson.value("colliders", nlohmann::json::array()))
	{
		COLLIDER collider{};
		for (uint32 end = 0; end < 2; ++end)
		{
			const string boneName = colliderJson.at(end == 0 ? "bone1" : "bone2").get<string>();
			const int32 bone = colliderModel->Get_BoneIndexByName(boneName);
			if (bone < 0)
			{
				LOG_ERROR(L"[SoftBody] {}: collider bone {} not in model {}", setName, Helper::To_wString(boneName),
					colliderModel->Get_ModelTag());
				m_Sets.push_back(std::move(chain));
				return E_FAIL;
			}
			collider.bones[end] = static_cast<uint32>(bone);
			collider.offsets[end] = Read_Vector3(colliderJson.at(end == 0 ? "offset1" : "offset2"));
		}
		collider.weight = colliderJson.value("weight", 0.f);
		collider.capsule = colliderJson.value("capsule", false);

		Vector3 from{}, to{};
		Get_ColliderSegment(colliderModel, colliderWorldMatrix, collider, from, to);
		collider.handle = GAME_INSTANCE->Add_SoftBodyCollider(chain.handle, colliderJson.value("radius", 0.f), Vector3::Distance(from, to) * 0.5f);
		if (0 == collider.handle)
			continue;
		GAME_INSTANCE->Move_SoftBodyCollider(collider.handle, from, to);
		chain.colliders.push_back(collider);
	}

	for (const nlohmann::json& windJson : setJson.value("winds", nlohmann::json::array()))
	{
		// partsNo가 있으면 원작에서 한 부위에만 부는 바람인데, 그 번호가 어느 마디인지 아직 모른다. 전체에 불리지 않고 뺀다(A2 머리).
		if (windJson.value("partsNo", -1) >= 0)
			continue;
		WIND wind{};
		wind.from = Read_Vector3(windJson.at("offset"));
		wind.to = Read_Vector3(windJson.at("offset2"));
		wind.direction = windJson.contains("direction") ? Read_Vector3(windJson.at("direction")) : wind.to - wind.from;
		wind.radius = windJson.value("type", 1) == 0 ? windJson.value("radius", 0.f) : 0.f;
		wind.power = windJson.value("power", 0.f);
		wind.timer = windJson.value("timer", 0.f);
		wind.swingRate = windJson.value("swingRate", 0.f);
		wind.swingSpeed = windJson.value("swingSpeed", 0.f);
		wind.delayMax = windJson.value("delayMax", 0.f);
		wind.rotMax = windJson.value("rotMax", 0.f);
		wind.rotSpeed = windJson.value("rotSpeed", 0.f);
		// 같은 모델이 여럿이어도 같이 흔들리지 않게 위상과 돌풍 시작을 흩는다.
		wind.phase = static_cast<Float>(Helper::Random_Double(0.0, XM_2PI));
		wind.gustTime = static_cast<Float>(Helper::Random_Double(0.0, std::max(wind.timer, 0.f)));
		if (wind.power > 0.f && wind.direction.LengthSquared() > 1e-10f)
			chain.winds.push_back(wind);
	}

	m_Sets.push_back(std::move(chain));
	return S_OK;
}

void SoftBody::Update_Winds(CHAIN_SET& chain, const Matrix& worldMatrix, Float frames)
{
	m_Winds.clear();
	if (m_Wind <= 0.f)
		return;

	const Float scale = Vector3::TransformNormal(Vector3::UnitX, worldMatrix).Length();
	for (WIND& wind : chain.winds)
	{
		Float gust = 1.f;
		if (wind.timer > 0.f)
		{
			wind.gustTime += frames;
			while (wind.gustTime >= wind.timer + wind.gustPause)
			{
				wind.gustTime -= wind.timer + wind.gustPause;
				wind.gustPause = static_cast<Float>(Helper::Random_Double(0.0, std::max(wind.delayMax, 0.f)));
			}
			// 돌풍은 0에서 일어 0으로 잦아든다.
			const Float rise = wind.gustTime < wind.timer ? sinf(XM_PI * wind.gustTime / wind.timer) : 0.f;
			gust = rise * rise;
		}
		const Float strength = wind.power * gust *
			std::max(0.f, 1.f + wind.swingRate * sinf(wind.swingSpeed * m_WindClock + wind.phase));
		if (strength <= 0.f)
			continue;

		const Matrix turn = Matrix::CreateRotationY(wind.rotMax * sinf(wind.rotSpeed * m_WindClock + wind.phase));
		Vector3 direction = Vector3::TransformNormal(Vector3::TransformNormal(wind.direction, turn), worldMatrix);
		if (direction.LengthSquared() <= 1e-10f)
			continue;
		direction.Normalize();

		WIND_VOLUME out{};
		out.from = Vector3::Transform(wind.from, worldMatrix);
		out.to = Vector3::Transform(wind.to, worldMatrix);
		out.radius = wind.radius * scale;
		// clp 중력과 같은 프레임² 단위다.
		out.acceleration = direction * (strength * SOURCE_FPS * SOURCE_FPS * WIND_RATE * m_Wind);
		out.waveNumber = scale > 1e-6f ? XM_2PI / (WIND_WAVE_LENGTH * scale) : 0.f;
		out.wavePhase = XM_2PI * WIND_WAVE_SPEED / WIND_WAVE_LENGTH * (m_WindClock / SOURCE_FPS) + wind.phase;
		m_Winds.push_back(out);
	}
}

void SoftBody::Get_ColliderSegment(const Shared<Model>& model, const Matrix& worldMatrix, const COLLIDER& collider,
	Vector3& outFrom, Vector3& outTo) const
{
	const Vector3 first = Vector3::Transform(collider.offsets[0], model->Get_BoneMatrix(collider.bones[0]) * worldMatrix);
	const Vector3 second = Vector3::Transform(collider.offsets[1], model->Get_BoneMatrix(collider.bones[1]) * worldMatrix);
	const Vector3 point = Vector3::Lerp(first, second, collider.weight);
	outFrom = collider.capsule ? first : point;
	outTo = point;
}

void SoftBody::Release()
{
	for (const CHAIN_SET& chain : m_Sets)
	{
		GAME_INSTANCE->Release_SoftBody(chain.handle);
		for (const COLLIDER& collider : chain.colliders)
			GAME_INSTANCE->Remove_SoftBodyCollider(collider.handle);
	}
	m_Sets.clear();
}

Shared<Component> SoftBody::Clone(void* arg)
{
	auto clone = make_shared<SoftBody>(*this);
	if (FAILED(clone->Initialize(arg))) return nullptr;
	return clone;
}

Shared<SoftBody> SoftBody::CreatePrototype()
{
	auto instance = make_shared<SoftBody>();
	if (FAILED(instance->Initialize_Prototype())) return nullptr;
	return instance;
}

Shared<SoftBody> SoftBody::Create(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context)
{
	auto instance = make_shared<SoftBody>(device, context);
	if (FAILED(instance->Initialize_Prototype())) return nullptr;
	return instance;
}
