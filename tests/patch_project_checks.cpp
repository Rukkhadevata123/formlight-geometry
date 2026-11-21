#include "document/patch_project.hpp"
#include "project_test_directory.hpp"

#include <fstream>
#include <iterator>
#include <string_view>

namespace
{
using namespace occt_lab::document;

void check_rejected(const std::filesystem::path &path, std::string text, const std::string_view field,
                    const std::string_view replacement)
{
    text.replace(text.find(field), field.size(), replacement);
    std::ofstream out(path, std::ios::binary);
    out.exceptions(std::ios::badbit | std::ios::failbit);
    out << text;
    out.close();
    if (open_patch_project(path))
    {
        throw std::runtime_error(std::format("Accepted invalid patch project field '{}'", field));
    }
}
} // namespace

void check_patch_projects()
{
    const ProjectTestDirectory directory;
    const auto path = directory.path / u8"曲面.occtlab";
    EditablePatch model;
    save_patch_project(path, model).value();

    auto poles = model.current().control_net();
    poles[6] = {.x = 11.123456789012345, .y = -19.5, .z = 32.25};
    poles[9].z = -0.23456789012345678;
    if (!model.apply(poles).value())
    {
        throw std::runtime_error("Expected the asymmetric pole edit to change the patch");
    }
    const EditablePatch::Sampling sampling{.constant_u_lines = 5, .constant_v_lines = 7, .samples_per_line = 17};
    model.resample(sampling).value();
    const auto expected = model.current().evaluate(0.25, 0.75).value();
    if (!model.preview_pole_move(6, {.x = 12, .y = -9, .z = 13}).value())
    {
        throw std::runtime_error("Expected a distinct drag preview while saving");
    }
    save_patch_project(path, model).value();

    auto restored = open_patch_project(path).value();
    if (restored.current().control_net() != poles || restored.sampling() != sampling)
    {
        throw std::runtime_error("Opening a patch changed its committed poles or isoline settings");
    }
    const auto sample = restored.current().evaluate(0.25, 0.75).value();
    const auto reference = restored.reference().evaluate(0.25, 0.75).value();
    const auto difference = compare_surface_samples(sample, reference);
    model.discard_preview();
    if (sample.position != expected.position || sample.normal != expected.normal)
    {
        throw std::runtime_error("Position or normal changed across the patch round trip");
    }
    if (difference.position_distance_mm > 1e-10 || difference.normal_angle_degrees > 1e-9)
    {
        throw std::runtime_error("The reopened patch disagrees with its reconstructed OCCT surface");
    }
    if (restored.isolines().constant_u != model.isolines().constant_u ||
        restored.isolines().constant_v != model.isolines().constant_v)
    {
        throw std::runtime_error("Rebuilt isolines differ from the saved patch");
    }
    if (restored.can_undo() || restored.can_redo())
    {
        throw std::runtime_error("A reopened patch should start a fresh edit history");
    }
    if (!restored.create_plane())
    {
        throw std::runtime_error("Could not edit the reopened patch");
    }
    restored.undo();
    if (restored.current().control_net() != poles)
    {
        throw std::runtime_error("Undo after opening should recover the saved patch");
    }

    std::ifstream in(path, std::ios::binary);
    const std::string text{std::istreambuf_iterator<char>{in}, {}};
    const auto invalid = directory.path / "invalid.occtlab";
    check_rejected(invalid, text, "occt-lab-bezier-patch 1", "occt-lab-curve 1");
    check_rejected(invalid, text, "occt-lab-bezier-patch 1", "occt-lab-bezier-patch 2");
    check_rejected(invalid, text, "units mm", "units m");
    check_rejected(invalid, text, "isolines 5 7 17", "isolines 0 7 17");
    check_rejected(invalid, text, "poles 4 4", "poles 3 4");
    check_rejected(invalid, text, "-45 -36 0", "nan -36 0");
    check_rejected(invalid, text, "-45 -36 0", "1e100 -36 0");
    check_rejected(invalid, text, "-45 -36 0", "");
    check_rejected(invalid, text, "end\n", "end\nextra");
}
