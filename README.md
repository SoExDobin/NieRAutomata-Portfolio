# NieRAutomata-Portfolio

C++20 · DirectX 11 자체 엔진으로 만든 NieR:Automata 모작 개인 프로젝트의 소스 공개 저장소입니다.

- 엔진 · 게임 · 에디터 · 런처 · 모델 변환 도구의 소스만 담았습니다.
- **게임 리소스(모델 · 텍스처 · 사운드)와 씬 데이터는 포함하지 않습니다.** 원작 자산이기 때문입니다.
- 코드를 읽기 위한 저장소입니다. 이 저장소만으로는 빌드 · 실행되지 않습니다.

## 개요

| 항목 | 내용 |
| --- | --- |
| 개발 기간 | 1차 2026.01 ~ 04, 리팩토링 2026.09 ~ 10 |
| 제작 | 개인, 학습 목적 모작 |
| 개발 환경 | C++20, DirectX 11, HLSL |
| 라이브러리 | PhysX, Jolt, RTTR, Recast, FMOD |
| 담당 | Engine, Client, Editor, Launcher, ModelConverter |
| 개발 도구 | GitHub 이슈, PR, Actions 릴리즈 빌드, Obsidian 작업 기록 |

## 프로젝트 구성

| 폴더 | 역할 |
| --- | --- |
| `Engine/` | DirectX 11 엔진 DLL. 렌더러, 리소스, 오브젝트 · 컴포넌트, 리플렉션, 물리, 사운드 |
| `Client/` | 게임 로직. 플레이어 · 몬스터 · 보스, 상태 머신, 이펙트, UI, 레벨 |
| `Editor/` | ImGui 기반 에디터. Hierarchy · Inspector · 애니메이션 프리셋 · 모델 뷰어 |
| `Launcher/` | 게임 실행 파일 |
| `ModelConverter/` | FBX → 엔진 모델 · 애니메이션 바이너리 변환 도구 |
| `*/bin/shaders/` | HLSL 셰이더 (Effect 11) |

빌드 구성은 Debug · Development(최적화 + 디버그 도구) · Release 세 가지이고, 외부 라이브러리는 `vcpkg.json`으로 받습니다. FMOD SDK는 재배포 대상이 아니라 포함하지 않았습니다.

## 핵심 파일 지도

### 01 리플렉션 · GUID

Client가 RTTR 문법을 몰라도 타입을 등록하고, 저장된 GUID 참조를 복원합니다.

| 파일 | 내용 |
| --- | --- |
| `Engine/public/Engine_Reflection.h`, `Engine_ReflectionRegistration.h`, `Engine_RTTR_Metadata.h` | RTTR을 감춘 등록 · 메타데이터 계층, GUID 참조(ObjectRef) |
| `Engine/public/Object.h` | 리플렉션 기반 `Cast<T>` |
| `Engine/private/TypeCatalog.*`, `Engine/private/rttr_generated/` | 엔진 타입 카탈로그와 생성된 등록 코드 |
| `Client/private/reflection/`, `Client/private/ClientTypeRegistration.cpp` | Client 타입별 등록 파일 |
| `Engine/private/LevelSerializer.cpp`, `PrefabManager.cpp` | 씬 · 프리팹 저장과 복원 |
| `Editor/private/Inspector*.cpp`, `Hierarchy.cpp` | 리플렉션으로 그리는 에디터 편집 UI |

### 02 월드 렌더 성능

| 파일 | 내용 |
| --- | --- |
| `Engine/private/Renderer.cpp` | 프러스텀 컬링, 가림 컬링(1~2프레임 전 깊이를 줄여 CPU에서 읽는 Hi-Z, GPU를 기다리지 않음), 렌더 그룹 제출 |
| `Engine/private/Pipeline.cpp` | 카메라 행렬과 프러스텀 |
| `Engine/public/Mesh.h`, `StaticMesh.h`, `SkeletalMesh.h`, `InstanceMesh.h` | Mesh 공통 기반과 정적 · 스키닝 · 인스턴스 메시 분리 |
| `Engine/private/Model.cpp` (`Render_Meshes`) | 연속 재질 바인딩 생략 |
| `Engine/private/VIBuffer_Instance.cpp` | 반복 소품 · 이펙트 인스턴싱 |
| `Client/private/WorldMap.cpp` | 월드 타일과 거리 LOD 전환 |
| `Engine/private/JobManager.cpp`, `ResourceManager.cpp` | 셰이더 · 텍스처 · 모델 병렬 로딩 |

