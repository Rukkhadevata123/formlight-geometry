#pragma once

#include <expected>
#include <filesystem>
#include <functional>
#include <string>

struct ProjectFileControls
{
    using Operation = std::function<std::expected<void, std::string>(const std::filesystem::path &)>;

    std::string path;
    std::string status;

    void draw(bool pending_edits, const Operation &save, const Operation &open);
};
