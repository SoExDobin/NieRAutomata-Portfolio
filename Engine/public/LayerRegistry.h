#pragma once
#include "Engine_Define.h"


NS_BEGIN(Engine)

typedef struct ENGINE_DLL LayerMask final {
    LayerMask() = default;
    LayerMask(const LayerMask &layerMask)
        : m_Layer(layerMask.m_Layer), m_Mask(layerMask.m_Mask), m_UseCustomMask(layerMask.m_UseCustomMask) {}

    uint32 Get_Layer() const { return m_Layer; }
    wstring Get_LayerName() const;
    void Set_Layer(const LAYER layer) { m_Layer = ETOI(layer); }
    void Set_Layer(const wstring& layerName);
    uint32 Get_Mask() const { return m_Mask; }
    void Set_Mask(const LAYER mask) { m_Mask = ETOI(mask); m_UseCustomMask = true; }
    void Set_UseCustomMask(Bool use) { m_UseCustomMask = use; }
    void Set_Mask(const wstring& maskName);
    void Set_Mask(std::initializer_list<wstring> layerName);
    void ClearMask() { m_Mask = 0; }

    void Add(LAYER layer) { m_Mask |= ETOI(layer); }
    void Add(uint32 layer) { m_Mask |= layer; }
    void Add(const wstring& layerName);
    void Add(std::initializer_list<wstring> layerName);
    void Remove(LAYER layer) { m_Mask &= ~ETOI(layer); }
    void Remove(uint32 layer) { m_Mask &= ~layer; }
    void Remove(const wstring& layer);
    void Remove(std::initializer_list<wstring> layerName);
    Bool Has(LAYER layer) const { return (m_Mask & ETOI(layer)) != 0; }
    Bool Has(uint32 layer) const { return (m_Mask & layer) != 0; }
    Bool Has(const wstring& layer) const;

public:
    Bool Is_GlobalMask() const { return !m_UseCustomMask; }

private:
    uint32 m_Layer = { ETOI(LAYER::LAYER0) };
    uint32 m_Mask = {0xFFFFFFFF};

    Bool m_UseCustomMask = false;
} LAYER_MASK;

class ENGINE_DLL LayerRegistry 
{
public:
    LayerRegistry() = default;
    ~LayerRegistry() = default;

public:
    wstring Get_LayerName(LAYER layer) const;
    wstring Get_LayerName(uint32 layerBit) const;
    void Set_LayerName(LAYER layer, const wstring &name);
    void Set_LayerName(uint32 layerBit, const wstring &name);

    LAYER Get_LayerByName(const wstring &name) const;
    const map<LAYER, wstring>& Get_AllLayers() const { return m_LayerToName; }

public:
    void LoadFromFile(const wstring& path);
    void SaveToFile(const wstring& path) const;
    void Clear();

public:
    uint32 Get_GlobalMask(uint32 layerBit) const;
    void Set_GlobalMask(uint32 layerBit, uint32 maskBits);
    void Toggle_GlobalMask_Symmetric(uint32 layerBitA, uint32 layerBitB);

private:
    void LoadDefaults();

    map<wstring, LAYER> m_NameToLayer;
    map<LAYER, wstring> m_LayerToName;

private:
    uint32 m_GlobalCollisionMatrix[32]{};
};

NS_END