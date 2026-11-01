#include "Shader.h"
#include "Game.h"
#include "SpdLogger.h"

Shader::Shader() : Component{} {}
Shader::Shader(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
    : Component(device, context) {}

Shader::Shader(const Shader& rhs)
    : Component(rhs), m_Effect{rhs.m_Effect}, m_NumPasses{rhs.m_NumPasses},
      m_InputLayouts{rhs.m_InputLayouts}, m_PassIndices{rhs.m_PassIndices}, m_SRVVariables{rhs.m_SRVVariables},
      m_MaterialSlots{rhs.m_MaterialSlots}, m_ConstantBuffers{rhs.m_ConstantBuffers} {}

HRESULT Shader::Initialize_Prototype(const tChar* shaderFilePath, const D3D11_INPUT_ELEMENT_DESC* elements, uint32 numElements) 
{
    uint32 hlslFlag = {};

#ifdef _DEBUG
    hlslFlag |= D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_DEBUG;
#else
    hlslFlag |= D3DCOMPILE_OPTIMIZATION_LEVEL1;
#endif

    /* A silent failure here used to surface much later as a null dereference inside
       Bind_Matrix, with nothing in the log. Always name the shader and the HRESULT. */
    if (const HRESULT compiled = D3DX11CompileEffectFromFile(
        shaderFilePath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, hlslFlag,
        0, m_Device.Get(), m_Effect.GetAddressOf(), nullptr); FAILED(compiled))
    {
        LOG_ERROR(L"[Shader] Failed to compile effect '{}' (HRESULT 0x{:08X})",
            shaderFilePath ? shaderFilePath : L"<null>", static_cast<uint32>(compiled));
        return E_FAIL;
    }

    if (ID3DX11EffectTechnique* technique = m_Effect->GetTechniqueByIndex(0);
        technique && technique->IsValid())
    {
        Shader::SHADER_DESC desc;
    	D3DX11_TECHNIQUE_DESC techniqueDesc = {};
    	
        if (FAILED(technique->GetDesc(&techniqueDesc)) || techniqueDesc.Passes == 0)
            return E_FAIL;
        m_NumPasses = techniqueDesc.Passes;
        auto passIndices = make_shared<unordered_map<string, uint32>>();
        m_InputLayouts.reserve(m_NumPasses);

        for (uint32 i = 0; i < m_NumPasses; ++i) 
        {
            ComPtr<ID3D11InputLayout> inputLayout = {nullptr};
            ID3DX11EffectPass* pass = technique->GetPassByIndex(i);
            if (nullptr == pass || !pass->IsValid())
            {
                LOG_ERROR(L"[Shader] '{}' pass {} is invalid",
                    shaderFilePath ? shaderFilePath : L"<null>", i);
                return E_FAIL;
            }

            D3DX11_PASS_DESC passDesc = {};
            if (FAILED(pass->GetDesc(&passDesc)) || !passDesc.Name || !*passDesc.Name ||
                !passIndices->emplace(passDesc.Name, i).second)
            {
                LOG_ERROR(L"[Shader] '{}' pass {} has a missing or duplicate name",
                    shaderFilePath ? shaderFilePath : L"<null>", i);
                return E_FAIL;
            }

            if (FAILED(m_Device->CreateInputLayout(
            elements, numElements, passDesc.pIAInputSignature,
            passDesc.IAInputSignatureSize, inputLayout.GetAddressOf())))
            {
                LOG_ERROR(L"[Shader] '{}' pass {} input layout creation failed",
                    shaderFilePath ? shaderFilePath : L"<null>", i);
                return E_FAIL;
            }

            
            desc.m_elementsDesc = elements;
            desc.m_numPasses = numElements;
            m_ObjectDesc = &desc;
            m_InputLayouts.push_back(inputLayout);
        }

        m_PassIndices = std::move(passIndices);

        auto srvVariables = make_shared<SRVVariableMap>();
        D3DX11_EFFECT_DESC effectDesc{};
        if (SUCCEEDED(m_Effect->GetDesc(&effectDesc)))
            for (uint32 i = 0; i < effectDesc.GlobalVariables; ++i)
            {
                ID3DX11EffectVariable* variable = m_Effect->GetVariableByIndex(i);
                D3DX11_EFFECT_VARIABLE_DESC variableDesc{};
                if (variable && variable->IsValid() && variable->AsShaderResource()->IsValid() &&
                    SUCCEEDED(variable->GetDesc(&variableDesc)) && variableDesc.Name)
                    srvVariables->emplace(variableDesc.Name, variable->AsShaderResource());
            }
        m_SRVVariables = std::move(srvVariables);
        for (uint32 i = 0; i < ETOI(ConstantBuffer::END); ++i)
        {
            ID3DX11EffectConstantBuffer* constantBuffer =
                m_Effect->GetConstantBufferByName(ShaderCB::Names[i].data());

            if (nullptr != constantBuffer && constantBuffer->IsValid())
                m_ConstantBuffers[i] = constantBuffer;
        }
    } else
    return E_FAIL;

	return Component::Initialize_Prototype();
}

HRESULT Shader::Initialize_Prototype()
{
	return Component::Initialize_Prototype();
}

HRESULT Shader::Initialize(void* arg)
{
	return Component::Initialize(arg);
}

void Shader::On_Destroy() {
  m_InputLayouts.clear();

  for (uint32 i = 0; i < ETOI(ConstantBuffer::END); ++i)
  {
      m_AllocatedConstantBuffers[i].Reset();
      m_ConstantBuffers[i].Reset();
      m_AllocatedConstantBufferSizes[i] = 0;
  }

  m_Effect.Reset();

  Component::On_Destroy();
}

HRESULT Shader::Begin(uint32 passIndex) {
    if (!m_Effect || passIndex >= m_NumPasses || passIndex >= m_InputLayouts.size() ||
        nullptr == m_InputLayouts[passIndex])
      return E_INVALIDARG;

    // Effects11 leaves a stage the pass does not name as it was, so a particle pass's GS would stick to later draws.
    m_Context->GSSetShader(nullptr, nullptr, 0);
    if (FAILED(m_Effect->GetTechniqueByIndex(0)->GetPassByIndex(passIndex)->Apply(0, m_Context.Get())))
        return E_FAIL;
    m_Context->IASetInputLayout(m_InputLayouts[passIndex].Get());

    return S_OK;
}

Bool Shader::Has_Pass(const string& passName) const
{
    return m_Effect && m_PassIndices && m_PassIndices->contains(passName);
}

HRESULT Shader::Begin(const string& passName)
{
    if (!Has_Pass(passName)) return E_INVALIDARG;
    return Begin(m_PassIndices->at(passName));
}

Bool Shader::Has_SRV(const string& slotName) const
{
    return nullptr != Find_SRV(slotName);
}

ID3DX11EffectShaderResourceVariable* Shader::Find_SRV(std::string_view slotName) const
{
    if (!m_Effect || !m_SRVVariables || slotName.empty()) return nullptr;
    const auto found = m_SRVVariables->find(slotName);
    return found == m_SRVVariables->end() ? nullptr : found->second;
}

vector<string> Shader::Get_PassNames() const
{
    vector<pair<uint32, string>> ordered;
    if (m_PassIndices)
        for (const auto& [name, index] : *m_PassIndices) ordered.emplace_back(index, name);
    std::ranges::sort(ordered);
    vector<string> names;
    names.reserve(ordered.size());
    for (auto& [index, name] : ordered) names.push_back(std::move(name));
    return names;
}

vector<string> Shader::Get_SRVNames() const
{
    vector<string> names;
    D3DX11_EFFECT_DESC effectDesc{};
    if (!m_Effect || FAILED(m_Effect->GetDesc(&effectDesc))) return names;
    for (uint32 i = 0; i < effectDesc.GlobalVariables; ++i)
    {
        ID3DX11EffectVariable* variable = m_Effect->GetVariableByIndex(i);
        D3DX11_EFFECT_VARIABLE_DESC variableDesc{};
        if (variable && variable->IsValid() && variable->AsShaderResource()->IsValid() &&
            SUCCEEDED(variable->GetDesc(&variableDesc)) && variableDesc.Name)
            names.emplace_back(variableDesc.Name);
    }
    return names;
}

HRESULT Shader::Clear_MaterialSlots(const vector<string>& slots)
{
    for (const string& slot : slots)
        if (!Has_SRV(slot)) return E_INVALIDARG;
    // 같은 슬롯 목록을 쓰는 재질이 이어지는 경우(한 모델의 재질들)가 대부분이므로 합집합·목록 복사 없이 비운다.
    if (*m_MaterialSlots == slots)
    {
        for (const string& slot : slots)
            if (FAILED(Bind_SRV(slot.c_str(), nullptr))) return E_FAIL;
        return S_OK;
    }
    vector<string> clearSlots = *m_MaterialSlots;
    for (const string& slot : slots)
        if (std::ranges::find(clearSlots, slot) == clearSlots.end()) clearSlots.push_back(slot);
    // Keep the union on failure so a retry clears partially changed Effect state.
    *m_MaterialSlots = clearSlots;
    for (const string& slot : clearSlots)
        if (FAILED(Bind_SRV(slot.c_str(), nullptr))) return E_FAIL;
    *m_MaterialSlots = slots;
    return S_OK;
}

HRESULT Shader::Bind_SRV(const Char *constantName, const ComPtr<ID3D11ShaderResourceView> &srv) 
{
    ID3DX11EffectShaderResourceVariable* srvVariable = constantName ? Find_SRV(constantName) : nullptr;
    if (nullptr == srvVariable) return E_INVALIDARG;

    return srvVariable->SetResource(srv.Get());
}

HRESULT Shader::Bind_Matrix(const Char *constantName, const Float4x4 *matrix) 
{
    if (!m_Effect) return S_OK;

    if (nullptr == constantName || nullptr == matrix)
        return E_INVALIDARG;

    if (0 == strcmp(constantName, WorldMatrix) && Supports_CBuffer(ConstantBuffer::Object))
    {
        ObjectCB objectBuffer{};
        objectBuffer.worldMatrix = *matrix;
        return Bind_CBufferData(objectBuffer);
    }

	ID3DX11EffectVariable* variable = m_Effect->GetVariableByName(constantName);
    if (nullptr == variable) {
		MSG_BOX("Failed To Throw Value To Shader");
		return E_FAIL;
    }

	ID3DX11EffectMatrixVariable* matrixVariable = variable->AsMatrix();
    if (nullptr == matrixVariable) {
        MSG_BOX("Matrix Types Do Not Match");
        return E_FAIL;
    }

    return matrixVariable->SetMatrix(reinterpret_cast<const Float*>(matrix));
}

HRESULT Shader::Bind_Matrices(const Char* constantName, const Float4x4* matrices, uint32 numMatrices)
{
    if (nullptr != constantName && 0 == strcmp(constantName, BoneMatrices) && Supports_CBuffer(ConstantBuffer::Skeletal))
    {
        if (nullptr == matrices || numMatrices > MODEL_BONE_MAX)
            return E_INVALIDARG;
        return Update_CBuffer(ConstantBuffer::Skeletal, matrices, sizeof(Float4x4) * numMatrices, sizeof(SkeletalCB));
    }

    ID3DX11EffectVariable* variable = m_Effect->GetVariableByName(constantName);
    if (nullptr == variable)
    {
        MSG_BOX("Failed to throw value to shader");
        LOG_ERROR(L"Failed to throw value to shader {}", Helper::To_wString(constantName));
        return E_FAIL;
    }

  	ID3DX11EffectMatrixVariable* matrixVariable = variable->AsMatrix();
    if (nullptr == matrixVariable)
    {
        MSG_BOX("Shader types do not match");
        LOG_ERROR(L"Shader types do not match {}", Helper::To_wString(constantName));
        return E_FAIL;
    }

    return matrixVariable->SetMatrixArray(reinterpret_cast<const Float*>(matrices), 0, numMatrices);
}

HRESULT Shader::Bind_RawValue(const Char* constantName, const void* data, uint32 length)
{
    if (!m_Effect) return S_OK;

    ID3DX11EffectVariable* variable = m_Effect->GetVariableByName(constantName);
    if (nullptr == variable) {
        MSG_BOX("Failed To Throw Value To Shader");
        return E_FAIL;
    }

	return variable->SetRawValue(data, 0, length);
}

Bool Shader::Supports_CBuffer(ConstantBuffer type) const
{
    const uint32 index = ETOI(type);
    return index < ETOI(ConstantBuffer::END) &&
        nullptr != m_ConstantBuffers[index] && m_ConstantBuffers[index]->IsValid();
}

HRESULT Shader::Update_CBuffer(ConstantBuffer type, const void* data, size_t size, size_t bufferSize)
{
    const size_t dataSize = size;
    size = (0 == bufferSize) ? dataSize : bufferSize;

    if (nullptr == data || 0 == size || dataSize > size || 0 != size % 16)
        return E_INVALIDARG;

    if (size > D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16ull)
        return E_INVALIDARG;

    const uint32 index = ETOI(type);
    if (index >= ETOI(ConstantBuffer::END))
        return E_INVALIDARG;

    if (!Supports_CBuffer(type))
        return S_OK;

    if (nullptr == m_AllocatedConstantBuffers[index] ||
        m_AllocatedConstantBufferSizes[index] != size)
    {
        m_AllocatedConstantBuffers[index].Reset();
        m_AllocatedConstantBufferSizes[index] = 0;

        D3D11_BUFFER_DESC bufferDesc{};
        bufferDesc.ByteWidth = static_cast<UINT>(size);
        bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        if (FAILED(m_Device->CreateBuffer(
            &bufferDesc, nullptr, m_AllocatedConstantBuffers[index].GetAddressOf())))
            return E_FAIL;

        m_AllocatedConstantBufferSizes[index] = static_cast<uint32>(size);
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(m_Context->Map(
        m_AllocatedConstantBuffers[index].Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return E_FAIL;

    memcpy(mapped.pData, data, dataSize);
    // SkeletalMesh keeps every vertex palette index below the uploaded count, so the bone palette tail is never read.
    if (dataSize < size && ConstantBuffer::Skeletal != type)
        memset(static_cast<uint8*>(mapped.pData) + dataSize, 0, size - dataSize);
    m_Context->Unmap(m_AllocatedConstantBuffers[index].Get(), 0);

    return m_ConstantBuffers[index]->SetConstantBuffer(
        m_AllocatedConstantBuffers[index].Get());
}

Shared<Shader> Shader::CreatePrototype()
{
    auto shader = make_shared<Shader>(
        GAME_INSTANCE->Get_Device(), GAME_INSTANCE->Get_Context());

    if (FAILED(shader->Initialize_Prototype())) {
        MSG_BOX("Failed To Create : Shader");
    }

    return shader;
}

Shared<Shader> Shader::Create(const ComPtr<ID3D11Device> &device,
                              const ComPtr<ID3D11DeviceContext> &context,
                              const tChar *shaderFilePath,
                              const D3D11_INPUT_ELEMENT_DESC *elements,
                              uint32 numElements) 
{
    auto shader = make_shared<Shader>(device, context);
    if (FAILED(shader->Initialize_Prototype(shaderFilePath, elements, numElements)))
        return nullptr;

    return shader;
}

Shared<Component> Shader::Clone(void* arg) {
    if (arg == nullptr)
    {
        LOG_ERROR(L"There is no ShaderDesc");
        MSG_BOX("There is no ShaderDesc");
        return nullptr;
    }

    SHADER_DESC& desc = *static_cast<SHADER_DESC*>(arg);
    auto resShader = GAME_INSTANCE->Get_Shader(0, desc.m_VertexTag.c_str());
    if (!resShader) return nullptr;

	auto shader = make_shared<Shader>(*resShader.get());
	shader->m_ShaderTag = desc.m_VertexTag;

    if (FAILED(shader->Initialize(arg))) {
    	MSG_BOX("Failed To Cloned : Shader");
    }
    return shader;
}
