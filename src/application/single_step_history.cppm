export module occt_lab.application.single_step_history;

import std;

export template <class T> class SingleStepHistory
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
    // Allow the owner to refresh derived data without changing which edit can be undone or redone.
    [[nodiscard]] constexpr std::pair<T &, T *> stored_snapshots() noexcept
    {
        return {current_, alternate_ ? std::addressof(*alternate_) : nullptr};
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
