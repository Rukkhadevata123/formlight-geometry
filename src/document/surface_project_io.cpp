#include "surface_project_io.hpp"

#include "project_io.hpp"

namespace occt_lab::document::io
{
geometry::PatchSampling read_sampling(std::istream &in)
{
    expect_token(in, "isolines");
    return {.constant_u_lines = read_number<int>(in, "constant u lines"),
            .constant_v_lines = read_number<int>(in, "constant v lines"),
            .samples_per_line = read_number<int>(in, "points per line")};
}

void write_sampling(std::ostream &out, const geometry::PatchSampling sampling)
{
    out << "isolines " << sampling.constant_u_lines << ' ' << sampling.constant_v_lines << ' '
        << sampling.samples_per_line << '\n';
}

BezierPatchData read_bezier_patch(std::istream &in)
{
    if (read_number<int>(in, "format version") != 1)
    {
        throw std::runtime_error("Unsupported Bezier patch project version");
    }
    expect_token(in, "units");
    expect_token(in, "mm");
    BezierPatchData patch{.sampling = read_sampling(in)};
    expect_token(in, "poles");
    expect_token(in, "4");
    expect_token(in, "4");
    for (auto &pole : patch.poles)
    {
        pole = read_position(in);
    }
    expect_end(in);
    return patch;
}
} // namespace occt_lab::document::io
