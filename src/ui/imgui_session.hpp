#pragma once

#include <imgui.h>

#include <memory>

struct SDL_Window;
struct ImGui_ImplVulkan_InitInfo;

class ImGuiSession
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
