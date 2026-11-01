#include "LayerRegistry.h"
#include "Engine_Define.h"
#include "String_Helper.h"
#include <fstream>
#include <nlohmann/json.hpp>

#include "Game.h"
#include "SpdLogger.h"

wstring LayerMask::Get_LayerName() const
{
    return GAME_INSTANCE->Get_LayerRegister()->Get_LayerName(m_Layer);
}

void LayerMask::Set_Layer(const wstring& layerName)
{
    auto layer = GAME_INSTANCE->Get_LayerRegister()->Get_LayerByName(layerName);
    m_Layer = ETOI(layer);
}

void LayerMask::Set_Mask(std::initializer_list<wstring> layerNames)
{
    m_Mask = 0;
    for (const auto& name : layerNames)
        Add(name);
}

void LayerMask::Add(std::initializer_list<wstring> layerNames)
{
    for (const auto& name : layerNames)
        Add(name);
}

void LayerMask::Remove(std::initializer_list<wstring> layerNames)
{
    for (const auto& name : layerNames)
        Remove(name);
}

void LayerMask::Set_Mask(const wstring& maskName)
{
    auto mask = GAME_INSTANCE->Get_LayerRegister()->Get_LayerByName(maskName);
    m_Mask = ETOI(mask);
}

void LayerMask::Add(const wstring& layerName)
{
    auto layer = GAME_INSTANCE->Get_LayerRegister()->Get_LayerByName(layerName);
    Add(layer);
}

void LayerMask::Remove(const wstring& layerName)
{
    auto layer = GAME_INSTANCE->Get_LayerRegister()->Get_LayerByName(layerName);
    Remove(layer);
}

Bool LayerMask::Has(const wstring& layerName) const 
{
    auto layer = GAME_INSTANCE->Get_LayerRegister()->Get_LayerByName(layerName);
    return Has(layer);
}


wstring LayerRegistry::Get_LayerName(LAYER layer) const {
  const auto iter = m_LayerToName.find(layer);

  if (iter != m_LayerToName.end())
    return iter->second;

  return Helper::To_wString(magic_enum::enum_name(layer));
}

wstring LayerRegistry::Get_LayerName(uint32 layerBit) const {
  return Get_LayerName(static_cast<LAYER>(layerBit));
}

LAYER LayerRegistry::Get_LayerByName(const wstring &name) const {
  auto it = m_NameToLayer.find(name);
  if (it != m_NameToLayer.end())
    return it->second;

  return LAYER::LAYER0;
}

void LayerRegistry::Set_LayerName(LAYER layer, const wstring &name) {
  auto oldName = m_LayerToName.find(layer);
  if (oldName != m_LayerToName.end())
    m_NameToLayer.erase(oldName->second);

  m_LayerToName[layer] = name;
  m_NameToLayer[name] = layer;
}

void LayerRegistry::Set_LayerName(uint32 layerBit, const wstring &name) {
  Set_LayerName(static_cast<LAYER>(layerBit), name);
}

void LayerRegistry::LoadFromFile(const wstring &path) {
    Clear();

    LoadDefaults();

    std::ifstream file(path);
    if (!file.is_open()) {
      return;
    }

	try {
		nlohmann::json jsonData;
		file >> jsonData;
		file.close();

        if (!jsonData.contains("version") || jsonData["version"] != "1.0") {
          return;
        }

        if (jsonData.contains("layers")) {
            for (const auto &layerJson : jsonData["layers"]) {
                uint32 bit = layerJson["bit"];
                std::string nameStr = layerJson["name"];

                unsigned long index = 0;
                if (_BitScanForward(&index, bit) && layerJson.contains("collision_mask")) {
                    m_GlobalCollisionMatrix[index] = layerJson["collision_mask"];
                }

                if (nameStr.empty())
                  continue;

                wstring name = Helper::To_wString(nameStr);
                LAYER layer = static_cast<LAYER>(bit);

                Set_LayerName(layer, name);
            }
        }
    } catch (const std::exception &e) {
    }
}

void LayerRegistry::SaveToFile(const wstring &path) const {
    nlohmann::json jsonData;
    jsonData["version"] = "1.0";
    auto &layersArray = jsonData["layers"];

    for (const auto &[layer, name] : m_LayerToName) {
        nlohmann::json layerJson;
        uint32 bit = static_cast<uint32>(layer);
        layerJson["bit"] = bit;
        layerJson["name"] = Helper::To_String(name);

        unsigned long index = 0;
        if (_BitScanForward(&index, bit)) {
            layerJson["collision_mask"] = m_GlobalCollisionMatrix[index];
        }

        layersArray.push_back(layerJson);
    }

    std::filesystem::path filePath(path);
    std::ofstream file(filePath);        
    if (!file.is_open())
    {
        LOG_CRITICAL(L"Failed To Save Layers");
        return;
    }

    file << jsonData.dump(2);
    file.close();
}

void LayerRegistry::LoadDefaults() {
    for (unsigned int& i : m_GlobalCollisionMatrix)
    {
	    i = 0xFFFFFFFF;
    }

    m_NameToLayer[Helper::To_wString(magic_enum::enum_name(LAYER::LAYER0))] =
        LAYER::LAYER0;
    m_LayerToName[LAYER::LAYER0] =
        Helper::To_wString(magic_enum::enum_name(LAYER::LAYER0));

    for (size_t i = 0; i < ETOI(LAYER::END); ++i) {
        LAYER layer = static_cast<LAYER>(1 << i);
        wstring defaultName = Helper::To_wString(magic_enum::enum_name(layer));

        if (defaultName.empty())
          defaultName = L"LAYER" + std::to_wstring(i);

        m_NameToLayer.emplace(defaultName, layer);
        m_LayerToName.emplace(layer, defaultName);
    }
}

void LayerRegistry::Clear() {
  m_NameToLayer.clear();
  m_LayerToName.clear();
}

uint32 LayerRegistry::Get_GlobalMask(uint32 layerBit) const
{
    unsigned long index = 0;
    if (_BitScanForward(&index, layerBit)) {
        return m_GlobalCollisionMatrix[index];
    }
    return 0xFFFFFFFF;
}

void LayerRegistry::Set_GlobalMask(uint32 layerBit, uint32 maskBits)
{
    unsigned long index = 0;
    if (_BitScanForward(&index, layerBit)) {
        m_GlobalCollisionMatrix[index] = maskBits;
    }
}

void LayerRegistry::Toggle_GlobalMask_Symmetric(uint32 layerBitA, uint32 layerBitB)
{
    unsigned long indexA = 0, indexB = 0;
    if (_BitScanForward(&indexA, layerBitA) && _BitScanForward(&indexB, layerBitB)) {
        // A가 B를 허용/거부하면, B도 A를 허용/거부하게 토글(XOR)
        m_GlobalCollisionMatrix[indexA] ^= layerBitB;
        if (layerBitA != layerBitB) { // 본인 자신일 경우 두 번 XOR 되어 원상복구되는 현상을 방지
            m_GlobalCollisionMatrix[indexB] ^= layerBitA;
        }
    }
}