### 03 충돌 · 물리

| 파일 | 내용 |
| --- | --- |
| `Engine/private/PhysicsManager.cpp` | PhysX 씬 · 캐릭터 컨트롤러, Jolt 소프트바디를 감춘 관리자 |
| `Engine/private/WorldCollider.cpp` | 원작 COL 메시로 만든 지형 충돌 |
| `Engine/private/CollisionManager.cpp`, `*Collider.cpp` | 공격 · 시야 · 회피 판정을 PhysX 씬 쿼리로 처리 |
| `Engine/private/SoftBody.cpp` | 머리카락 · 치마 본 체인 소프트바디 |
| `Engine/private/Navigation*.cpp`, `NavCell.cpp` | 길찾기용 내비게이션 |
| `Client/private/ThirdPersonCamera.cpp` | 3인칭 카메라 벽 · 바닥 충돌 |

### 04 셰이딩

| 파일 | 내용 |
| --- | --- |
| `Engine/private/Renderer.cpp` | 패스 순서: G-buffer → SSAO → 볼류메트릭 → 조명 → 합성 → 블렌드 · 왜곡 → 발광 → 블룸 · 그레이딩 → UI |
| `Engine/private/RenderTarget*.cpp` | 렌더 타깃 · MRT 관리 |
| `Engine/private/LightManager.cpp`, `Light.cpp` | 방향광 · 점광원, 캐스케이드 그림자 |
| `Engine/public/Shader_CBuffer_Define.h`, `Engine_Rendering_Const.h` | 셰이더와 공유하는 상수 버퍼 정의 |
| `Client/bin/shaders/DeferredShader.hlsl`, `PostProcessShader.hlsl` | 디퍼드 조명 · SSAO · 후처리 |
| `Client/bin/shaders/Shader_VtxMesh_Map.hlsl`, `Shader_VtxMeshInstance_Map.hlsl` | 월드 타일 재질. 원작 셰이더 그래프를 패스별로 재구성 |
| `Client/bin/shaders/EngineShader*.hlsli` | 셰이더 공용 상수 버퍼 · 재질 · 정점 정의 |

### 05 애니메이션 · 모델 파이프라인

| 파일 | 내용 |
| --- | --- |
| `ModelConverter/Converter.cpp` | FBX → 엔진 모델 · 애니메이션 바이너리 변환 |
| `ModelConverter/Nier_Fbx_Rule.cpp` | 월드 FBX 메시 역할 분류(본체 · LOD · COL · 인스턴스) |
| `Engine/private/Model.cpp`, `Animation.cpp`, `Channel.cpp`, `Bone.cpp` | 모델 로드, 키프레임 샘플링, 본 계층 |
| `Engine/private/AnimationTracker.cpp` | 애니메이션 재생 · 블렌드 상태 |
| `Editor/private/AnimationPresetEditor.cpp` | 상태별 클립 · 노티파이를 묶는 애니메이션 프리셋 편집 |
| `Editor/private/ModelViewer.cpp`, `AssetBrowser.cpp` | 모델 확인 · 에셋 탐색 |

### 06 게임플레이

| 파일 | 내용 |
| --- | --- |
| `Client/private/Pl0000*.cpp`, `State2B_*.cpp`, `StateA2_*.cpp` | 플레이어 입력 · 이동 · 상태 머신, 지상 · 공중 콤보, 회피 판정 |
| `Client/private/Em*.cpp`, `StateEm*.cpp` | 일반 몬스터와 보스(Em3000, Em8000) 상태 · 패턴 |
| `Client/private/EmMovement.cpp`, `EmLaser.cpp`, `EmRing.cpp` | 보스들이 함께 쓰는 루트 모션 이동 · 레이저 · 링 패턴 |
| `Client/private/MonsterSpawner.cpp` | 씬에 배치하는 몬스터 스포너 |
| `Client/private/CameraEventSystem.cpp`, `EmCutscene.cpp` | 이벤트 카메라 · 보스 등장 연출 |
| `Client/private/CommunicationUI.cpp` | Pod 통신 UI · 자막 |
| `Engine/private/SoundManager.cpp`, `SoundEmitter.cpp` | FMOD 사운드, 거리 감쇠 발음체 |
