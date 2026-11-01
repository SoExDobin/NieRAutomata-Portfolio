#include "pch.h"
#include "EditorObject.h"
#include "GameObject.h"

NS_BEGIN(Editor)

class InspectorObject : public EditorObject
{
public:
	InspectorObject();
	~InspectorObject() override = default;

public:
    HRESULT Initialize(const Shared<Engine::GameObject>& pTarget);

public:
    std::vector<std::wstring> Get_LayerNames() const;
    void Set_LayerNames(const std::vector<std::wstring>& names);

    std::vector<std::wstring> Get_TagNames() const;
    void Set_TagNames(const std::vector<std::wstring>& names);

    Bool Get_UseCustomMask() const;
    void Set_UseCustomMask(Bool use);

private:
    Shared<Engine::GameObject> m_pTarget{ nullptr };
};

NS_END
