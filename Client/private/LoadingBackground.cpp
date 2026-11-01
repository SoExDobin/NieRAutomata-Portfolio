#include "pch.h"
#include "LoadingBackground.h"
#include "TextUI.h"
#include "Game.h"
#include "SpdLogger.h"
#include <filesystem>
#include <cmath>
LoadingBackground::LoadingBackground(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context) : UIPanel(device, context) {}
LoadingBackground::LoadingBackground(const LoadingBackground& rhs) : UIPanel(rhs), m_LogDuration(rhs.m_LogDuration) {}
HRESULT LoadingBackground::Set_LogDuration(Float value) {
    if (!std::isfinite(value) || value < 0.1f || value > 30.f) return E_INVALIDARG;
    m_LogDuration = value;
    return S_OK;
}
void LoadingBackground::Update(Float timeDelta) {
    if (!m_Bound) {
        const auto namedChild = [this](const wstring& name) -> Shared<GameObject> {
            for (const auto& child : Get_Children()) if (child->Get_Name() == name) return child;
            return nullptr;
        };
        if (const auto logs = namedChild(L"Logs")) {
            m_RowCount = logs->Get_Children().size();
            for (size_t row = 0; row < m_RowCount; ++row)
                for (const auto& child : logs->Get_Children()[row]->Get_Children())
                    if (auto text = Cast<TextUI>(child))
                        m_Lines.push_back({text, text->Get_Text(), row});
        }
        m_Spinner = namedChild(L"Spinner");
        const auto bindText = [this](const auto& self, const Shared<GameObject>& object) -> void {
            if (auto text = Cast<TextUI>(object)) {
                const auto& name = text->Get_Name();
                const Float channel = name.find(L"Red") != wstring::npos ? -1.f :
                    name.find(L"Cyan") != wstring::npos ? 1.f : 0.f;
                m_TextEffects.push_back({text, text->Get_Transform()->Get_LocalPosition(), channel});
            }
            for (const auto& child : object->Get_Children()) self(self, child);
        };
        for (const auto& child : Get_Children()) bindText(bindText, child);
        m_Bound = true;
    }
    m_Elapsed += std::max(timeDelta, 0.f);
    const Float progress = std::min(m_Elapsed / m_LogDuration, 1.f);
    for (auto& line : m_Lines) {
        const Float amount = progress * static_cast<Float>(m_RowCount) - static_cast<Float>(line.row);
        const size_t count = (!m_LoadComplete && line.row + 1 == m_RowCount) ? 0 :
            static_cast<size_t>(std::clamp(amount, 0.f, 1.f) * line.fullText.size());
        line.text->Set_Text(line.fullText.substr(0, count));
    }
    // Set_LocalRotation takes degrees: one turn a second for as long as the screen is up.
    if (m_Spinner) m_Spinner->Get_Transform()->Set_LocalRotation(0.f, 0.f, -std::fmod(m_Elapsed, 1.f) * 360.f);
    // Brief, staggered horizontal tears; always return to authored positions between bursts.
    const Float burst = std::fmod(m_Elapsed, 1.35f);
    for (size_t i = 0; i < m_TextEffects.size(); ++i) {
        const auto& effect = m_TextEffects[i];
        Vector3 position = effect.position;
        const Float phase = std::fmod(static_cast<Float>(i / 3) * 0.019f, 0.065f);
        if (burst >= 0.88f + phase && burst < 0.95f + phase) {
            const Float tear = std::sin(std::floor(m_Elapsed * 48.f) + static_cast<Float>(i / 3) * 2.1f);
            position.x += tear * 5.f + effect.channel * 2.f;
        }
        effect.text->Get_Transform()->Set_LocalPosition(position);
    }
    if (m_LoadComplete && progress >= 1.f) m_FinishTime += std::max(timeDelta, 0.f);
    UIPanel::Update(timeDelta);
}
Shared<LoadingBackground> LoadingBackground::Load_UI(uint32 levelIndex) {
    for (const auto& [guid, path] : GAME_INSTANCE->Get_PrefabDocuments())
        if (std::filesystem::path(path).stem() == L"LoadingUI") {
            Shared<GameObject> root;
            if (SUCCEEDED(GAME_INSTANCE->DeSerializePrefabDocument(guid, root, levelIndex)))
                if (auto ui = Cast<LoadingBackground>(root)) return ui;
            if (root) root->Destroy_Subtree();
            break;
        }
    LOG_ERROR(L"[LoadingUI] LoadingUI prefab is missing or invalid");
    return nullptr;
}
Shared<LoadingBackground> LoadingBackground::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context) {
    auto result = make_shared<LoadingBackground>(device, context);
    return SUCCEEDED(result->Initialize_Prototype()) ? result : nullptr;
}
Shared<GameObject> LoadingBackground::Clone(void* arg) {
    auto result = make_shared<LoadingBackground>(*this);
    return SUCCEEDED(result->Initialize(arg)) ? result : nullptr;
}
