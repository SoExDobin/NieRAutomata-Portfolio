#pragma once
#include "Engine_Define.h"
#include "Object.h"
#include <chrono>


NS_BEGIN(Engine)
class GraphicDevice final : public Object {
    friend class Game;
private:
    struct OffscreenRenderTarget {
        ComPtr<ID3D11Texture2D> texture;
        ComPtr<ID3D11RenderTargetView> RTV;
        ComPtr<ID3D11ShaderResourceView> SRV;
        ComPtr<ID3D11DepthStencilView> DSV;
        D3D11_VIEWPORT viewport;
    };

public:
	GraphicDevice() = default;
	~GraphicDevice() override;

public:
	ComPtr<ID3D11Device> Get_Device() const { return m_Device; }
	ComPtr<ID3D11DeviceContext> Get_Context() const { return m_Context; }
	ComPtr<IDXGISwapChain1> Get_SwapChain() const { return m_SwapChain; }
    const D3D11_VIEWPORT& Get_ViewportDesc() const { return m_ViewPort; }
    uint32 Get_ActiveScreen() const { return m_ActiveScreen; }
    void Set_DepthStencilState(ID3D11DepthStencilState* state, UINT ref);

public:
    HRESULT Initialize(const ENGINE_DESC &engineDesc);
    void On_Destroy() override;
    HRESULT Clear_BackBufferView(const Shared<Float4> &clearColor) const;
    HRESULT Clear_DepthStencilView() const;
    HRESULT Present() const;
    HRESULT OnResize(uint32 width, uint32 height, uint32 screenIndex = UINT_MAX);

public:
    HRESULT Begin_RenderOffScreen(uint32 screenIndex);
    HRESULT End_RenderOffScreen();
    ComPtr<ID3D11ShaderResourceView> Get_OffscreenSRV(uint32 screenIndex);

private:
  HRESULT Ready_SwapChain(HWND hWnd, WINMODE isWindowed, uint32 winSizeX, uint32 winSizeY);
  HRESULT Ready_BackBufferRenderTargetView();
  HRESULT Ready_DepthStencilView(uint32 winSizeX, uint32 winSizeY);
  HRESULT Create_OffScreenTarget(uint32 width, uint32 height);
  HRESULT Create_OffScreenTarget(uint32 width, uint32 height, OffscreenRenderTarget& target);
  HRESULT Prepare_OffscreenResize(uint32 width, uint32 height, uint32 screenIndex, OffscreenRenderTarget& replacement);
  void Commit_OffscreenResize(uint32 screenIndex, OffscreenRenderTarget&& replacement);
#ifdef _DEBUG
  Bool m_FailNextOffscreenCreation = false;
#endif

private:
  ComPtr<ID3D11Device> m_Device = {nullptr};
  ComPtr<ID3D11DeviceContext> m_Context = {nullptr};
  ComPtr<IDXGISwapChain1> m_SwapChain = {nullptr};
  // 창 모드 flip 스왑체인은 Present(0)이어도 모니터 주사율에 묶이므로, 지원되면 tearing을 허용해 프레임 상한을 푼다.
  Bool m_AllowTearing = false;
  // 모든 실행(Launcher·Editor)의 프레임 상한. Present 앞에서 다음 차례까지 고해상도 타이머로 기다린다.
  HANDLE m_FrameTimer = nullptr;
  mutable std::chrono::steady_clock::time_point m_NextPresent{};

  ComPtr<ID3D11RenderTargetView> m_RTV = {nullptr};
  ComPtr<ID3D11DepthStencilView> m_DSV = {nullptr};
  D3D11_VIEWPORT m_ViewPort = {};

  vector<OffscreenRenderTarget> m_Offscreens;
  uint32 m_ActiveScreen = UINT_MAX;

public:
  static Unique<GraphicDevice> Create(_In_ const ENGINE_DESC &engineDesc);
};

NS_END
