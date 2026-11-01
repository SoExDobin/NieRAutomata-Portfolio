#pragma once
#include "EditorObject.h"
#include "Model.h"
#include <filesystem>

struct ImDrawList;

NS_BEGIN(Editor)

class ModelViewer final : public EditorObject
{
public:
	// Diffuse/Normal/Material are the pass outputs (PS_GBUFFER_OUT order); Lit shades them with a headlight.
	enum class PreviewBuffer : int32 { Diffuse, Normal, Material, Lit, END };
	static constexpr uint32 PreviewGBufferCount = static_cast<uint32>(PreviewBuffer::Lit);
	// How an image is drawn: RGB ignores alpha so G-buffer/albedo alpha cannot hide the picture.
	enum class ImageChannel : int32 { RGB, R, G, B, A, RGBA, END };

public:
	ModelViewer();
	~ModelViewer() override = default;

public:
	HRESULT Initialize() override;
	void Update(Bool isResize) override;
	void Render(Bool isResize) override;
	Shared<Engine::Model> Get_SelectedModel() const { return m_pSelectedModel; }
	HRESULT Preview_Model(const Shared<Engine::Model>& model, ComPtr<ID3D11ShaderResourceView>& image,
		int32 meshIndex = -1, uint32 width = 384, uint32 height = 256);
	// Scripted captures (gate "model-preview") pick the camera and G-buffer the UI would.
	void Set_PreviewView(Float yaw, Float pitch, Float zoom, PreviewBuffer buffer)
	{
		m_PreviewYaw = yaw; m_PreviewPitch = pitch; m_PreviewZoom = zoom; m_PreviewBuffer = buffer;
	}
	void Set_PreviewPan(Float x, Float y) { m_PreviewPan = Engine::Vector2(x, y); }
	static HRESULT Save_MaterialSettings(const Shared<Engine::Model>& model,
		const Engine::MODEL_MATERIAL_SETTINGS& draft, const filesystem::path& target, string& status);

private:
	void View_ModelList();
	void View_AnimationList();
	void View_MaterialSettings();
	void View_PreviewPanel();
	void View_PreviewToolbar();
	void View_Preview(Float width, Float height);
	void View_TextureViewer();
	void Draw_Image(ImDrawList* drawList, ID3D11ShaderResourceView* image, Float minX, Float minY,
		Float maxX, Float maxY, ImageChannel channel);
	HRESULT Create_PreviewTargets(uint32 width, uint32 height);
	HRESULT Shade_Preview(const Engine::Vector3& toLight, const Engine::Vector3& toEye);
	Float Compute_PreviewDistance(const BoundingBox& bounds, Float aspect) const;
	void Reset_PreviewView();
	void Handle_KeyInput();
	// Clips live in AnimationPreset documents, not the .model; "<ModelTag>_*.json" is picked when a model is selected.
	void Select_AnimationPreset();
	void Apply_AnimationPreset();
	void Load_MaterialDraft();
	void Generate_MaterialDraft();
	void Save_MaterialDraft();
	void Apply_DraftPreview(Bool force);
	void Refresh_ModelTextureFiles();
	filesystem::path Get_MaterialSettingsPath() const;

private:
	wstring m_SelectedModelTag = L"";
	int32 m_SelectedAnimIndex = -1;
	// Plays the selected clip on the previewed prototype model; clones copy their own clips, so placed objects are unaffected.
	Bool m_IsPlaying{ true };
	Float m_PlaySpeed{ 1.f };
	filesystem::path m_SelectedPresetPath;
	string m_PresetStatus;
	int32 m_PreviewMeshIndex = -1;
	int32 m_CurrentLevelIndex = 0;

	Shared<Engine::Model> m_pSelectedModel = nullptr;
	Engine::MODEL_MATERIAL_SETTINGS m_MaterialDraft{};
	Bool m_HasMaterialDraft{ false };
	string m_MaterialStatus{};
	// Live preview: the draft is prepared from a temp file and applied to the model without saving.
	Bool m_LiveDraftPreview{ true };
	string m_AppliedDraftDump{};
	string m_DraftPreviewStatus{};
	Weak<Engine::Model> m_PreviewModel;
	Shared<Engine::Shader> m_PreviewShader;
	Shared<Engine::Shader> m_PreviewSourceShader;
	// Diffuse, Normal, Material, Lit render targets, recreated when the preview size changes.
	ComPtr<ID3D11RenderTargetView> m_PreviewTargets[ETOI(PreviewBuffer::END)];
	ComPtr<ID3D11ShaderResourceView> m_PreviewImages[ETOI(PreviewBuffer::END)];
	ComPtr<ID3D11DepthStencilView> m_PreviewDepth;
	uint32 m_PreviewWidth{};
	uint32 m_PreviewHeight{};
	PreviewBuffer m_PreviewBuffer{ PreviewBuffer::Lit };
	// Full-screen headlight pass over the preview G-buffer (compiled on first use).
	ComPtr<ID3D11VertexShader> m_LitVertexShader;
	ComPtr<ID3D11PixelShader> m_LitPixelShader;
	ComPtr<ID3D11Buffer> m_LitConstants;
	Bool m_LitShaderTried{ false };
	// Orbit camera around the (pre-transformed) model bounds. Pan is in camera right/up world units.
	Float m_PreviewYaw{};
	Float m_PreviewPitch{ 0.1f };
	Float m_PreviewZoom{ 1.f };
	Engine::Vector2 m_PreviewPan{};
	Float m_PreviewHeightInline{ 520.f };
	Bool m_PreviewPoppedOut{ false };
	Bool m_ColumnsInitialized{ false };
	// Material texture slot thumbnails keyed by resource path; null marks a path that did not load.
	map<string, ComPtr<ID3D11ShaderResourceView>> m_TexturePreviews;
	Float m_ThumbnailSize{ 128.f };
	// Resource-relative paths of Models/<tag>/Textures files for the "Pick Texture" combo.
	vector<string> m_ModelTextureFiles;
	string m_InspectTexturePath{};
	string m_InspectTextureSlot{};
	ImageChannel m_InspectChannel{ ImageChannel::RGB };
	Bool m_ShowTextureViewer{ false };
	// ImGui pixel shaders that swizzle one channel to grey (index = ImageChannel, RGBA uses ImGui's own).
	ComPtr<ID3D11PixelShader> m_ChannelShaders[ETOI(ImageChannel::RGBA)];
	Bool m_ChannelShadersTried{ false };

public:
	static Shared<ModelViewer> Create();
};

NS_END
