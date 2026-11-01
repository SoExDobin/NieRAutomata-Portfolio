#pragma once
#include "Component.h"

#include <type_traits>

NS_BEGIN(Engine)

class ENGINE_DLL Shader final : public Component, public enable_shared_from_this<Shader> {

public:
    typedef struct tagShaderDesc : public COMPONENT_DESC
    {
        tagShaderDesc(){}
        tagShaderDesc(const wstring& vertextag, const D3D11_INPUT_ELEMENT_DESC* desc, uint32_t numPasses)
	        : m_VertexTag(vertextag), m_elementsDesc(desc), m_numPasses(numPasses) {}

        wstring m_VertexTag{};
        const D3D11_INPUT_ELEMENT_DESC* m_elementsDesc{nullptr};
        uint32_t m_numPasses{};
    } SHADER_DESC;
public:
    explicit Shader();
    explicit Shader(const ComPtr<ID3D11Device> &device, const ComPtr<ID3D11DeviceContext> &context);
    explicit Shader(const Shader& rhs);
    ~Shader() override = default;

public:
    COMPONENT_TYPE Get_ComponentType() const override { return COMPONENT_TYPE::SHADER; };
    HRESULT Initialize_Prototype(const tChar *shaderFilePath,
                                 const D3D11_INPUT_ELEMENT_DESC* elements,
                                 uint32 numElements);
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void *arg) override;
    void On_Destroy() override;

public:
    HRESULT Begin(uint32 passIndex);
    HRESULT Begin(const string& passName);
    Bool Has_Pass(const string& passName) const;
    Bool Has_SRV(const string& slotName) const;
    // Pass names in technique order and every texture (SRV) variable, for editors that author material settings.
    vector<string> Get_PassNames() const;
    vector<string> Get_SRVNames() const;
    // The shader resource this component was cloned from (SHADER_DESC::m_VertexTag).
    const wstring& Get_ShaderTag() const { return m_ShaderTag; }
    uint32 Get_NumPasses() const { return m_NumPasses; }
    HRESULT Clear_MaterialSlots(const vector<string>& slots);
    HRESULT Bind_SRV(const Char* constantName, const ComPtr<ID3D11ShaderResourceView> &srv);
    HRESULT Bind_Matrix(const Char* constantName, const Float4x4 *matrix);
    HRESULT Bind_Matrices(const Char* constantName, const Float4x4* matrices, uint32 numMatrices);
    HRESULT Bind_RawValue(const Char* constantName, const void* data, uint32 length);
    Bool Supports_CBuffer(ConstantBuffer type) const;

    template <typename Desc>
    HRESULT Bind_CBufferData(const Desc& desc)
    {
        static_assert(sizeof(Desc) % 16 == 0,
            "Constant buffer data size must be 16-byte aligned.");
        static_assert(std::is_trivially_copyable_v<Desc>,
            "Constant buffer data must be trivially copyable.");

        return Update_CBuffer(Desc::CBType, &desc, sizeof(Desc));
    }

private:
    // bufferSize 0 means size. A larger bufferSize uploads data first and zero-fills the rest (Skeletal leaves it unwritten).
    HRESULT Update_CBuffer(ConstantBuffer type, const void* data, size_t size, size_t bufferSize = 0);

private:
    ComPtr<ID3DX11Effect> m_Effect = { nullptr };
    uint32 m_NumPasses = {};
    wstring m_ShaderTag{};
    vector<ComPtr<ID3D11InputLayout>> m_InputLayouts;
    Shared<const unordered_map<string, uint32>> m_PassIndices;
    /* SRV 변수 이름 → Effect 변수. 재질을 바꿀 때마다 슬롯·텍스처 수만큼 이름으로 찾는데 Effects11 GetVariableByName은
       변수 목록을 문자열 비교로 훑으므로, 프로토타입이 한 번 모아 두고 클론이 공유한다(Effect도 공유한다). */
    struct NameHash
    {
        using is_transparent = void;
        size_t operator()(std::string_view name) const { return std::hash<std::string_view>{}(name); }
    };
    using SRVVariableMap = unordered_map<string, ID3DX11EffectShaderResourceVariable*, NameHash, std::equal_to<>>;
    Shared<const SRVVariableMap> m_SRVVariables;
    ID3DX11EffectShaderResourceVariable* Find_SRV(std::string_view slotName) const;
    // Effect state and the previous material's slots share the same clone lifetime.
    Shared<vector<string>> m_MaterialSlots{ make_shared<vector<string>>() };
    array<ComPtr<ID3DX11EffectConstantBuffer>, ETOI(ConstantBuffer::END)> m_ConstantBuffers{};
    array<ComPtr<ID3D11Buffer>, ETOI(ConstantBuffer::END)> m_AllocatedConstantBuffers{};
    array<uint32, ETOI(ConstantBuffer::END)> m_AllocatedConstantBufferSizes{};

public:
    static Shared<Shader> CreatePrototype();
	static Shared<Shader> Create(const ComPtr<ID3D11Device>& device,
                               const ComPtr<ID3D11DeviceContext>& context,
                               const tChar* shaderFilePath,
                               const D3D11_INPUT_ELEMENT_DESC* elements,
                               uint32 numElements);
	Shared<Component> Clone(void* arg) override;
};

NS_END
