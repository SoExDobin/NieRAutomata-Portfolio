#include "TagRegistry.h"
#include "Engine_Define.h"
#include "String_Helper.h"
#include <fstream>
#include <nlohmann/json.hpp>

#include "Game.h"
#include "SpdLogger.h"


void TagMask::Set_Tag(std::initializer_list<wstring> tags)
{
    m_Tag = 0;
    for (const auto& name : tags)
        Add(name);
}

Bool TagMask::Has(const wstring& tagName) const
{
    auto tag = GAME_INSTANCE->Get_TagRegister()->Get_TagByName(tagName);
    if (tag == TAG::NONE)
    {
        LOG_ERROR(L"Wrong Tag Naming {}", tagName);
        return false;
    }

    return Has(tag);
}

void TagMask::Add(const wstring& tagName)
{
    auto tag = GAME_INSTANCE->Get_TagRegister()->Get_TagByName(tagName);
    if (tag == TAG::NONE)
    {
        LOG_ERROR(L"Wrong Tag Naming {}", tagName);
        return;
    }

    Add(tag);
}

void TagMask::Add(std::initializer_list<wstring> tags)
{
    for (const auto& name : tags)
        Add(name);
}

void TagMask::Remove(const wstring& tagName)
{
    auto tag = GAME_INSTANCE->Get_TagRegister()->Get_TagByName(tagName);
    if (tag == TAG::NONE)
    {
        LOG_ERROR(L"Wrong Tag Naming {}", tagName);
    	return;
    }
		
    Remove(tag);
}

void TagMask::Remove(std::initializer_list<wstring> tags)
{
    for (const auto& name : tags)
        Remove(name);
}

wstring TagRegistry::Get_TagName(TAG tag) const {
    const auto iter = m_TagToName.find(tag);
    if (iter != m_TagToName.end())
		return iter->second;

    return Helper::To_wString(magic_enum::enum_name(tag));
}

wstring TagRegistry::Get_TagName(uint32 tagBit) const {
	return Get_TagName(static_cast<TAG>(tagBit));
}

TAG TagRegistry::Get_TagByName(const wstring &name) const {
    auto it = m_NameToTag.find(name);
    if (it != m_NameToTag.end())
		return it->second;

    return TAG::NONE;
}

void TagRegistry::Set_TagName(TAG tag, const wstring &name) {
    auto oldName = m_TagToName.find(tag);
    if (oldName != m_TagToName.end())
		m_NameToTag.erase(oldName->second);

    m_TagToName[tag] = name;
    m_NameToTag[name] = tag;
}

void TagRegistry::Set_TagName(uint32 tagBit, const wstring &name) {
	Set_TagName(static_cast<TAG>(tagBit), name);
}

void TagRegistry::LoadFromFile(const wstring &path) {
    Clear();

    LoadDefaults();

    std::ifstream file(path);
    if (!file.is_open()) {
      return;
    }

    nlohmann::json jsonData;
    file >> jsonData;
    file.close();

    if (!jsonData.contains("version") || jsonData["version"] != "1.0") {
        return;
    }

    if (jsonData.contains("tags")) 
    {
        for (const auto& tagJson : jsonData["tags"]) {
            uint32 bit = tagJson["bit"];
            std::string nameStr = tagJson["name"];

            if (nameStr.empty())
                continue;

            wstring name = Helper::To_wString(nameStr);
            TAG tag = static_cast<TAG>(bit);

            Set_TagName(tag, name);
        }
    }
}

void TagRegistry::SaveToFile(const wstring &path) const {
    nlohmann::json jsonData;
    jsonData["version"] = "1.0";

    auto &tagsArray = jsonData["tags"];

    for (const auto &[tag, name] : m_TagToName) {
        nlohmann::json tagJson;
        tagJson["bit"] = static_cast<uint32>(tag);
        tagJson["name"] = Helper::To_String(name);
        tagsArray.push_back(tagJson);
    }

    std::ofstream file(path);
    if (!file.is_open())
		return;

    file << jsonData.dump(2);
    file.close();
}

void TagRegistry::LoadDefaults() {
    m_NameToTag[Helper::To_wString(magic_enum::enum_name(TAG::TAG_0))] = TAG::TAG_0;
    m_TagToName[TAG::TAG_0] = Helper::To_wString(magic_enum::enum_name(TAG::TAG_0));

    for (size_t i = 0; i < ETOI(TAG::END); ++i) {
        TAG tag = static_cast<TAG>(1 << i);
        wstring defaultName = Helper::To_wString(magic_enum::enum_name(tag));

        if (defaultName.empty())
			defaultName = L"TAG" + std::to_wstring(i);

        m_NameToTag.emplace(defaultName, tag);
        m_TagToName.emplace(tag, defaultName);
    }
}

void TagRegistry::Clear() {
    m_NameToTag.clear();
    m_TagToName.clear();
}