#pragma once
#include "GameObject.h"

NS_BEGIN(Engine)

class Shader;
class Texture;
class VIBuffer_Rect;

class ENGINE_DLL UIObject abstract : public GameObject {
public:
    typedef struct tagUIObjectDesc : public GAMEOBJECT_DESC {
		UI_ANCHOR anchor = { UI_ANCHOR::CENTER };
		Float x{}, y{}, sizeX{}, sizeY{};
    } UI_DESC;

public:
    explicit UIObject();
    explicit UIObject(const ComPtr<ID3D11Device> &device, const ComPtr<ID3D11DeviceContext> &context);
    explicit UIObject(const UIObject& rhs);
    virtual ~UIObject() override = default;

public:
    Float Get_X() const { return m_Transform->Get_LocalPosition().x; }
    Float Get_Y() const { return m_Transform->Get_LocalPosition().y; }
    Float Get_SizeX() const { return m_Transform->Get_LocalScale().x; }
    Float Get_SizeY() const { return m_Transform->Get_LocalScale().y; }
    void Set_AnchorState(UI_ANCHOR anchor) { m_Anchor = anchor; Update_UITransform(); }
    UI_ANCHOR Get_AnchorState() const { return m_Anchor; }
    // The anchor as its UI_ANCHOR index, as reflection saves it; an index past BOTTOM_RIGHT is rejected.
    uint32 Get_AnchorIndex() const { return static_cast<uint32>(m_Anchor); }
    HRESULT Set_AnchorIndex(uint32 anchor);
    Vector2 Get_AnchorPos() const;
    Vector2 Get_AnchorPos(Float viewportWidth, Float viewportHeight) const;
    Matrix Get_UIWorldMatrix(Float viewportWidth, Float viewportHeight) const;

public:
    void Update_UITransform() const;
    void Update_UITransform(Float viewportWidth, Float viewportHeight) const;
    GAMEOBJECTTYPE Get_GameObjectType() final { return GAMEOBJECTTYPE::UI; }
    virtual HRESULT Initialize_Prototype() override;
    virtual HRESULT Initialize(void *arg) override;
    virtual void On_Destroy() override;
    virtual void On_Enable() override;
    virtual void On_Disable() override;


public:
    virtual void Priority_Update(Float timeDelta) override;
    virtual void Update(Float timeDelta) override;
    virtual void Late_Update(Float timeDelta) override;
    virtual void Fixed_Update(Float fixedDelta) override;
    virtual HRESULT Render() override;

protected:
    HRESULT Bind_ShaderResource(const Shared<Shader> &shader,
                              const Char *constantName,
                              D3DTS transformState) const;

protected:
    UI_ANCHOR m_Anchor = { UI_ANCHOR::CENTER };
    Float m_ViewportHeight{}, m_ViewportWidth{};
    Matrix m_TransformationMatrices[ETOI(D3DTS::END)];

public:
	virtual Shared<GameObject> Clone(void *arg = nullptr) override PURE;
};

NS_END
