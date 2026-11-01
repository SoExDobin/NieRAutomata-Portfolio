#pragma once
#include "EditorObject.h"

NS_BEGIN(Editor)

class AssetBrowser final : public EditorObject
{
	NO_COPY(AssetBrowser)
public:
	AssetBrowser();
	~AssetBrowser() override = default;

public:
	HRESULT Initialize() override;
	void Update(Bool isResize) override;
	void Render(Bool isResize) override;

private:
	void Render_PrototypeList();
	void Render_PrototypeItem(const Shared<GameObject> &pProto, const wstring& prototypeTag);
	// Returns true when the item was double-clicked.
	Bool Render_ResourceItem(const wstring& tag, const string& assetType);

public:
	static Shared<AssetBrowser> Create();
};

NS_END
