#include "patch_project.hpp"

#include "project_io.hpp"
#include "surface_project_io.hpp"

namespace occt_lab::document
{
auto save_patch_project(const std::filesystem::path &path, const EditablePatch &model)
    -> std::expected<void, std::string>
{
    const auto write = [&](std::ostream &out)
    {
        out << "occt-lab-bezier-patch 1\nunits mm\n";
        io::write_sampling(out, model.sampling());
        out << "poles 4 4\n";
        for (const auto &[x, y, z] : model.current().control_net())
        {
            out << x << ' ' << y << ' ' << z << '\n';
        }
        out << "end\n";
    };
    return io::write_atomically(path, write);
}

auto open_patch_project(const std::filesystem::path &path) -> std::expected<EditablePatch, std::string>
{
    try
    {
        auto in = io::open_input(path);
        io::expect_token(in, "occt-lab-bezier-patch");
        const auto patch = io::read_bezier_patch(in);
        return EditablePatch{patch.poles, patch.sampling};
    }
    catch (const std::exception &error)
    {
        return std::unexpected(error.what());
    }
}
} // namespace occt_lab::document
