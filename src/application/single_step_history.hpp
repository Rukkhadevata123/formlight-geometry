#pragma once

#include <optional>
#include <type_traits>
#include <utility>

template <class T> class SingleStepHistory
{
    // After replacing the current snapshot, recording the old one must not fail.
    static_assert(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_assignable_v<T> &&
                  std::is_nothrow_swappable_v<T>);

  public:
    constexpr explicit SingleStepHistory(T initial) : current_(std::move(initial))
    {
    }

    [[nodiscard]] constexpr const T &current() const
    {
        return current_;
    }
    [[nodiscard]] constexpr const std::optional<T> &alternate() const
    {
        return alternate_;
    }
    [[nodiscard]] constexpr bool can_undo() const
    {
        return alternate_.has_value() && !undone_;
    }
    [[nodiscard]] constexpr bool can_redo() const
    {
        return alternate_.has_value() && undone_;
    }

    constexpr void commit(T &&next) noexcept
    {
        alternate_ = std::exchange(current_, std::move(next));
        undone_ = false;
    }

    constexpr void undo() noexcept
    {
        if (can_undo())
        {
            using std::swap;
            swap(current_, *alternate_);
            undone_ = true;
        }
    }

    constexpr void redo() noexcept
    {
        if (can_redo())
        {
            using std::swap;
            swap(current_, *alternate_);
            undone_ = false;
        }
    }

  private:
    T current_;
    std::optional<T> alternate_;
    bool undone_ = false;
};
