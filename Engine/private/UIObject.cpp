#include "UIObject.h"

#include "Game.h"
#include "Shader.h"

UIObject::UIObject() : GameObject() {}

UIObject::UIObject(const ComPtr<ID3D11Device> &device, const ComPtr<ID3D11DeviceContext> &context)
    : GameObject{device, context} {}

UIObject::UIObject(const UIObject& rhs)
    : GameObject{rhs}, m_Anchor(rhs.m_Anchor),
      m_ViewportHeight(rhs.m_ViewportHeight), m_ViewportWidth(rhs.m_ViewportWidth), m_TransformationMatrices{}
{
    m_TransformationMatrices[ETOI(D3DTS::VIEW)] = rhs.m_TransformationMatrices[ETOI(D3DTS::VIEW)];
    m_TransformationMatrices[ETOI(D3DTS::PROJ)] = rhs.m_TransformationMatrices[ETOI(D3DTS::PROJ)];
}

HRESULT UIObject::Initialize_Prototype() {
  return GameObject::Initialize_Prototype();
}

HRESULT UIObject::Initialize(void *arg) {
	if (FAILED(GameObject::Initialize(arg)))
		return E_FAIL;

    if (arg != nullptr) {
        UI_DESC* desc = static_cast<UI_DESC*>(arg);
        m_Anchor = desc->anchor;

        // Desc 값을 바로 Transform에 저장
        m_Transform->Set_LocalPosition(desc->x, desc->y, 0.f);
        m_Transform->Set_LocalScale(desc->sizeX, desc->sizeY, 1.f);
    }

    D3D11_VIEWPORT viewPortDesc = GAME_INSTANCE->Get_ViewportDesc();
    uint32 numViewPorts{1};
    m_ViewportWidth = viewPortDesc.Width;
    m_ViewportHeight = viewPortDesc.Height;

    Update_UITransform();

    XMStoreFloat4x4(&m_TransformationMatrices[ETOI(D3DTS::VIEW)], Matrix::Identity);
    XMStoreFloat4x4(&m_TransformationMatrices[ETOI(D3DTS::PROJ)],
        XMMatrixOrthographicLH(m_ViewportWidth, m_ViewportHeight, 0.f, 1.f));

    return S_OK;
}

void UIObject::On_Destroy()                 { GameObject::On_Destroy(); }
void UIObject::On_Enable()                  { GameObject::On_Enable(); }
void UIObject::On_Disable()                 { GameObject::On_Disable(); }
void UIObject::Priority_Update(Float timeDelta) {
	GameObject::Priority_Update(timeDelta);
}
void UIObject::Update(Float timeDelta) {
	Update_UITransform();
}
void UIObject::Late_Update(Float timeDelta) {
	
}
void UIObject::Fixed_Update(Float fixedDelta) {
	
}
HRESULT UIObject::Render() {
	return GameObject::Render();
}

HRESULT UIObject::Set_AnchorIndex(uint32 anchor) {
    if (anchor > static_cast<uint32>(UI_ANCHOR::BOTTOM_RIGHT))
        return E_INVALIDARG;
    Set_AnchorState(static_cast<UI_ANCHOR>(anchor));
    return S_OK;
}

void UIObject::Update_UITransform() const {
    Update_UITransform(m_ViewportWidth, m_ViewportHeight);
}

void UIObject::Update_UITransform(Float viewportWidth, Float viewportHeight) const {
    if (false == m_IsActive || true == m_IsDestroy) return;
    m_Transform->Set_WorldMatrix(Get_UIWorldMatrix(viewportWidth, viewportHeight));
}

