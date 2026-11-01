#pragma once
#include "Client_Define.h"
#include "UIPanel.h"

NS_BEGIN(Engine)
class UIImage;
class TextUI;
NS_END

NS_BEGIN(Client)
// Fixed 1920x1080 canvas: only PodPanel follows the Pod; subtitles remain at the bottom.
class CLIENT_DLL CommunicationUI final : public UIPanel
{
public:
    CommunicationUI() = default;
    CommunicationUI(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
    CommunicationUI(const CommunicationUI& rhs);
    HRESULT Initialize(void* arg) override;
    Bool Is_SceneSerializable() const override { return false; }
    void Late_Update(Float timeDelta) override;
    HRESULT Render() override;
    HRESULT Ready_Layout(const Shared<GameObject>& pod, const wstring& portraitTag);
    void Set_PodVisible(Bool visible);
    // The operator's portrait, or SOUND ONLY when the Pod speaks or no portrait was given.
    void Show_Portrait(Bool visible);
    void Show_Calling();
    void Show_Connected();
    void Show_Line(const wstring& speaker, const wstring& text);
    void Hide_Line();
    void Set_Opacity(Float alpha);
    void Set_CloseProgress(Float progress);
    static Shared<CommunicationUI> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
    Shared<GameObject> Clone(void* arg = nullptr) override;

private:
    Shared<UIImage> Add_Image(const Shared<UIPanel>& parent, const tChar* key, const wstring& tag,
        Float x, Float y, Float width, Float height, Float alpha = 1.f);
    Shared<TextUI> Add_Text(const Shared<UIPanel>& parent, const tChar* key,
        Float x, Float y, Float scale, Float pivot);
    // A subtitle line with its red/cyan copies created first, so the copies draw underneath it.
    Bool Add_SubtitleLine();
    void Layout_Subtitle(Float viewportWidth, Float viewportHeight);
    // A short signal tear: the panel jumps, one band of it slides, the portrait flickers and the subtitle splits in colour.
    void Start_Glitch(Float duration, Float strength);
    void Update_Glitch(Float timeDelta);
    void Apply_PanelWidth();
    void Update_Beam(const Vector2& apex, Bool apexVisible, const Vector2& panel);
    Float Random01();
    Weak<GameObject> m_Pod;
    Shared<UIPanel> m_PodPanel;
    Shared<UIPanel> m_SubtitlePanel;
    Shared<UIImage> m_Status;
    Shared<UIImage> m_Portrait;
    Shared<UIImage> m_SoundOnly;
    Shared<UIImage> m_Beam;
    Shared<UIImage> m_TearBar;
    struct PodPiece { Shared<UIImage> image; Vector3 position; Float width; };
    vector<PodPiece> m_PodPieces;
    vector<Shared<TextUI>> m_SubtitleLines;
    vector<pair<Shared<TextUI>, Shared<TextUI>>> m_SubtitleSplits;
    vector<Shared<UIImage>> m_VoiceBars;
    wstring m_SubtitleText;
    Vector2 m_SubtitleViewport{ 1920.f, 1080.f };
    vector<pair<Shared<UIImage>, Float>> m_Images;
    vector<Shared<TextUI>> m_Texts;
    Float m_Time{};
    Float m_VoiceLevel{};
    Float m_Opacity{ 1.f };
    Float m_OpenWidth{ 1.f };
    Bool m_Calling{};
    Bool m_ShowPodUI{ true };
    uint32 m_GlitchSeed{ 0x9E3779B9u };
    Float m_GlitchNext{ 0.8f };
    Float m_GlitchLeft{};
    Float m_GlitchStrength{};
    Float m_GlitchStep{};
    Float m_PodJitter{};
    Float m_TearY{};
    Float m_TearHeight{};
    Float m_TearShift{};
    Float m_SubtitleShift{};
    Float m_Flicker{ 1.f };
};
NS_END
