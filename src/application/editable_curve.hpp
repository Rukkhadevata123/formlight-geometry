#pragma once

#include "geometry/curves/bspline_curve.hpp"
#include "single_step_history.hpp"

#include <expected>
#include <optional>
#include <string>

struct LineCurveParameters
{
    static constexpr int min_poles = 2;
    static constexpr int max_poles = 64;
    [[nodiscard]] static int degree_limit(int pole_count);
    int pole_count = 6;
    int degree = 3;
};

class EditableCurve
{
  public:
    using Curve = occt_lab::geometry::SampledCurve;
    using Definition = occt_lab::geometry::CurveDefinition;

    EditableCurve();
    explicit EditableCurve(Curve curve);

    [[nodiscard]] auto current() const -> const Curve &;
    [[nodiscard]] auto displayed() const -> const Curve &;
    [[nodiscard]] auto preview_pole_move(std::size_t index, const occt_lab::geometry::Vec3 &position)
        -> std::expected<void, std::string>;
    void accept_pole_move();
    void cancel_pole_move();

    // clang-format off
    [[nodiscard]] auto create_line(LineCurveParameters params)   -> std::expected<void, std::string>;
    [[nodiscard]] auto create_cubic_bezier()                     -> std::expected<void, std::string>;
    [[nodiscard]] auto apply(const Definition &definition)       -> std::expected<void, std::string>;
    [[nodiscard]] auto insert_knot(double parameter)             -> std::expected<void, std::string>;
    [[nodiscard]] auto elevate_degree(int target_degree)         -> std::expected<void, std::string>;
    [[nodiscard]] auto interpolate_through_poles()               -> std::expected<void, std::string>;
    [[nodiscard]] auto approximate_data_points(double tolerance) -> std::expected<void, std::string>;
    // clang-format on

    [[nodiscard]] bool history_changes_knots() const
    {
        const auto &other = history_.alternate();
        return other && other->definition.knots != current().definition.knots;
    }
    [[nodiscard]] bool can_undo() const
    {
        return history_.can_undo();
    }
    [[nodiscard]] bool can_redo() const
    {
        return history_.can_redo();
    }
    void undo();
    void redo();

  private:
    void commit(Curve candidate);
    void replace_with_created_curve(const Definition &definition);
    void commit_refinement(const Definition &definition);
    SingleStepHistory<Curve> history_;
    std::optional<Curve> pole_preview_;
};
