#pragma once
#include "UIPanel.h"
NS_BEGIN(Engine)
class TextUI;
NS_END
NS_BEGIN(Client)
// Layout and strings belong to the editable LoadingUI prefab.
class CLIENT_DLL LoadingBackground final : public UIPanel {
public:
    LoadingBackground() = default;
    LoadingBackground(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
    LoadingBackground(const LoadingBackground& rhs);
    void Update(Float timeDelta) override;
    Float Get_LogDuration() const { return m_LogDuration; }
    HRESULT Set_LogDuration(Float value);
    void Set_LoadComplete(Bool value) { m_LoadComplete = value; }
    Bool Is_PresentationFinished() const { return m_FinishTime >= 0.45f; }
    static Shared<LoadingBackground> Load_UI(uint32 levelIndex);
    static Shared<LoadingBackground> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
    Shared<GameObject> Clone(void* arg) override;
private:
    struct LogLine { Shared<TextUI> text; wstring fullText; size_t row; };
    struct TextEffect { Shared<TextUI> text; Vector3 position; Float channel; };
    vector<LogLine> m_Lines;
    vector<TextEffect> m_TextEffects;
    Shared<GameObject> m_Spinner;
    Float m_LogDuration{3.6f}, m_Elapsed{}, m_FinishTime{};
    size_t m_RowCount{};
    Bool m_Bound{}, m_LoadComplete{true};
};
NS_END