Matrix UIObject::Get_UIWorldMatrix(Float viewportWidth, Float viewportHeight) const {

    Float baseWidth = viewportWidth;
    Float baseHeight = viewportHeight;

    Vector2 anchorPos = Get_AnchorPos(baseWidth, baseHeight);
    Float anchorPosX = anchorPos.x;
    Float anchorPosY = anchorPos.y;

    Vector3 offset = m_Transform->Get_LocalPosition();
    Vector3 size = m_Transform->Get_LocalScale();
    Quaternion rotation = m_Transform->Get_LocalRotation();
    
    Vector3 finalPos;
    finalPos.x = (offset.x + anchorPosX) - (baseWidth * 0.5f);
    finalPos.y = -(offset.y + anchorPosY) + (baseHeight * 0.5f);
    finalPos.z = 0.f;
    
    Matrix matScale = Matrix::CreateScale(size.x, size.y, 1.f);
    Matrix matRotation = Matrix::CreateFromQuaternion(rotation);
    Matrix matTranslation = Matrix::CreateTranslation(finalPos);

    if (const auto parent = std::dynamic_pointer_cast<UIObject>(Get_Parent())) {
        // Parent scale is its pixel rectangle, not a multiplier on child pixels.
        const Float width = parent->Get_SizeX();
        const Float height = parent->Get_SizeY();
        if (fabsf(width) > 0.0001f && fabsf(height) > 0.0001f) {
            finalPos = Vector3(offset.x + anchorPosX - width * 0.5f,
                -offset.y - anchorPosY + height * 0.5f, 0.f);
            return matScale * matRotation * Matrix::CreateTranslation(finalPos) *
                Matrix::CreateScale(1.f / width, 1.f / height, 1.f) *
                parent->Get_UIWorldMatrix(viewportWidth, viewportHeight);
        }
    }
    return matScale * matRotation * matTranslation;
}

Vector2 UIObject::Get_AnchorPos() const {
    return Get_AnchorPos(m_ViewportWidth, m_ViewportHeight);
}

Vector2 UIObject::Get_AnchorPos(Float viewportWidth, Float viewportHeight) const {
    Float baseWidth = viewportWidth;
    Float baseHeight = viewportHeight;
    auto parent = Get_Parent();
    if (parent) {
        auto uiParent = std::dynamic_pointer_cast<UIObject>(parent);
        if (uiParent) {
            baseWidth = uiParent->Get_SizeX();
            baseHeight = uiParent->Get_SizeY();
        }
    }

    Float anchorRatioX = 0.5f;
    Float anchorRatioY = 0.5f;

    switch (m_Anchor) {
    case UI_ANCHOR::TOP_LEFT:      anchorRatioX = 0.0f; anchorRatioY = 0.0f; break;
    case UI_ANCHOR::TOP_CENTER:    anchorRatioX = 0.5f; anchorRatioY = 0.0f; break;
    case UI_ANCHOR::TOP_RIGHT:     anchorRatioX = 1.0f; anchorRatioY = 0.0f; break;
    case UI_ANCHOR::CENTER_LEFT:   anchorRatioX = 0.0f; anchorRatioY = 0.5f; break;
    case UI_ANCHOR::CENTER:        anchorRatioX = 0.5f; anchorRatioY = 0.5f; break;
    case UI_ANCHOR::CENTER_RIGHT:  anchorRatioX = 1.0f; anchorRatioY = 0.5f; break;
    case UI_ANCHOR::BOTTOM_LEFT:   anchorRatioX = 0.0f; anchorRatioY = 1.0f; break;
    case UI_ANCHOR::BOTTOM_CENTER: anchorRatioX = 0.5f; anchorRatioY = 1.0f; break;
    case UI_ANCHOR::BOTTOM_RIGHT:  anchorRatioX = 1.0f; anchorRatioY = 1.0f; break;
    }

    return Vector2(baseWidth * anchorRatioX, baseHeight * anchorRatioY);
}

HRESULT UIObject::Bind_ShaderResource(const Shared<Shader> &shader,
                                      const Char *constantName,
                                      D3DTS transformState) const {
	return shader->Bind_Matrix(constantName, &m_TransformationMatrices[ETOI(transformState)]);
}

