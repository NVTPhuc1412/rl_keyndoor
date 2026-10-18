#include <array>
#include <optional>
#include <random>
#include <string>
#include <iostream>
#include <stdexcept>
#include <cassert>
#include <vector>


enum Action
{
    Up,
    Down,
    Left,
    Right,
};

struct State
{
    int x;
    int y;
    bool has_key;
};

struct Transition
{
    State next_state;
    double reward;
    bool terminated;
    std::optional<double> probability {std::nullopt};
};


int encode_state(const State &state, int width, int height)
{
    const int num_cells = width * height;

    const int position_id = state.y * width + state.x;

    const int key_offset = state.has_key ? num_cells : 0;

    return position_id + key_offset;
};


State decode_state(int state_id, int width, int height)
{
    const int num_cells = width * height;

    const bool has_key = state_id >= num_cells;

    const int position_id = state_id - has_key * num_cells;
    const int x = position_id % width;
    const int y = position_id / width;

    return {x, y, has_key};
};


constexpr int WIDTH = 8;
constexpr int HEIGHT = 8;


bool in_bounds(int x, int y)
{
    return x >= 0 && x < WIDTH &&
           y >= 0 && y < HEIGHT;
};

bool is_wall(
    const std::array<std::string, HEIGHT> &grid,
    int x, int y
)
{
    if (!in_bounds(x, y))
    {
        return true;
    }

    return grid.at(y).at(x) == '#';
};


State move(const State &state, Action action)
{
    State next_state = state;
    switch (action)
    {
        case Action::Up:
            next_state.y--;
            break;
        case Action::Down:
            next_state.y++;
            break;
        case Action::Left:
            next_state.x--;
            break;
        case Action::Right:
            next_state.x++;
            break;
    }
    return next_state;
}


State move_if_allowed(
    const std::array<std::string, HEIGHT> &grid,
    const State &state, Action action
)
{
    State next_state {move(state, action)};

    if (is_wall(grid, next_state.x, next_state.y))
    {
        return state;
    }

    const char next_cell {grid.at(next_state.y).at(next_state.x)};
    if (next_cell == 'D' && !state.has_key)
    {
        return state;
    }
    if (next_cell == 'K')
    {
        next_state.has_key = true;
    }

    return next_state;
}


Transition transition (
    const std::array<std::string, HEIGHT> &grid,
    const State &state, Action action
)
{
    State next_state {move_if_allowed(grid, state, action)};

    bool terminated {grid.at(next_state.y).at(next_state.x) == 'G'};

    return {next_state, -1.0, terminated};
}


class KeyDoorEnv
{
private:
    std::array<std::string, HEIGHT> grid_;
    State state_;
    bool terminated_;

    double slip_prob_;
    std::mt19937 rng_;

    Action sample_action(Action intended_action);

public:
    KeyDoorEnv(double slip_prob, std::uint32_t seed);
    State reset();
    Transition step(Action action);

    // Action sample_action(Action intended_action); // Uncomment for testing
    // std::array<std::string, HEIGHT> grid_;        // Uncomment for testing
};

KeyDoorEnv::KeyDoorEnv(double slip_prob, std::uint32_t seed)
    : grid_{
        "########",
        "#S..#..#",
        "#...#..#",
        "#...#..#",
        "#K..D.G#",
        "#...#..#",
        "#...#..#",
        "########"
    },
     state_{1,1,false},
     terminated_{false},
     slip_prob_{slip_prob},
     rng_{seed}
{
    if (slip_prob < 0.0 || slip_prob > 1.0)
    {
        throw std::invalid_argument("Slip probability must be between 0 and 1");
    }
}

State KeyDoorEnv::reset()
{
    terminated_ = false;
    state_ = {1,1,false};
    return state_;
}

Action KeyDoorEnv::sample_action(Action intended_action)
{
    Action actual_action = intended_action;

    std::uniform_real_distribution<double> dist{0.0, 1.0};
    std::uniform_int_distribution<int> action_dist{0, 3};

    bool slip = dist(rng_) < slip_prob_;
    if (slip)
    {
        actual_action = static_cast<Action>(action_dist(rng_));
    }

    return actual_action;
}

std::vector<Transition> transition_probs(
    const std::array<std::string, HEIGHT> &grid,
    const State &state,
    Action intended_action,
    double slip_prob
)
{
    std::vector<Transition> outcomes {};
    outcomes.reserve(4);

    for (int i = 0; i < 4; ++i)
    {
        Action action = static_cast<Action>(i);
        Transition outcome = transition(grid, state, action);

        outcome.probability =
            slip_prob / 4
            + (action == intended_action ? 1.0 - slip_prob : 0.0);

        outcomes.push_back(outcome);
    }
    return outcomes;
}

Transition KeyDoorEnv::step(Action action)
{
    if (terminated_)
    {
        throw std::runtime_error("Episode has already terminated");
    }

    Action actual_action = sample_action(action);
    Transition result = transition(grid_, state_, actual_action);

    state_ = result.next_state;
    terminated_ = result.terminated;

    return result;
}

int main()
{
    std::array<std::string, HEIGHT> grid
    {
    "########",
    "#S..#..#",
    "#...#..#",
    "#...#..#",
    "#K..D.G#",
    "#...#..#",
    "#...#..#",
    "########"
    };

    State state{1, 1, false};

    auto outcomes = transition_probs(
        grid,
        state,
        Action::Right,
        0.2
    );

    double total_probability {};

    for (int i = 0; i < 4; ++i)
    {
        total_probability = total_probability + outcomes[i].probability.value();
    };

    assert(std::abs(total_probability - 1.0) < 1e-12);


}
