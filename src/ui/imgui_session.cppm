module;

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

export module occt_lab.ui.imgui_session;

import std;

export class ImGuiSession
{
  public:
    explicit ImGuiSession(SDL_Window *window);
    ~ImGuiSession();
    ImGuiSession(const ImGuiSession &) = delete;
    ImGuiSession &operator=(const ImGuiSession &) = delete;

    void initialize_renderer(ImGui_ImplVulkan_InitInfo &info) const;
    void begin_frame() const;

  private:
    SDL_Window *window_;
    std::unique_ptr<ImGuiContext, decltype(&ImGui::DestroyContext)> context_{nullptr, ImGui::DestroyContext};
};

namespace
{
void configure_imgui_fonts(ImGuiIO &io)
{
    ImFontConfig font_config;
#ifdef _WIN32
    if (const auto *windows_dir = SDL_getenv("SystemRoot"); windows_dir && *windows_dir)
    {
        const auto path = std::format("{}/Fonts/segoeui.ttf", windows_dir);
        font_config.Flags |= ImFontFlags_NoLoadError;
        io.FontDefault = io.Fonts->AddFontFromFileTTF(path.c_str(), 0.0F, &font_config);
    }
#endif
    if (!io.FontDefault)
    {
        io.FontDefault = io.Fonts->AddFontDefaultVector(&font_config);
    }
    ImGui::GetStyle().FontSizeBase = 17.0F;
}

void shutdown_renderer()
{
    // The backend can register itself before creating all of its device objects.
    if (ImGui::GetIO().BackendRendererUserData)
    {
        ImGui_ImplVulkan_Shutdown();
    }
}
} // namespace

ImGuiSession::ImGuiSession(SDL_Window *window) : window_(window)
{
    IMGUI_CHECKVERSION();
    context_.reset(ImGui::CreateContext());
    ImGui::SetCurrentContext(context_.get());
    auto &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();
    configure_imgui_fonts(io);
    if (!ImGui_ImplSDL3_InitForVulkan(window_))
    {
        throw std::runtime_error(std::format("ImGui_ImplSDL3_InitForVulkan: {}", SDL_GetError()));
    }
}

ImGuiSession::~ImGuiSession()
{
    ImGui::SetCurrentContext(context_.get());
    shutdown_renderer();
    ImGui_ImplSDL3_Shutdown();
}

void ImGuiSession::initialize_renderer(ImGui_ImplVulkan_InitInfo &info) const
{
    ImGui::SetCurrentContext(context_.get());
    shutdown_renderer();
    if (!ImGui_ImplVulkan_Init(&info))
    {
        throw std::runtime_error("ImGui Vulkan backend initialization failed");
    }
}

void ImGuiSession::begin_frame() const
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();

    const float display_scale = SDL_GetWindowDisplayScale(window_);
    const float pixel_density = SDL_GetWindowPixelDensity(window_);
    // ImGui already uses the framebuffer scale when baking glyphs.
    const float font_scale = display_scale > 0.0F && pixel_density > 0.0F ? display_scale / pixel_density : 1.0F;
    ImGui::GetStyle().FontScaleDpi = font_scale;
    ImGui::NewFrame();
}
