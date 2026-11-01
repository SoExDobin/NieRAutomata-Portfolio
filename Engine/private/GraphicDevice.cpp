#include "GraphicDevice.h"
#include "Game.h"
#include "SpdLogger.h"
#include <dxgi1_6.h>

namespace {
// 모든 실행의 프레임 상한(75 FPS). 노트북 GPU는 144에서 계속 100%로 돌다 열 제한에 걸리므로 낮춘다. tearing Present는 주사율에 묶이지 않으므로 직접 간격을 맞춘다.
constexpr std::chrono::nanoseconds FramePeriod{1'000'000'000 / 75};
} // namespace

GraphicDevice::~GraphicDevice() {
  if (m_FrameTimer)
    CloseHandle(m_FrameTimer);
}

HRESULT GraphicDevice::Initialize(const ENGINE_DESC &engineDesc) {
  uint32 flag = {};

#ifdef _DEBUG
  flag |= D3D11_CREATE_DEVICE_DEBUG;
#endif

  // 내장·외장 GPU가 함께 있으면 기본 어댑터가 내장 GPU로 잡히므로 고성능 GPU를
  // 고른다. 찾지 못하면 기본 어댑터를 쓴다.
  ComPtr<IDXGIAdapter1> adapter;
  ComPtr<IDXGIFactory6> factory;
  if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(factory.GetAddressOf()))))
    factory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                        IID_PPV_ARGS(adapter.GetAddressOf()));

  D3D_FEATURE_LEVEL feature_Level = {};
  if (FAILED(D3D11CreateDevice(adapter.Get(),
                               adapter ? D3D_DRIVER_TYPE_UNKNOWN
                                       : D3D_DRIVER_TYPE_HARDWARE,
                               nullptr, flag, nullptr, 0, D3D11_SDK_VERSION,
                               m_Device.GetAddressOf(), &feature_Level,
                               m_Context.GetAddressOf()))) {
    return E_FAIL;
  }

  // 1 ms보다 정밀하게 깨어나는 타이머(Windows 10 1803+). 없으면 Present 앞 대기를 돌면서 한다.
  m_FrameTimer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);

  if (FAILED(Ready_SwapChain(engineDesc.hWnd, engineDesc.winMode,
                             engineDesc.viewportWidth,
                             engineDesc.viewportHeight)))
    return E_FAIL;

  if (FAILED(Ready_BackBufferRenderTargetView()))
    return E_FAIL;

  if (FAILED(Ready_DepthStencilView(engineDesc.viewportWidth,
                                    engineDesc.viewportHeight)))
    return E_FAIL;

  { // Set Viewport
    ComPtr<ID3D11RenderTargetView> RTVs[1] = {m_RTV};
    m_Context->OMSetRenderTargets(1, RTVs[0].GetAddressOf(), m_DSV.Get());

    m_ViewPort.TopLeftX = 0.f;
    m_ViewPort.TopLeftY = 0.f;
    m_ViewPort.Width = static_cast<Float>(engineDesc.viewportWidth);
    m_ViewPort.Height = static_cast<Float>(engineDesc.viewportHeight);
    m_ViewPort.MinDepth = 0.f;
    m_ViewPort.MaxDepth = 1.f;

    m_Context->RSSetViewports(1, &m_ViewPort);
  }

  if (engineDesc.useOffscreenRendering && engineDesc.renderTargetCount > 0) {
    for (uint32 i = 0; i < engineDesc.renderTargetCount; ++i) {

      if (FAILED(Create_OffScreenTarget(engineDesc.viewportWidth,
                                        engineDesc.viewportHeight)))
        return E_FAIL;
    }
  }

  return S_OK;
}

void GraphicDevice::On_Destroy() {
  if (m_SwapChain) {
      m_SwapChain->SetFullscreenState(FALSE, nullptr);
  }
  
  // D3D11 Context가 지연 삭제용으로 들고 있는 타겟 뷰들의 레퍼런스 카운트를 강제로 비워냅니다.
  if (m_Context) {
      m_Context->ClearState();
      m_Context->Flush();
  }

  m_Offscreens.clear();
  Object::On_Destroy();
}

HRESULT
GraphicDevice::Clear_BackBufferView(const Shared<Float4> &clearColor) const {
  if (nullptr == m_Context)
    return E_FAIL;

  m_Context->ClearRenderTargetView(
      m_RTV.Get(), reinterpret_cast<const Float *>(clearColor.get()));

  return S_OK;
}

