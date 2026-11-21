#pragma once

#include <filesystem>
#include <format>
#include <random>
#include <stdexcept>

struct ProjectTestDirectory
{
    std::filesystem::path path =
        std::filesystem::temp_directory_path() / std::format("occt-lab-project-{:08x}", std::random_device{}());
    ProjectTestDirectory()
    {
        if (!std::filesystem::create_directory(path))
        {
            throw std::runtime_error("Could not create a unique project test directory");
        }
    }
    ProjectTestDirectory(const ProjectTestDirectory &) = delete;
    ProjectTestDirectory &operator=(const ProjectTestDirectory &) = delete;
    ~ProjectTestDirectory()
    {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
};
