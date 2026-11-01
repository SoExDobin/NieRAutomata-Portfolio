#pragma once
#include "Em0000Body.h"

NS_BEGIN(Client)

// The golden rabbit statue's body (emb001: em0000's skeleton plus a trumpet bone). Its attacks are the variant clips of
// emb001.dat (em0000_<mot>_1_seq): a headbutt, a straight punch and the windmill of spinning arms.
class CLIENT_DLL Emb001Body final : public Em0000Body
{
public:
	explicit Emb001Body();
	explicit Emb001Body(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Emb001Body(const Emb001Body& rhs);
	~Emb001Body() override = default;

protected:
	const tChar* Get_ModelTag() const override { return L"emb001"; }
	void Ready_MoveNotify() override;
	void Ready_AttackNotify() override;

public:
	static Shared<Emb001Body> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;
};

NS_END