HRESULT GraphicDevice::Clear_DepthStencilView() const {
  if (nullptr == m_Context)
    return E_FAIL;

  m_Context->ClearDepthStencilView(m_DSV.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.f, 0);

  return S_OK;
}

void GraphicDevice::Set_DepthStencilState(ID3D11DepthStencilState* state, UINT ref)
{
    m_Context->OMSetDepthStencilState(state, ref);
}

HRESULT GraphicDevice::Present() const {
    if (nullptr == m_SwapChain)
      return E_FAIL;

    m_Context->OMSetRenderTargets(0, nullptr, nullptr);

    // 다음 차례까지 기다린다. 타이머로 0.5 ms 전까지 자고 나머지는 돈다. 늦은 프레임은 한 프레임만 따라잡는다.
    using Clock = std::chrono::steady_clock;
    const auto remaining = m_NextPresent - Clock::now();
    if (remaining > std::chrono::microseconds(500) && m_FrameTimer) {
      LARGE_INTEGER due{};
      due.QuadPart = -static_cast<LONGLONG>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                                remaining - std::chrono::microseconds(500)).count() / 100);
      if (SetWaitableTimerEx(m_FrameTimer, &due, 0, nullptr, nullptr, nullptr, 0))
        WaitForSingleObject(m_FrameTimer, INFINITE);
    }
    while (Clock::now() < m_NextPresent)
      YieldProcessor();
    m_NextPresent = (std::max)(m_NextPresent, Clock::now() - FramePeriod) + FramePeriod;

    // 독점 전체 화면(Alt+Enter 포함)에서는 tearing 플래그를 줄 수 없다.
    BOOL fullscreen = FALSE;
    const Bool tearing = m_AllowTearing && SUCCEEDED(m_SwapChain->GetFullscreenState(&fullscreen, nullptr)) && !fullscreen;
    return m_SwapChain->Present(0, tearing ? DXGI_PRESENT_ALLOW_TEARING : 0);
}

HRESULT GraphicDevice::OnResize(uint32 width, uint32 height, uint32 screenIndex) {
	if (nullptr == m_SwapChain || nullptr == m_Device)
		return E_FAIL;

	if (width == 0 || height == 0)
		return S_OK;
	if (width > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION || height > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION)
		return E_INVALIDARG;

    if (screenIndex != UINT_MAX) {
        OffscreenRenderTarget replacement{};
        const HRESULT result = Prepare_OffscreenResize(width, height, screenIndex, replacement);
        if (FAILED(result)) return result;
        if (result == S_OK) Commit_OffscreenResize(screenIndex, std::move(replacement));
        return S_OK;
    }
    m_Context->OMSetRenderTargets(0, nullptr, nullptr);
    m_RTV.Reset();
    m_DSV.Reset();
    m_Context->ClearState();
    m_Context->Flush();

    if (FAILED( m_SwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN,
        m_AllowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0)))
      return E_FAIL;

    if (FAILED(Ready_BackBufferRenderTargetView()))
      return E_FAIL;

    if (FAILED(Ready_DepthStencilView(width, height)))
      return E_FAIL;

    m_ViewPort.Width = static_cast<Float>(width);
    m_ViewPort.Height = static_cast<Float>(height);

    ID3D11RenderTargetView *rtvs[] = {m_RTV.Get()};
    m_Context->OMSetRenderTargets(1, rtvs, m_DSV.Get());
    m_Context->RSSetViewports(1, &m_ViewPort);
    m_ActiveScreen = UINT_MAX;

    return S_OK;
}

HRESULT GraphicDevice::Prepare_OffscreenResize(uint32 width, uint32 height, uint32 screenIndex,
    OffscreenRenderTarget& replacement)
{
    if (!m_Device || !m_SwapChain) return E_FAIL;
    if (screenIndex >= m_Offscreens.size() || width == 0 || height == 0 ||
        width > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION || height > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION)
        return E_INVALIDARG;
    const auto& target = m_Offscreens[screenIndex];
    if (target.viewport.Width == width && target.viewport.Height == height) return S_FALSE;
    return Create_OffScreenTarget(width, height, replacement);
}

void GraphicDevice::Commit_OffscreenResize(uint32 screenIndex, OffscreenRenderTarget&& replacement)
{
    // All fallible work (including the matching MRT batch) must precede this call.
    m_Context->OMSetRenderTargets(0, nullptr, nullptr);
    ID3D11ShaderResourceView* nullViews[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};
    m_Context->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullViews);
    m_Offscreens[screenIndex] = std::move(replacement);
}

