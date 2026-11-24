export module occt_lab.document.project_io;

import std;
import occt_lab.geometry.vec3;

namespace occt_lab::document::io
{
export template <class T> T read_number(std::istream &in, const std::string_view field)
{
    std::string token;
    in >> token;
    const auto end = token.data() + token.size();
    T value{};
    const auto [next, error] = std::from_chars(token.data(), end, value);
    if (error != std::errc{} || next != end || !std::isfinite(value))
    {
        throw std::runtime_error(std::format("Invalid or missing {}", field));
    }
    return value;
}

export void expect_token(std::istream &in, const std::string_view expected)
{
    std::string token;
    if (!(in >> token) || token != expected)
    {
        throw std::runtime_error(std::format("Expected '{}' in the project file", expected));
    }
}

export void expect_end(std::istream &in)
{
    expect_token(in, "end");
    if (!(in >> std::ws).eof() || in.bad())
    {
        throw std::runtime_error("Unexpected data after the project");
    }
}

export geometry::Vec3 read_position(std::istream &in)
{
    return {.x = read_number<double>(in, "X"), .y = read_number<double>(in, "Y"), .z = read_number<double>(in, "Z")};
}

export std::ifstream open_input(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        throw std::runtime_error("Could not open the project file");
    }
    in.imbue(std::locale::classic());
    return in;
}

export [[nodiscard]] auto write_atomically(const std::filesystem::path &path,
                                           const std::function<void(std::ostream &)> &write)
    -> std::expected<void, std::string>
{
    std::filesystem::path temporary;
    bool owns_temporary = false;
    try
    {
        temporary = path;
        temporary += std::format(".{:08x}.tmp", std::random_device{}());
        std::ofstream out(temporary, std::ios::binary | std::ios::noreplace);
        if (!out)
        {
            return std::unexpected("Could not create a temporary file in the project directory");
        }
        owns_temporary = true;
        out.exceptions(std::ios::badbit | std::ios::failbit);
        out.imbue(std::locale::classic());
        out << std::setprecision(std::numeric_limits<double>::max_digits10);
        write(out);
        out.close();
        std::filesystem::rename(temporary, path);
        return {};
    }
    catch (const std::exception &error)
    {
        if (owns_temporary)
        {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
        }
        return std::unexpected(error.what());
    }
}
} // namespace occt_lab::document::io
