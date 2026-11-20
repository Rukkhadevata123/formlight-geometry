#include "application/editable_curve.hpp"
#include "document/curve_project.hpp"

#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <random>
#include <stdexcept>
#include <string_view>

namespace
{
namespace fs = std::filesystem;
using namespace occt_lab::document;

struct TestDirectory
{
    fs::path path = fs::temp_directory_path() / std::format("occt-lab-project-{:08x}", std::random_device{}());
    TestDirectory()
    {
        if (!fs::create_directory(path))
        {
            throw std::runtime_error("Could not create a unique project test directory");
        }
    }
    ~TestDirectory()
    {
        std::error_code ignored;
        fs::remove_all(path, ignored);
    }
};

void check_invalid_file(const fs::path &path, const std::string &original, const std::string_view part,
                        const std::string_view replacement)
{
    auto text = original;
    text.replace(text.find(part), part.size(), replacement);
    std::ofstream out(path, std::ios::binary);
    out.exceptions(std::ios::badbit | std::ios::failbit);
    out << text;
    out.close();
    if (open_curve_project(path))
    {
        throw std::runtime_error(std::format("Accepted invalid project field '{}'", part));
    }
}
} // namespace

void check_curve_projects()
{
    const TestDirectory directory;
    const auto path = directory.path / u8"曲线.occtlab";
    EditableCurve curve;
    auto definition = curve.current().definition;
    definition.weights[2] = 3.125;
    definition.poles[1].z = 0.12345678901234567;
    const CurveProjectSettings settings{.object_id = 7,
                                        .name = "A \"weighted\" curve",
                                        .pole_number = 2,
                                        .knot_parameter = 0.45,
                                        .fit_tolerance = 0.25};
    if (!curve.apply(definition) || !save_curve_project(path, curve.current(), settings))
    {
        throw std::runtime_error("Could not save the weighted test project");
    }
    const auto restored = open_curve_project(path);
    if (!restored || restored->curve.definition != definition || restored->curve.samples != curve.current().samples ||
        restored->settings != settings)
    {
        throw std::runtime_error("Weighted definition or editor settings changed on reopening");
    }
    if (!curve.interpolate_through_poles() || !save_curve_project(path, curve.current(), settings))
    {
        throw std::runtime_error("Could not save the interpolated test curve");
    }
    const auto interpolation = open_curve_project(path);
    if (!interpolation || interpolation->curve.data_points != curve.current().data_points ||
        interpolation->curve.approximation)
    {
        throw std::runtime_error("Reopening the interpolant changed its fitting data or mode");
    }
    if (!curve.approximate_data_points(1.0) || !curve.insert_knot(0.45) ||
        !save_curve_project(path, curve.current(), settings))
    {
        throw std::runtime_error("Could not replace the saved project with a refined fit");
    }
    auto bad_settings = settings;
    bad_settings.pole_number = 0;
    if (save_curve_project(path, curve.current(), bad_settings))
    {
        throw std::runtime_error("Save accepted an invalid selected pole");
    }
    auto incomplete = curve.current();
    incomplete.definition.weights.pop_back();
    if (save_curve_project(path, incomplete, settings))
    {
        throw std::runtime_error("An incomplete weighted curve replaced the existing file");
    }
    const auto fit = open_curve_project(path);
    const auto &before = curve.current();
    if (!fit || fit->curve.definition != before.definition || fit->curve.framing_extent != before.framing_extent ||
        fit->curve.refinement != before.refinement || fit->curve.approximation != before.approximation ||
        fit->curve.data_points != before.data_points)
    {
        throw std::runtime_error("Reopening the refined fit lost its source points, reference or report");
    }
    std::ifstream in(path, std::ios::binary);
    const std::string text{std::istreambuf_iterator<char>{in}, {}};
    const auto invalid = directory.path / "invalid.occtlab";
    check_invalid_file(invalid, text, "occt-lab-curve 1", "occt-lab-curve 2");
    check_invalid_file(invalid, text, "units mm", "units inch");
    check_invalid_file(invalid, text, "degree 3", "degree 0");
    check_invalid_file(invalid, text, "end\n", "");
    check_invalid_file(invalid, text, "end\n", "end\nextra");
}