HRESULT GraphicDevice::Begin_RenderOffScreen(uint32 screenIndex) {
  if (screenIndex >= m_Offscreens.size()) {
    LOG_CRITICAL(L"Invalid Offscreen Index: {}", screenIndex);
    return E_FAIL;
  }

  ID3D11ShaderResourceView *nullSRV[1] = {nullptr};
  m_Context->PSSetShaderResources(0, 1, nullSRV);

  auto &rt = m_Offscreens[screenIndex];

  Float clearColor[4] = { 0.f, 0.f, 0.f, 1.f };
  m_Context->ClearRenderTargetView(rt.RTV.Get(), clearColor);

  m_Context->ClearDepthStencilView(
      rt.DSV.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.f, 0.f);
  ID3D11RenderTargetView *RTVs[] = {rt.RTV.Get()};
  m_Context->OMSetRenderTargets(1, RTVs, rt.DSV.Get()); // rt.DSV 사용!
  m_Context->RSSetViewports(1, &rt.viewport);
  m_ActiveScreen = screenIndex;

  return S_OK;
}

HRESULT GraphicDevice::End_RenderOffScreen() {
  m_ActiveScreen = UINT_MAX;
  ID3D11RenderTargetView *RTVs[] = {m_RTV.Get()};
  m_Context->OMSetRenderTargets(1, RTVs, m_DSV.Get());
  m_Context->RSSetViewports(1, &m_ViewPort);

  return S_OK;
}

ComPtr<ID3D11ShaderResourceView>
GraphicDevice::Get_OffscreenSRV(uint32 screenIndex) {
  if (screenIndex >= m_Offscreens.size()) {
    LOG_CRITICAL(L"Invalid Offscreen Index: {}", screenIndex);
    return nullptr;
  }

  return m_Offscreens[screenIndex].SRV;
}

HRESULT GraphicDevice::Ready_SwapChain(HWND hWnd, WINMODE isWindowed,
                                       uint32 winSizeX, uint32 winSizeY) {
#ifdef _DEBUG
  // Keep debug sessions in a normal desktop window so Visual Studio can take
  // focus immediately when the native debugger breaks on an exception.
  isWindowed = WINMODE::WIN;
#endif

  ComPtr<IDXGIDevice> device = {nullptr};
  if (FAILED(m_Device->QueryInterface(
          __uuidof(IDXGIDevice),
          reinterpret_cast<void **>(device.GetAddressOf()))))
    return E_FAIL;

  ComPtr<IDXGIAdapter> adapter = {nullptr};
  if (FAILED(
          device->GetParent(__uuidof(IDXGIAdapter),
                            reinterpret_cast<void **>(adapter.GetAddressOf()))))
    return E_FAIL;

  ComPtr<IDXGIFactory2> factory2 = {nullptr};
  if (FAILED(adapter->GetParent(
          __uuidof(IDXGIFactory2),
          reinterpret_cast<void **>(factory2.GetAddressOf()))))
    return E_FAIL;

  DXGI_SWAP_CHAIN_DESC1 swapChain = {};
  ZeroMemory(&swapChain, sizeof(DXGI_SWAP_CHAIN_DESC1));
  {
    swapChain.Width = winSizeX;
    swapChain.Height = winSizeY;
    swapChain.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChain.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChain.BufferCount = 2;
    swapChain.SampleDesc.Quality = 0;
    swapChain.SampleDesc.Count = 1;
    swapChain.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChain.Scaling = DXGI_SCALING_STRETCH;
    swapChain.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
  }

  // 창 모드만: 독점 전체 화면은 tearing 스왑체인을 쓰지 않아도 Present(0)이 주사율에 묶이지 않는다.
  ComPtr<IDXGIFactory5> factory5;
  BOOL tearingSupported = FALSE;
  m_AllowTearing = WINMODE::WIN == isWindowed && SUCCEEDED(factory2.As(&factory5)) &&
                   SUCCEEDED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                                                          &tearingSupported, sizeof(tearingSupported))) &&
                   tearingSupported;
  if (m_AllowTearing)
    swapChain.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

  DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullScreenSwapChain = {};
  fullScreenSwapChain.RefreshRate = {.Numerator = 60, .Denominator = 1};
  fullScreenSwapChain.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
  fullScreenSwapChain.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
  fullScreenSwapChain.Windowed = static_cast<Bool>(isWindowed);

#ifdef _DEBUG
  if (FAILED(factory2->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER)))
    return E_FAIL;
#endif

  return factory2->CreateSwapChainForHwnd(m_Device.Get(), hWnd, &swapChain,
                                          &fullScreenSwapChain, nullptr,
                                          m_SwapChain.ReleaseAndGetAddressOf());
}

