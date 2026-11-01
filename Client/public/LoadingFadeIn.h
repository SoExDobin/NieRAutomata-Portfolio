#pragma once
#include "UIImage.h"
NS_BEGIN(Client)
// One image plays both directions; retain the registered type for existing editor documents.
class CLIENT_DLL LoadingFadeIn : public UIImage {
public:
    struct FADE_IN_DESC : UI_DESC { Float fadeSpeed{0.25f}; };
    LoadingFadeIn() = default;
    LoadingFadeIn(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
    LoadingFadeIn(const LoadingFadeIn& rhs);
    HRESULT Initialize(void* arg) override;
    void Update(Float timeDelta) override;
    void Submit_RenderGroup() override;
    Bool Is_SceneSerializable() const override { return false; }
    void Play(Bool cover);
    Bool Is_FadeFinished() const { return !m_Playing; }
    Float Get_Duration() const { return m_Duration; }
    HRESULT Set_Duration(Float seconds);
    void Reset_Fade(Float speed);
    static Shared<LoadingFadeIn> Load_UI(uint32 levelIndex);
    static Shared<LoadingFadeIn> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
    Shared<GameObject> Clone(void* arg) override;
private:
    Float m_Duration{0.45f}, m_Elapsed{}, m_StartAlpha{}, m_TargetAlpha{};
    Bool m_Playing{};
};
NS_END
