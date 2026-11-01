#include "Light.h"

#include "Game.h"
#include "Shader.h"
#include "VIBuffer_Rect.h"

namespace
{
	/* Screen rectangle a point light can reach: the corners of its range box projected with the bound view's
	   camera, clamped to the bound viewport. False when the box is entirely off screen. A box reaching behind
	   the camera covers the whole view, because its projection is not bounded there. */
	Bool Compute_PointScissor(const LIGHT_DESC& desc, D3D11_RECT& rect)
	{
		UINT viewportCount = 1;
		D3D11_VIEWPORT viewport{};
		GAME_INSTANCE->Get_Context()->RSGetViewports(&viewportCount, &viewport);
		if (0 == viewportCount)
			return false;

		const Matrix viewProj = GAME_INSTANCE->Get_Transform(D3DTS::VIEW) * GAME_INSTANCE->Get_Transform(D3DTS::PROJ);
		Float minX = 1.f, minY = 1.f, maxX = -1.f, maxY = -1.f;
		Bool behind = false;
		for (uint32 corner = 0; corner < 8 && !behind; ++corner)
		{
			const Vector4 point{
				desc.position.x + ((corner & 1) ? desc.range : -desc.range),
				desc.position.y + ((corner & 2) ? desc.range : -desc.range),
				desc.position.z + ((corner & 4) ? desc.range : -desc.range), 1.f };
			const Vector4 clip = Vector4::Transform(point, viewProj);
			if (clip.w <= 1e-3f)
			{
				behind = true;
				break;
			}
			minX = min(minX, clip.x / clip.w); maxX = max(maxX, clip.x / clip.w);
			minY = min(minY, clip.y / clip.w); maxY = max(maxY, clip.y / clip.w);
		}
		if (behind)
		{
			minX = minY = -1.f;
			maxX = maxY = 1.f;
		}
		if (maxX < -1.f || minX > 1.f || maxY < -1.f || minY > 1.f)
			return false;

		// NDC y is up, screen y is down.
		rect.left = static_cast<LONG>(viewport.TopLeftX + (max(minX, -1.f) * 0.5f + 0.5f) * viewport.Width);
		rect.right = static_cast<LONG>(ceilf(viewport.TopLeftX + (min(maxX, 1.f) * 0.5f + 0.5f) * viewport.Width));
		rect.top = static_cast<LONG>(viewport.TopLeftY + (0.5f - min(maxY, 1.f) * 0.5f) * viewport.Height);
		rect.bottom = static_cast<LONG>(ceilf(viewport.TopLeftY + (0.5f - max(minY, -1.f) * 0.5f) * viewport.Height));
		return rect.right > rect.left && rect.bottom > rect.top;
	}
}

Light::Light()
{
}

HRESULT Light::Initialize_Prototype(const LIGHT_DESC& desc)
{
	memcpy(&m_LightDesc, &desc, sizeof(LIGHT_DESC));
	return S_OK;
}

HRESULT Light::Initialize(void* arg)
{
	return Object::Initialize(arg);
}

HRESULT Light::Render(const Shared<Shader>& shader, const Shared<VIBuffer_Rect>& buffer)
{
	/* DeferredShader pass index, not the light type: pass 0 is the Debug blit. */
	uint32 shaderPass{ ETOI(DEFERRED::DIRECTIONAL) };

	LightCB lightBuffer{};
	lightBuffer.direction = m_LightDesc.direction;
	lightBuffer.position = m_LightDesc.position;
	lightBuffer.diffuse = m_LightDesc.diffuse;
	lightBuffer.ambient = m_LightDesc.ambient;
	lightBuffer.specular = m_LightDesc.specular;
	lightBuffer.range = m_LightDesc.range;
	// The beam is part of the volumetric light, so Ctrl+F4 turns it off with the sun's shafts.
	lightBuffer.spot = Float3{ cosf(m_LightDesc.outerCone), cosf(m_LightDesc.innerCone),
		GAME_INSTANCE->Get_Volumetric() ? m_LightDesc.beam : 0.f };
	if (FAILED(shader->Bind_CBufferData(lightBuffer)))
		return E_FAIL;

	if (m_LightDesc.type == LIGHT::POINT || m_LightDesc.type == LIGHT::SPOT)
	{
		// The Point and Spot passes rasterize with scissor on, so the full-screen quad only shades the light's
		// rectangle; the range box holds a spot's cone too.
		D3D11_RECT scissor{};
		if (!Compute_PointScissor(m_LightDesc, scissor))
			return S_OK;
		GAME_INSTANCE->Get_Context()->RSSetScissorRects(1, &scissor);
		shaderPass = ETOI(m_LightDesc.type == LIGHT::POINT ? DEFERRED::POINT : DEFERRED::SPOT);
	}

	if (FAILED(shader->Begin(shaderPass)))
		return E_FAIL;

	return buffer->Render();
}

Shared<Light> Light::Create(const LIGHT_DESC& desc)
{
	auto light = make_shared<Light>();

	if (FAILED(light->Initialize_Prototype(desc)))
	{
		MSG_BOX("Failed to Created : Light");
		return nullptr;
	}
	if (FAILED(light->Initialize(nullptr)))
		{
		MSG_BOX("Failed to Created : Light");
		return nullptr;
	}

	return light;
}