HRESULT GraphicDevice::Ready_BackBufferRenderTargetView() {
  if (nullptr == m_Device)
    return E_FAIL;

  ComPtr<ID3D11Texture2D> backBufferTexture = {nullptr};

  if (FAILED(m_SwapChain->GetBuffer(
          0, __uuidof(ID3D11Texture2D),
          reinterpret_cast<void **>(backBufferTexture.GetAddressOf()))))
    return E_FAIL;
  if (FAILED(m_Device->CreateRenderTargetView(backBufferTexture.Get(), nullptr,
                                              m_RTV.GetAddressOf())))
    return E_FAIL;

  return S_OK;
}

HRESULT GraphicDevice::Ready_DepthStencilView(uint32 winSizeX,
                                              uint32 winSizeY) {
  if (nullptr == m_Device)
    return E_FAIL;

  ComPtr<ID3D11Texture2D> depthStencilTexture = {nullptr};
  D3D11_TEXTURE2D_DESC textureDesc = {};
  {
    textureDesc.Width = winSizeX;
    textureDesc.Height = winSizeY;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

    textureDesc.SampleDesc.Quality = 0;
    textureDesc.SampleDesc.Count = 1;

    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    textureDesc.CPUAccessFlags = 0;
    textureDesc.MiscFlags = 0;
  }

  if (FAILED(m_Device->CreateTexture2D(&textureDesc, nullptr,
                                       depthStencilTexture.GetAddressOf())))
    return E_FAIL;

  if (FAILED(m_Device->CreateDepthStencilView(depthStencilTexture.Get(),
                                              nullptr, m_DSV.GetAddressOf())))
    return E_FAIL;

  return S_OK;
}

HRESULT GraphicDevice::Create_OffScreenTarget(uint32 width, uint32 height) {
  OffscreenRenderTarget target{};
  if (FAILED(Create_OffScreenTarget(width, height, target))) return E_FAIL;
  m_Offscreens.push_back(std::move(target));
  return S_OK;
}

HRESULT GraphicDevice::Create_OffScreenTarget(uint32 width, uint32 height, OffscreenRenderTarget& target) {
  OffscreenRenderTarget rt = {};
  D3D11_TEXTURE2D_DESC textureDesc = {};
  {
    textureDesc.Width = width;
    textureDesc.Height = height;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags =
        D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    textureDesc.CPUAccessFlags = 0;
  }

  if (FAILED(m_Device->CreateTexture2D(&textureDesc, nullptr,
                                       rt.texture.GetAddressOf())))
    return E_FAIL;

  if (FAILED(m_Device->CreateRenderTargetView(rt.texture.Get(), nullptr,
                                              rt.RTV.GetAddressOf())))
    return E_FAIL;

  if (FAILED(m_Device->CreateShaderResourceView(rt.texture.Get(), nullptr,
                                                rt.SRV.GetAddressOf())))
    return E_FAIL;

#ifdef _DEBUG
  // Exercise cleanup after real color resources exist, before depth creation.
  if (m_FailNextOffscreenCreation) {
    m_FailNextOffscreenCreation = false;
    return E_FAIL;
  }
#endif

  D3D11_TEXTURE2D_DESC dsDesc = {};
  dsDesc.Width = width;
  dsDesc.Height = height;
  dsDesc.MipLevels = 1;
  dsDesc.ArraySize = 1;
  dsDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
  dsDesc.SampleDesc.Count = 1;
  dsDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
  ComPtr<ID3D11Texture2D> depthStencilTex;
  if (FAILED(m_Device->CreateTexture2D(&dsDesc, nullptr,
                                       depthStencilTex.GetAddressOf())))
    return E_FAIL;
  if (FAILED(m_Device->CreateDepthStencilView(depthStencilTex.Get(), nullptr,
                                              rt.DSV.GetAddressOf())))
    return E_FAIL;

  rt.viewport.TopLeftX = 0.f;
  rt.viewport.TopLeftY = 0.f;
  rt.viewport.Width = static_cast<Float>(width);
  rt.viewport.Height = static_cast<Float>(height);
  rt.viewport.MinDepth = 0.f;
  rt.viewport.MaxDepth = 1.f;

  target = std::move(rt);

  return S_OK;
}

Unique<GraphicDevice>
GraphicDevice::Create(_In_ const ENGINE_DESC &engineDesc) {
  auto pInstance = make_unique<GraphicDevice>();

  if (FAILED(pInstance->Initialize(engineDesc))) {
    MSG_BOX("Failed to Created : CGraphic_Device");
  }

  return pInstance;
}
