#include "pch.h"
#include "CommunicationUI.h"
#include "TextUI.h"
#include "Texture.h"
#include "UIImage.h"
#include "WP3000Body.h"
#include "WorldUIObject.h"
#include "PartObject.h"
#include <cmath>

CommunicationUI::CommunicationUI(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
    : UIPanel{ device, context } {}
CommunicationUI::CommunicationUI(const CommunicationUI& rhs) : UIPanel{ rhs } {}

HRESULT CommunicationUI::Initialize(void*)
{
    UI_DESC desc{};
    desc.sizeX = 1920.f;
    desc.sizeY = 1080.f;
    return UIPanel::Initialize(&desc);
}

Shared<UIImage> CommunicationUI::Add_Image(const Shared<UIPanel>& parent, const tChar* key, const wstring& tag,
    Float x, Float y, Float width, Float height, Float alpha)
{
    UIImage::UI_IMAGE_DESC desc{};
    desc.x = x; desc.y = y; desc.sizeX = width; desc.sizeY = height;
    desc.textureTag = tag;
    desc.color = Color{ 1.f, 1.f, 1.f, alpha };
    if (FAILED(parent->Add_Child(Get_LevelIndex(), L"UIImage", key, &desc))) return nullptr;
    auto image = static_pointer_cast<UIImage>(parent->Find_Child(key));
    m_Images.emplace_back(image, alpha);
    return image;
}

Shared<TextUI> CommunicationUI::Add_Text(const Shared<UIPanel>& parent, const tChar* key,
    Float x, Float y, Float scale, Float pivot)
{
    TextUI::UI_TEXT_DESC desc{};
    desc.x = x; desc.y = y; desc.sizeX = 1.f; desc.sizeY = 1.f;
    desc.fontTag = L"Nier_Communication_32";
    desc.fontScale = scale; desc.pivot = pivot;
    desc.color = Color{ 0.92f, 0.90f, 0.86f, 1.f };
    if (FAILED(parent->Add_Child(Get_LevelIndex(), L"TextUI", key, &desc))) return nullptr;
    return static_pointer_cast<TextUI>(parent->Find_Child(key));
}

Bool CommunicationUI::Add_SubtitleLine()
{
    const wstring key = m_SubtitleLines.empty() ? wstring{ L"Text" } : L"Text" + std::to_wstring(m_SubtitleLines.size());
    const auto red = Add_Text(m_SubtitlePanel, (key + L"Red").c_str(), 0.f, 0.f, 0.90f, 0.5f);
    const auto cyan = Add_Text(m_SubtitlePanel, (key + L"Cyan").c_str(), 0.f, 0.f, 0.90f, 0.5f);
    const auto text = Add_Text(m_SubtitlePanel, key.c_str(), 0.f, 0.f, 0.90f, 0.5f);
    if (!red || !cyan || !text) return false;
    red->Set_Active(false);
    cyan->Set_Active(false);
    m_SubtitleSplits.emplace_back(red, cyan);
    m_SubtitleLines.push_back(text);
    m_Texts.push_back(text);
    return true;
}

HRESULT CommunicationUI::Ready_Layout(const Shared<GameObject>& pod, const wstring& portraitTag)
{
    m_Pod = pod;
    // The antenna's light cone (apex at the texture's top) is laid in canvas pixels behind the window.
    m_Beam = Add_Image(static_pointer_cast<UIPanel>(shared_from_this()), L"PodBeam", L"Comm_PodBeam", 0.f, 0.f, 1.f, 1.f, 0.45f);
    if (!m_Beam) return E_FAIL;
    m_Beam->Set_AnchorState(UI_ANCHOR::TOP_LEFT);
    m_Beam->Get_Component<Texture>()->Set_RGBA(Color{ 0.80f, 0.95f, 1.f, 0.f });
    m_Beam->Set_Active(false);
    UI_DESC podDesc{};
    podDesc.anchor = UI_ANCHOR::TOP_LEFT;
    podDesc.sizeX = 220.f; podDesc.sizeY = 144.f;
    if (FAILED(Add_Child(Get_LevelIndex(), L"UIPanel", L"PodPanel", &podDesc))) return E_FAIL;
    m_PodPanel = static_pointer_cast<UIPanel>(Find_Child(L"PodPanel"));
    // One compact rectangle contains both the signal graphics and the upright portrait.
    // Keep all content inset from its thin outer frame, with a shared internal divider.
    if (!Add_Image(m_PodPanel, L"Backing", L"UI_HUD_Black", 0.f, 0.f, 220.f, 144.f, 0.22f) ||
        !(m_Portrait = Add_Image(m_PodPanel, L"Portrait", portraitTag, 64.f, 0.f, 80.f, 128.f)) ||
        !Add_Image(m_PodPanel, L"Top", L"UI_HUD_Ivory", 0.f, -71.f, 220.f, 2.f, 0.65f) ||
        !Add_Image(m_PodPanel, L"Bottom", L"UI_HUD_Ivory", 0.f, 71.f, 220.f, 2.f, 0.65f) ||
        !Add_Image(m_PodPanel, L"Left", L"UI_HUD_Ivory", -109.f, 0.f, 2.f, 140.f, 0.65f) ||
        !Add_Image(m_PodPanel, L"Right", L"UI_HUD_Ivory", 109.f, 0.f, 2.f, 140.f, 0.65f) ||
        !Add_Image(m_PodPanel, L"Divider", L"UI_HUD_Ivory", 18.f, 0.f, 1.f, 128.f, 0.35f) ||
        !Add_Image(m_PodPanel, L"Header", L"Comm_Header", -47.f, -53.f, 108.f, 12.f, 0.75f) ||
        !Add_Image(m_PodPanel, L"SignalRing", L"Comm_Ring", -47.f, -17.f, 30.f, 30.f, 0.70f) ||
        !Add_Image(m_PodPanel, L"SignalLine", L"Comm_Horizontal", -47.f, 57.f, 104.f, 2.f, 0.65f)) return E_FAIL;
    for (int i = 0; i < 9; ++i)
    {
        const wstring key = L"VoiceBar" + std::to_wstring(i);
        auto bar = Add_Image(m_PodPanel, key.c_str(), L"UI_HUD_Ivory", -87.f + i * 10.f, 14.f, 4.f, 2.f, 0.65f);
        if (!bar) return E_FAIL;
        m_VoiceBars.push_back(bar);
    }
    m_SoundOnly = Add_Image(m_PodPanel, L"UnassignedPortrait", L"Comm_SoundOnly", 64.f, 0.f, 72.f, 10.f, 0.65f);
    if (!m_SoundOnly) return E_FAIL;
    Show_Portrait(!portraitTag.empty());
    m_Status = Add_Image(m_PodPanel, L"Status", L"Comm_Calling", -47.f, 38.f, 96.f, 12.f, 0.85f);
    for (const auto& child : m_PodPanel->Get_Children())
        if (const auto piece = Cast<UIImage>(child))
            m_PodPieces.push_back({ piece, piece->Get_Transform()->Get_LocalPosition(), piece->Get_Transform()->Get_LocalScale().x });
    m_TearBar = Add_Image(m_PodPanel, L"TearBar", L"UI_HUD_Ivory", 0.f, 0.f, 216.f, 2.f, 0.30f);
    if (!m_TearBar) return E_FAIL;
    m_TearBar->Set_Active(false);

    UI_DESC subtitleDesc{};
    subtitleDesc.y = 470.f;
    subtitleDesc.sizeX = 1920.f; subtitleDesc.sizeY = 140.f;
    if (FAILED(Add_Child(Get_LevelIndex(), L"UIPanel", L"SubtitlePanel", &subtitleDesc))) return E_FAIL;
    m_SubtitlePanel = static_pointer_cast<UIPanel>(Find_Child(L"SubtitlePanel"));
    if (!Add_Image(m_SubtitlePanel, L"Backing", L"UI_HUD_Black", 0.f, 0.f, 1920.f, 140.f, 0.50f)) return E_FAIL;
    if (!m_Status || !Add_SubtitleLine()) return E_FAIL;
    m_PodPanel->Set_Active(false);
    Hide_Line();
    Show_Calling();
    return S_OK;
}

void CommunicationUI::Set_PodVisible(Bool visible)
{
    if (visible) Set_CloseProgress(0.f);
    m_ShowPodUI = visible;
    if (!visible && m_PodPanel) m_PodPanel->Set_Active(false);
}
void CommunicationUI::Show_Portrait(Bool visible)
{
    m_Portrait->Set_Active(visible);
    m_SoundOnly->Set_Active(!visible);
}
void CommunicationUI::Show_Calling()
{
    m_Calling = true;
    m_Status->Get_Component<Texture>()->Set_TextureTag(L"Comm_Calling");
    Hide_Line();
}
void CommunicationUI::Show_Connected()
{
    m_Calling = false;
    m_Status->Get_Component<Texture>()->Set_TextureTag(L"Comm_Connected");
    Start_Glitch(0.28f, 1.f); // The link locking on.
}
void CommunicationUI::Show_Line(const wstring& /*speaker*/, const wstring& text)
{
    m_SubtitleText = text;
    Layout_Subtitle(m_SubtitleViewport.x, m_SubtitleViewport.y);
    Start_Glitch(0.14f, 0.7f);
}
void CommunicationUI::Layout_Subtitle(Float viewportWidth, Float viewportHeight)
{
    m_SubtitleViewport = Vector2{ viewportWidth, viewportHeight };
    // TextUI scales uniformly by viewport height, while UIImage uses the canvas axes.
    // Convert measured text widths into canvas units to also fit narrow game views.
    const Float widthScale = 1920.f * viewportHeight / (1080.f * viewportWidth);
    const Float paddingX = 24.f * widthScale;
    const Float maxTextWidth = 1728.f - 2.f * paddingX;
    // Wrap by the actual font metrics, including unspaced Korean sentences.
    vector<wstring> lines;
    wstring line;
    for (const auto ch : m_SubtitleText)
    {
        if (ch == L'\n') { lines.push_back(line); line.clear(); continue; }
        const wstring next = line + ch;
        if (!line.empty() && GAME_INSTANCE->Measure_Font(L"Nier_Communication_32", next.c_str(), 0.f).x * 0.90f * widthScale > maxTextWidth)
        { lines.push_back(line); line.clear(); }
        line += ch;
    }
    lines.push_back(line);
    // Separate TextUI lines keep shorter wrapped lines individually centred.
    while (m_SubtitleLines.size() < lines.size())
        if (!Add_SubtitleLine()) { Hide_Line(); return; }
    // Communication_32.spritefont is Noto Sans KR 32, whose generated line spacing is 32 + 7 pixels.
    const Float extraHeight = static_cast<Float>(lines.size() - 1) * 39.f * 0.90f;
    const Float panelHeight = extraHeight + 39.f * 0.90f + 24.f;
    for (size_t i = 0; i < m_SubtitleLines.size(); ++i)
    {
        const auto& textLine = m_SubtitleLines[i];
        textLine->Set_Active(i < lines.size());
        if (i >= lines.size()) continue;
        textLine->Set_Text(lines[i]);
        m_SubtitleSplits[i].first->Set_Text(lines[i]);
        m_SubtitleSplits[i].second->Set_Text(lines[i]);
        textLine->Get_Transform()->Set_LocalPosition(0.f, static_cast<Float>(i) * 39.f * 0.90f - extraHeight * 0.5f, 0.f);
    }
    // As in the reference, the band runs edge to edge; only its height follows the line count.
    const Float panelWidth = 1920.f;
    m_SubtitlePanel->Get_Transform()->Set_LocalPosition(0.f, 540.f - 24.f - panelHeight * 0.5f, 0.f);
    m_SubtitlePanel->Get_Transform()->Set_LocalScale(panelWidth, panelHeight, 1.f);
    m_SubtitlePanel->Find_Child(L"Backing")->Get_Transform()->Set_LocalScale(panelWidth, panelHeight, 1.f);
    m_SubtitlePanel->Set_Active(true);
}
HRESULT CommunicationUI::Render()
{
    // The active D3D viewport is also correct for Editor game views and offscreen MRTs.
    D3D11_VIEWPORT viewport{};
    UINT count = 1;
    m_Context->RSGetViewports(&count, &viewport);
    if (m_SubtitlePanel && m_SubtitlePanel->Is_Active() && count && viewport.Width > 0.f && viewport.Height > 0.f &&
        (m_SubtitleViewport.x != viewport.Width || m_SubtitleViewport.y != viewport.Height))
        Layout_Subtitle(viewport.Width, viewport.Height);
    return UIPanel::Render();
}
void CommunicationUI::Hide_Line() { m_SubtitlePanel->Set_Active(false); }
void CommunicationUI::Set_Opacity(Float alpha)
{
    alpha = std::clamp(alpha, 0.f, 1.f);
    m_Opacity = alpha;
    for (const auto& [image, base] : m_Images)
        image->Get_Component<Texture>()->Set_RGBA(Color{ 1.f, 1.f, 1.f, base * alpha });
    for (const auto& text : m_Texts)
        text->Set_Color(Color{ 0.92f, 0.90f, 0.86f, alpha });
}
void CommunicationUI::Set_CloseProgress(Float progress)
{
    const Float t = std::clamp(progress, 0.f, 1.f);
    m_OpenWidth = 1.f - t * t * (3.f - 2.f * t);
    Apply_PanelWidth();
    if (t >= 1.f)
    {
        m_PodPanel->Set_Active(false);
        m_Beam->Set_Active(false);
    }
}
void CommunicationUI::Apply_PanelWidth()
{
    // UI children use pixel dimensions and cancel parent scale: shrink their actual geometry.
    for (const auto& [piece, base, width] : m_PodPieces)
    {
        const auto transform = piece->Get_Transform();
        const auto scale = transform->Get_LocalScale();
        transform->Set_LocalPosition(base.x * m_OpenWidth, base.y, base.z);
        transform->Set_LocalScale((std::max)(width * m_OpenWidth, 0.001f), scale.y, scale.z);
    }
    if (m_OpenWidth < 1.f) m_TearBar->Set_Active(false);
}
void CommunicationUI::Late_Update(Float timeDelta)
{
    if (!m_PodPanel) return;
    m_Time += std::max(timeDelta, 0.f);
    const Float target = std::clamp(GAME_INSTANCE->Get_SoundLevel(SOUNDCHANNEL::COMMUNICATION_VOICE) * 8.f, 0.f, 1.f);
    const Float response = target > m_VoiceLevel ? 24.f : 10.f;
    m_VoiceLevel += (target - m_VoiceLevel) * (1.f - std::exp(-response * std::max(timeDelta, 0.f)));
    if (target == 0.f && m_VoiceLevel < 0.001f) m_VoiceLevel = 0.f;
    for (size_t i = 0; i < m_VoiceBars.size(); ++i)
    {
        // A stylised meter, not frequency bins: all bars follow the measured voice envelope.
        const Float shape = 0.4f + 0.6f * std::abs(std::sin(m_Time * 11.f + static_cast<Float>(i) * 1.7f));
        m_VoiceBars[i]->Get_Transform()->Set_LocalScale(4.f, 2.f + 20.f * m_VoiceLevel * shape, 1.f);
    }
    Vector2 position{};
    const auto pod = m_Pod.lock();
    const auto part = Cast<PartObject>(pod);
    const Vector3 worldPosition = pod && !pod->Is_Destroy() ?
        (part ? part->Get_CombinedWorldMatrix().Translation() : pod->Get_Transform()->Get_Position()) : Vector3::Zero;
    const Bool visible = m_OpenWidth > 0.f && m_ShowPodUI && pod && !pod->Is_Destroy() && WorldUIObject::To_Canvas(
        worldPosition + Vector3{ 0.f, 0.65f, 0.f }, position) &&
        std::isfinite(position.x) && std::isfinite(position.y) &&
        position.x >= 0.f && position.x <= 1920.f && position.y >= 0.f && position.y <= 1080.f;
    m_PodPanel->Set_Active(visible);
    Update_Glitch(timeDelta);
    const Vector2 panel{ std::clamp(position.x + 125.f, 131.f, 1789.f), std::clamp(position.y - 78.f, 88.f, 888.f) };
    if (visible)
        m_PodPanel->Get_Transform()->Set_LocalPosition(panel.x + m_PodJitter, panel.y, 0.f);
    Vector2 apex{};
    const auto body = Cast<WP3000Body>(pod);
    const Bool apexVisible = visible && WorldUIObject::To_Canvas(body ? body->Get_AntennaPosition() : worldPosition, apex) &&
        std::isfinite(apex.x) && std::isfinite(apex.y);
    Update_Beam(apex, apexVisible, panel);
    if (m_OpenWidth < 1.f)
        Apply_PanelWidth();
    if (m_Calling)
        m_Status->Set_Active(std::fmod(m_Time, 0.8f) < 0.6f);
    else m_Status->Set_Active(true);
}

Float CommunicationUI::Random01()
{
    m_GlitchSeed ^= m_GlitchSeed << 13;
    m_GlitchSeed ^= m_GlitchSeed >> 17;
    m_GlitchSeed ^= m_GlitchSeed << 5;
    return static_cast<Float>(m_GlitchSeed >> 8) / 16777216.f;
}

void CommunicationUI::Start_Glitch(Float duration, Float strength)
{
    m_GlitchStrength = m_GlitchLeft > 0.f ? std::max(m_GlitchStrength, strength) : strength;
    m_GlitchLeft = std::max(m_GlitchLeft, duration);
    m_GlitchStep = 0.f;
    m_GlitchNext = 1.2f + 1.8f * Random01();
}

void CommunicationUI::Update_Glitch(Float timeDelta)
{
    const Float dt = std::max(timeDelta, 0.f);
    if (m_GlitchLeft > 0.f) m_GlitchLeft -= dt;
    else if ((m_GlitchNext -= dt) <= 0.f) Start_Glitch(0.05f + 0.08f * Random01(), 0.45f + 0.4f * Random01());
    const Bool glitching = m_GlitchLeft > 0.f;
    // Re-rolled about 40 times a second so the tear jumps instead of sliding; everything snaps back after a burst.
    if (glitching && (m_GlitchStep -= dt) <= 0.f)
    {
        m_GlitchStep = 0.025f;
        const Float k = m_GlitchStrength;
        m_PodJitter = (Random01() * 2.f - 1.f) * 6.f * k;
        m_TearY = -64.f + 128.f * Random01();
        m_TearHeight = 10.f + 34.f * Random01();
        m_TearShift = (Random01() * 2.f - 1.f) * 16.f * k;
        m_SubtitleShift = (Random01() * 2.f - 1.f) * 10.f * k;
        m_Flicker = Random01() < 0.35f ? 0.35f : 1.f;
    }
    else if (!glitching)
    {
        m_PodJitter = m_TearShift = m_SubtitleShift = 0.f;
        m_Flicker = 1.f;
    }
    for (const auto& [piece, base, width] : m_PodPieces)
    {
        const Bool torn = glitching && std::abs(base.y - m_TearY) <= m_TearHeight * 0.5f;
        piece->Get_Transform()->Set_LocalPosition(base.x + (torn ? m_TearShift : 0.f), base.y, base.z);
    }
    m_TearBar->Set_Active(glitching);
    if (glitching)
    {
        m_TearBar->Get_Transform()->Set_LocalPosition(m_TearShift * 0.5f, m_TearY, 0.f);
        m_TearBar->Get_Transform()->Set_LocalScale(216.f, 1.f + 3.f * m_GlitchStrength, 1.f);
        m_TearBar->Get_Component<Texture>()->Set_RGBA(Color{ 1.f, 1.f, 1.f, 0.30f * m_Opacity });
    }
    m_Portrait->Get_Component<Texture>()->Set_RGBA(Color{ 1.f, 1.f, 1.f, m_Opacity * m_Flicker });
    // Subtitles split into red and cyan copies either side of the line while it slides.
    const Float split = 1.f + 4.f * m_GlitchStrength;
    for (size_t i = 0; i < m_SubtitleLines.size(); ++i)
    {
        const auto& line = m_SubtitleLines[i];
        const auto& [red, cyan] = m_SubtitleSplits[i];
        Vector3 linePosition = line->Get_Transform()->Get_LocalPosition();
        linePosition.x = glitching ? m_SubtitleShift : 0.f;
        line->Get_Transform()->Set_LocalPosition(linePosition);
        const Bool shown = glitching && line->Is_Active();
        red->Set_Active(shown);
        cyan->Set_Active(shown);
        if (!shown) continue;
        red->Get_Transform()->Set_LocalPosition(linePosition.x - split, linePosition.y, 0.f);
        cyan->Get_Transform()->Set_LocalPosition(linePosition.x + split, linePosition.y, 0.f);
        red->Set_Color(Color{ 1.f, 0.25f, 0.25f, 0.6f * m_Opacity });
        cyan->Set_Color(Color{ 0.25f, 0.9f, 1.f, 0.6f * m_Opacity });
    }
}

void CommunicationUI::Update_Beam(const Vector2& apex, Bool apexVisible, const Vector2& panel)
{
    // The cone's apex sits on the antenna and its open end fans across the window.
    const Vector2 direction = panel - apex;
    const Float length = direction.Length();
    const Bool shown = apexVisible && length >= 24.f;
    m_Beam->Set_Active(shown);
    if (!shown) return;
    const Vector2 middle = (apex + panel) * 0.5f;
    m_Beam->Get_Transform()->Set_LocalPosition(middle.x + m_PodJitter * 0.5f, middle.y, 0.f);
    m_Beam->Get_Transform()->Set_LocalScale((std::max)(0.001f, std::min(150.f, length * 1.4f) * m_OpenWidth), length, 1.f);
    // Canvas y runs down; the texture's apex-to-base (its -y) turns onto the apex-to-window direction.
    m_Beam->Get_Transform()->Set_LocalRotation(0.f, 0.f, XMConvertToDegrees(std::atan2(direction.x, direction.y)));
    const Float hum = 0.85f + 0.15f * std::sin(m_Time * 9.f);
    m_Beam->Get_Component<Texture>()->Set_RGBA(Color{ 0.92f, 0.90f, 0.86f, 0.35f * hum * m_Flicker * m_Opacity });
}
Shared<CommunicationUI> CommunicationUI::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
    auto result = make_shared<CommunicationUI>(device, context);
    return SUCCEEDED(result->Initialize_Prototype()) ? result : nullptr;
}
Shared<GameObject> CommunicationUI::Clone(void* arg)
{
    auto result = make_shared<CommunicationUI>(*this);
    return SUCCEEDED(result->Initialize(arg)) ? result : nullptr;
}
