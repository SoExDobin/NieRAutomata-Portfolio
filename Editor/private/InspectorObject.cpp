#include "pch.h"
#include "InspectorObject.h"
#include "Game.h"
#include "LayerRegistry.h"
#include "TagRegistry.h"

using namespace Editor;
using namespace Engine;

InspectorObject::InspectorObject() : EditorObject() {}

HRESULT InspectorObject::Initialize(const Shared<Engine::GameObject>& pTarget)
{
    m_pTarget = pTarget;
    return S_OK;
}

std::vector<std::wstring> InspectorObject::Get_LayerNames() const
{
    std::vector<std::wstring> names;
    if (!m_pTarget) return names;

    auto& registry = GAME_INSTANCE->Get_LayerRegister();
    for (auto& pair : registry->Get_AllLayers()) {
        if (m_pTarget->Get_LayerMask().Has(pair.first))
            names.push_back(pair.second);
    }
    return names;
}

void InspectorObject::Set_LayerNames(const std::vector<std::wstring>& names)
{
    if (!m_pTarget) return;

    auto& registry = GAME_INSTANCE->Get_LayerRegister();
    m_pTarget->Get_LayerMask().Set_Mask(static_cast<LAYER>(0)); 
    for (const auto& name : names) {
        m_pTarget->Get_LayerMask().Add(registry->Get_LayerByName(name));
    }
}

std::vector<std::wstring> InspectorObject::Get_TagNames() const
{
    std::vector<std::wstring> names;
    if (!m_pTarget) return names;

    auto& registry = GAME_INSTANCE->Get_TagRegister();
    for (auto& pair : registry->Get_AllTags()) {
        if (m_pTarget->Get_TagMask().Has(pair.first))
            names.push_back(pair.second);
    }
    return names;
}

void InspectorObject::Set_TagNames(const std::vector<std::wstring>& names)
{
    if (!m_pTarget) return;

    auto& registry = GAME_INSTANCE->Get_TagRegister();
    m_pTarget->Get_TagMask().Clear();
    for (const auto& name : names) {
        m_pTarget->Get_TagMask().Add(registry->Get_TagByName(name));
    }
}

Bool InspectorObject::Get_UseCustomMask() const
{
    if (!m_pTarget) return false;
    return !m_pTarget->Get_LayerMask().Is_GlobalMask();
}

void InspectorObject::Set_UseCustomMask(Bool use)
{
    if (!m_pTarget) return;
    m_pTarget->Get_LayerMask().Set_UseCustomMask(use);
}
