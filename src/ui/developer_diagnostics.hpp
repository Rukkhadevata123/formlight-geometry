#pragma once

#include <string>

// Returns true when the user requests a compute buffer check this frame.
[[nodiscard]] bool draw_developer_diagnostics(bool &open, bool compute_supported, const std::string &compute_status);
