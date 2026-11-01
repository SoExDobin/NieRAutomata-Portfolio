#pragma once
#include "EditorObject.h"

NS_BEGIN(Engine)
class GameObject;
NS_END

NS_BEGIN(Editor)

class Hierarchy : public EditorObject {
  NO_COPY(Hierarchy)
public:
  Hierarchy();
  ~Hierarchy() override;

public:
  HRESULT Initialize() override;
  void Update(Bool isResize) override;
  void Render(Bool isResize) override;

private:
  void Render_Node(const Shared<Engine::GameObject> &pObj, uint32 levelIndex);
  void Delete_Selected();

private:
  // 이름을 바꾸는 중인 노드. 비어 있으면 편집 중이 아니다.
  ObjectGuid m_RenamingGuid{};
  Char m_RenameBuffer[256]{};
  Bool m_RenameFocusPending{ false };

public:
  static Shared<Hierarchy> Create();
};

NS_END
