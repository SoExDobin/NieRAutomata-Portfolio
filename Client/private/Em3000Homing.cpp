#include "pch.h"
#include "Em3000Homing.h"

#include <Game.h>
#include <Shader.h>
#include <Texture.h>

#include "Entity.h"
#include "ExplodeEffect.h"

Em3000Homing::Em3000Homing(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: GameObject{ device, context } {}
Em3000Homing::Em3000Homing(const Em3000Homing& rhs) : GameObject{ rhs } {}

HRESULT Em3000Homing::Initialize_Prototype()
{
	// Pl0000::TryEvade refuses a dash evasion when the attacker is on the Bullet layer, so this attack hands itself in as
	// the attacker: it has to be avoided by moving or jumping, not dodged.
	m_LayerMask.Set_Layer(L"Bullet");
	return GameObject::Initialize_Prototype();
}

HRESULT Em3000Homing::Initialize(void* arg)
{
	if (!arg || FAILED(GameObject::Initialize(arg))) return E_FAIL;
	const auto& desc = *static_cast<EM3000_HOMING_DESC*>(arg);
	m_Target = desc.target;
	m_Position = desc.position;
	m_GroundY = desc.groundY;
	m_Velocity = desc.direction;
	m_Velocity.Normalize();
	m_Velocity *= RiseSpeed;
	m_Trail.push_front(m_Position);

	Shader::SHADER_DESC shaderDesc{ L"VTX_Mesh_Effect", VTXMESH::Elements, VTXMESH::numElements };
	m_Shader = Add_Component<Shader>(ETOI(LEVEL::STATIC), &shaderDesc);
	Texture::TEXTURE_DESC textureDesc{ Get_LevelIndex(), L"Effect_Em3000" };
	m_Texture = Add_Component<Texture>(Get_LevelIndex(), &textureDesc);
	if (!m_Shader || !m_Texture) return E_FAIL;

	D3D11_BUFFER_DESC bufferDesc{};
	bufferDesc.ByteWidth = sizeof(VTXMESH) * VertexCapacity;
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	return m_Device->CreateBuffer(&bufferDesc, nullptr, m_VertexBuffer.GetAddressOf());
}

void Em3000Homing::Update(Float timeDelta)
{
	if (m_Exploded) return;
	m_Age += timeDelta;

	const Shared<GameObject> target = m_Target.lock();
	const Vector3 aim = target ? target->Get_Transform()->Get_Position() + Vector3{ 0.f, ChestHeight, 0.f } : Vector3::Zero;
	if (m_Age > RiseTime && target)
	{
		// Turn toward the chest by at most TurnRate this frame, speeding up to the homing speed. The shot must also sink
		// a little more steeply every moment it homes, so one the player keeps dodging ends up in the floor.
		Vector3 heading = m_Velocity;
		heading.Normalize();
		Vector3 wanted = aim - m_Position;
		wanted.Normalize();
		const Float angle = acosf(std::clamp(heading.Dot(wanted), -1.f, 1.f));
		const Float turn = std::min(1.f, TurnRate * timeDelta / std::max(angle, 1e-4f));
		Vector3 next = Vector3::Lerp(heading, wanted, turn);
		next.Normalize();
		const Float sink = std::min(MaxSink, SinkGrowth * (m_Age - RiseTime));
		if (next.y > -sink)
		{
			const Float flat = sqrtf(1.f - sink * sink);
			Vector3 ahead{ next.x, 0.f, next.z };
			if (ahead.LengthSquared() < 1e-6f) ahead = Vector3{ heading.x, 0.f, heading.z };
			ahead.Normalize();
			next = ahead * flat + Vector3{ 0.f, -sink, 0.f };
		}
		const Float speed = std::min(HomingSpeed, m_Velocity.Length() + HomingSpeed * timeDelta);
		m_Velocity = next * speed;
	}
	m_Position += m_Velocity * timeDelta;

	m_TrailTimer += timeDelta;
	if (m_TrailTimer >= TrailStep)
	{
		m_TrailTimer = 0.f;
		m_Trail.push_front(m_Position);
		if (m_Trail.size() > TrailPoints) m_Trail.pop_back();
	}
	else
		m_Trail.front() = m_Position;

	if (m_Position.y <= m_GroundY)
		Explode(Vector3{ m_Position.x, m_GroundY, m_Position.z });
	else if (target && Vector3::Distance(m_Position, aim) <= ContactRadius)
		Explode(m_Position);
	else if (m_Age >= Lifetime)
		Explode(m_Position);
}

void Em3000Homing::Explode(const Vector3& position)
{
	m_Exploded = true;

	ExplodeEffect::EXPLODE_EFFECT_DESC burst{};
	burst.position = position;
	burst.size = BlastSize;
	GAME_INSTANCE->Instantiate<ExplodeEffect>(L"ExplodeEffect", Get_LevelIndex(), &burst);
	GAME_INSTANCE->PlaySound3D(L"em3000_homing_laser_explo", BlastVolume, position, BlastNear, BlastFar, false);

	const Shared<Entity> target = Cast<Entity>(m_Target.lock());
	if (target && Vector3::Distance(target->Get_Transform()->Get_Position() + Vector3{ 0.f, ChestHeight, 0.f }, position)
		<= BlastRadius + ChestHeight)
	{
		Entity::DAMAGE_INFO damage{};
		damage.attacker = shared_from_this();
		damage.attackType = ATK_TYPE::LIGHT;
		damage.damage = Damage;
		damage.hitPosition = position;
		target->TakeDamage(damage);
	}
	GAME_INSTANCE->Destroy(Get_ObjectGuid());
}

void Em3000Homing::Late_Update(Float timeDelta)
{
	m_TrailVertices = 0;
	if (m_Exploded) return;

	const Matrix cameraWorld = GAME_INSTANCE->Get_Transform(D3DTS::VIEW).Invert();
	const Vector3 camera = cameraWorld.Translation();
	Vector3 right = cameraWorld.Right(), up = cameraWorld.Up();
	right.Normalize();
	up.Normalize();

	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (FAILED(m_Context->Map(m_VertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;
	auto* vertices = static_cast<VTXMESH*>(mapped.pData);
	const auto put = [](VTXMESH* quad, const Vector3 (&corners)[4], const Vector2 (&uvs)[4], const Vector3& tint0,
		const Vector3& tint1)
	{
		constexpr uint32 order[6] = { 0, 2, 1, 1, 2, 3 };
		for (uint32 v = 0; v < 6; ++v)
		{
			quad[v] = {};
			quad[v].position = corners[order[v]];
			quad[v].normal = order[v] < 2 ? tint0 : tint1;
			quad[v].texcoord = uvs[order[v]];
			quad[v].texcoord1 = Vector2{ 1.f, 0.f };
		}
	};

	// The trail: a camera-facing ribbon that narrows and fades toward its tail.
	const uint32 points = static_cast<uint32>(m_Trail.size());
	for (uint32 i = 0; i + 1 < points; ++i)
	{
		const Vector3& a = m_Trail[i];
		const Vector3& b = m_Trail[i + 1];
		Vector3 along = b - a;
		if (along.LengthSquared() < 1e-8f) continue;
		Vector3 side = along.Cross(camera - a);
		if (side.LengthSquared() < 1e-8f) continue;
		side.Normalize();
		const Float fadeA = 1.f - static_cast<Float>(i) / (TrailPoints - 1);
		const Float fadeB = 1.f - static_cast<Float>(i + 1) / (TrailPoints - 1);
		const Float halfA = TrailWidth * 0.5f * fadeA, halfB = TrailWidth * 0.5f * fadeB;
		const Vector3 corners[4] = { a + side * halfA, a - side * halfA, b + side * halfB, b - side * halfB };
		const Vector2 uvs[4] = { { 0.5f, 0.f }, { 0.5f, 1.f }, { 0.5f, 0.f }, { 0.5f, 1.f } };
		put(vertices + m_TrailVertices, corners, uvs, TrailTint * fadeA, TrailTint * fadeB);
		m_TrailVertices += 6;
	}

	// The head: a glow and a slowly turning star flare.
	const auto billboard = [&](VTXMESH* quad, Float size, Float spin, const Vector3& tint)
	{
		const Float c = cosf(spin) * size * 0.5f, s = sinf(spin) * size * 0.5f;
		const Vector3 r = right * c + up * s, u = up * c - right * s;
		const Vector3 corners[4] = { m_Position - r + u, m_Position - r - u, m_Position + r + u, m_Position + r - u };
		const Vector2 uvs[4] = { { 0.f, 0.f }, { 0.f, 1.f }, { 1.f, 0.f }, { 1.f, 1.f } };
		put(quad, corners, uvs, tint, tint);
	};
	billboard(vertices + HeadFirst, HeadSize, 0.f, HeadTint);
	billboard(vertices + FlareFirst, FlareSize, m_Age * 1.5f, FlareTint);
	m_Context->Unmap(m_VertexBuffer.Get(), 0);
}

HRESULT Em3000Homing::Render()
{
	if (m_Exploded) return S_OK;
	const Matrix identity = Matrix::Identity;
	if (FAILED(m_Shader->Bind_Matrix(WorldMatrix, &identity)) ||
		FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ViewMatrix, D3DTS::VIEW)) ||
		FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ProjMatrix, D3DTS::PROJ))) return E_FAIL;

	ID3D11Buffer* buffer = m_VertexBuffer.Get();
	const uint32 stride = sizeof(VTXMESH), offset = 0;
	m_Context->IASetVertexBuffers(0, 1, &buffer, &stride, &offset);
	m_Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	const struct { uint32 image, first, count; } draws[] = {
		{ TrailImage, 0, m_TrailVertices }, { HeadImage, HeadFirst, 6 }, { FlareImage, FlareFirst, 6 } };
	for (const auto& draw : draws)
	{
		if (draw.count == 0) continue;
		if (FAILED(m_Texture->Bind_ShaderResourceView(m_Shader, "g_EffectMap", draw.image)) ||
			FAILED(m_Shader->Begin("Additive"))) return E_FAIL;
		m_Context->Draw(draw.count, draw.first);
	}
	return S_OK;
}

void Em3000Homing::Submit_RenderGroup()
{
	if (!m_Exploded) GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::WORLDUI, shared_from_this());
}

Shared<Em3000Homing> Em3000Homing::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<Em3000Homing>(device, context);
	return SUCCEEDED(prototype->Initialize_Prototype()) ? prototype : nullptr;
}

Shared<GameObject> Em3000Homing::Clone(void* arg)
{
	auto instance = make_shared<Em3000Homing>(*this);
	return SUCCEEDED(instance->Initialize(arg)) ? instance : nullptr;
}
