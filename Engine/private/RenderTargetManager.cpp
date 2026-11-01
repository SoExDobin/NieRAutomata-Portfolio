#include "RenderTargetManager.h"

#include "Game.h"
#include "RenderTarget.h"

RenderTargetManager::RenderTargetManager(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: m_Device{device}, m_Context{context} {}

RenderTargetManager::~RenderTargetManager()
{
	m_RenderTargets.clear();

	for (auto& rtList : m_MultiRenderTargets) {
		rtList.second.clear();
	}

	m_MultiRenderTargets.clear();
}

HRESULT RenderTargetManager::Add_RenderTarget(const wstring& rtTag, uint32 sizeX, uint32 sizeY, DXGI_FORMAT pixelFormat, const Color& clearColor)
{
	if (nullptr != Find_RenderTarget(rtTag))
		return E_FAIL;

	auto renderTarget = RenderTarget::Create(m_Device, m_Context, sizeX, sizeY, pixelFormat, clearColor);
	if (nullptr == renderTarget)
		return E_FAIL;

	renderTarget->Set_DebugName(rtTag);
	m_RenderTargets.emplace(rtTag, renderTarget);

	return S_OK;
}

HRESULT RenderTargetManager::Bind_ShaderResource(const Shared<Shader>& shader, const Char* constantName, const wstring& rtTag)
{
	auto renderTarget = Find_RenderTarget(rtTag);
	if (nullptr == renderTarget)
		return E_FAIL;

	return renderTarget->Bind_ShaderResource(shader, constantName);
}

HRESULT RenderTargetManager::Copy_BoundTarget(const wstring& rtTag)
{
	auto renderTarget = Find_RenderTarget(rtTag);
	if (nullptr == renderTarget)
		return E_FAIL;

	ComPtr<ID3D11RenderTargetView> bound;
	m_Context->OMGetRenderTargets(1, bound.GetAddressOf(), nullptr);
	if (nullptr == bound)
		return E_FAIL;
	ComPtr<ID3D11Resource> source;
	bound->GetResource(source.GetAddressOf());
	return renderTarget->Copy_From(source.Get());
}

HRESULT RenderTargetManager::Add_MultiRenderTarget(const wstring& mrtTag, const wstring& rtTag)
{
	auto renderTarget = Find_RenderTarget(rtTag);
	if (!renderTarget)
		return E_FAIL;

	auto mrtList = Find_MultiRenderTarget(mrtTag);
	
	if (!mrtList)
	{
		list<wstring> newList;
		newList.push_back(rtTag);
		m_MultiRenderTargets.emplace(mrtTag, newList);
	}
	else
	{
		mrtList->push_back(rtTag);
	}

	return S_OK;
}

HRESULT RenderTargetManager::Begin_MultiRenderTarget(const wstring& mrtTag)
{
	auto mrtList = Find_MultiRenderTarget(mrtTag);
	if (!mrtList)
		return E_FAIL;

	if (mrtList->size() > D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT || m_BackBuffer)
		return E_FAIL;
	ID3D11ShaderResourceView* nullViews[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};
	m_Context->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullViews);
	m_Context->OMGetRenderTargets(1, m_BackBuffer.ReleaseAndGetAddressOf(), m_OriginalDepthStencil.ReleaseAndGetAddressOf());

	ID3D11RenderTargetView* renderTargets[8] = { nullptr };
	uint32 numRenderTargets{ 0 };

	for (const auto& tag : *mrtList)
	{
		auto renderTarget = Find_RenderTarget(tag);
		renderTarget->Clear_RenderTarget();
		renderTargets[numRenderTargets++] = renderTarget->Get_RenderTargetView().Get();
	}

	m_Context->OMSetRenderTargets(numRenderTargets, renderTargets, m_OriginalDepthStencil.Get());

	return S_OK;
}

HRESULT RenderTargetManager::End_MultiRenderTarget()
{
	m_Context->OMSetRenderTargets(1, m_BackBuffer.GetAddressOf(), m_OriginalDepthStencil.Get());

	m_BackBuffer.Reset();
	m_OriginalDepthStencil.Reset();

	return S_OK;
}

HRESULT RenderTargetManager::Prepare_View(uint32 screenIndex, uint32 width, uint32 height)
{
	const HRESULT result = Resize_View(screenIndex, width, height);
	if (FAILED(result)) return result;
	m_ActiveScreen = screenIndex;
	return S_OK;
}

HRESULT RenderTargetManager::Resize_View(uint32 screenIndex, uint32 width, uint32 height)
{
	if (width == 0 || height == 0 || m_BackBuffer) return E_INVALIDARG;
	auto found = m_ViewRenderTargets.find(screenIndex);
	if (found == m_ViewRenderTargets.end() || found->second.size() != m_RenderTargets.size() ||
		(!found->second.empty() && !found->second.begin()->second->Has_Size(width, height)))
	{
		map<const wstring, Shared<RenderTarget>> replacements;
		for (const auto& [tag, source] : m_RenderTargets)
		{
			auto target = source->Create_Resized(width, height);
			if (!target) return E_FAIL;
			target->Set_DebugName(tag);
			replacements.emplace(tag, std::move(target));
#ifdef _DEBUG
			// Fail with a partially built batch, not at argument validation.
			if (m_FailNextViewCreation) {
				m_FailNextViewCreation = false;
				return E_FAIL;
			}
#endif
		}
		m_ViewRenderTargets[screenIndex].swap(replacements);
	}
	return S_OK;
}

list<wstring>* RenderTargetManager::Find_MultiRenderTarget(const wstring& mrtTag)
{
	if (m_MultiRenderTargets.contains(mrtTag))
	{
		return &m_MultiRenderTargets[mrtTag];
	}

	return nullptr;
}

Shared<RenderTarget> RenderTargetManager::Find_RenderTarget(const wstring& rtTag)
{
	const auto view = m_ViewRenderTargets.find(m_ActiveScreen);
	if (view != m_ViewRenderTargets.end())
	{
		const auto target = view->second.find(rtTag);
		return target == view->second.end() ? nullptr : target->second;
	}
	if (m_RenderTargets.contains(rtTag))
		return m_RenderTargets[rtTag];

	return nullptr;
}

Unique<RenderTargetManager> RenderTargetManager::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto instance = make_unique<RenderTargetManager>(device, context);

	if (FAILED(instance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : RenderTargetManager");
		return nullptr;
	}

	return instance;
}

#ifdef _DEBUG
HRESULT RenderTargetManager::Ready_RenderTarget_Debug(const wstring& rtTag, Float x, Float y, Float sizeX, Float sizeY)
{
	Shared<RenderTarget> renderTarget = Find_RenderTarget(rtTag);
	if (nullptr == renderTarget)
		return E_FAIL;

	return renderTarget->Ready_Debug(x, y, sizeX, sizeY);
}

HRESULT RenderTargetManager::Render_RenderTarget_Debug(const Shared<VIBuffer_Rect>& buffer, const Shared<Shader>& shader, const wstring& mrtTag)
{
	list<wstring>* mrtList = Find_MultiRenderTarget(mrtTag);
	if (nullptr == mrtList)
		return E_FAIL;

	for (const auto& tag : *mrtList)
	{
		auto renderTarget = Find_RenderTarget(tag);
		if (FAILED(renderTarget->Render_Debug(buffer, shader)))
		{
			return E_FAIL;
		}
	}

	return S_OK;
}
#endif
