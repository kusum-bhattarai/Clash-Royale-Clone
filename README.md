# Clash Royale Clone

![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)
![CMake](https://img.shields.io/badge/build-CMake-green.svg)
![GoogleTest](https://img.shields.io/badge/tested%20with-GoogleTest-red.svg)

A command-line, simplified clone of the popular strategy game "Clash Royale," built from the ground up in modern C++. This project demonstrates a strong understanding of object-oriented design, modern build systems, and professional software development practices, including unit testing.

---

## Key Features

* **Real-time Strategy Gameplay:** Deploy troops in real-time against an AI opponent in a terminal-based arena.
* **Data-Driven Cards:** Troops and buildings are `CardSpec` values in a registry -- stats, armor, targeting rules and damage modifiers are all data, so you can add your own units without touching the library.
* **Headless & Deterministic:** The simulation performs no I/O and runs on an explicit timestep, so it can be driven far faster than real time. A seed replays a match exactly, on any platform.
* **Modern C++ Practices:** Written in C++20, using smart pointers for memory management, and organized into a professional, scalable project structure.
* **CMake Build System:** Employs a clean, modern CMake configuration that handles dependencies and builds the project efficiently.
* **Unit Tested:** 83 tests built with GoogleTest, covering entity behavior, combat arithmetic, the card registry, win conditions and replay determinism.

## Example gameplay
<video controls src="SampleGameplay.mp4" title="Gameplay" height="480" width="280"></video>

## Project Architecture

This project is organized into a clean, library-first architecture that separates concerns and promotes modularity.
```
Clash-Royale-Clone/
├── include/clash_royale/   # Public headers, all under namespace `cr`
│   ├── core/               # Shared vocabulary (Lane, EntityType, arena size)
│   ├── sim/                # Simulation: entities, board, factory
│   └── tui/                # Terminal front-end: renderer, input, game loop
├── src/                    # Implementations, mirroring the above
├── apps/tui/               # The playable terminal game
├── tests/                  # Unit and characterization tests
├── cmake/                  # Package-config template
└── CMakeLists.txt
```

The build produces two libraries:

| Target | Alias | Contents |
| --- | --- | --- |
| `cr_core` | `ClashRoyale::core` | The simulation. No I/O, no platform dependencies, builds anywhere. |
| `cr_tui` | `ClashRoyale::tui` | POSIX terminal rendering and input. Optional. |

The split is enforced in CI: `cr_core` must build with the front-end disabled,
and no core header may reach terminal I/O.

For more details, checkout [Documentation](Architecture_documentation.md).

## How to Build and Run

This project uses CMake to handle the build process. Google Test is automatically downloaded as a dependency.

### Prerequisites
* A C++20 compatible compiler (g++, Clang, etc.)
* CMake (version 3.16 or higher)
* Git

### Build Options

| Option | Default | Effect |
| --- | --- | --- |
| `CR_BUILD_TUI` | on at top level | Build the terminal front-end and the playable game |
| `CR_BUILD_TESTING` | on at top level | Build the tests (downloads GoogleTest) |
| `CR_INSTALL` | on at top level | Generate install and package-config rules |

All three default to off when the project is consumed via `add_subdirectory` or
`FetchContent`, so embedding the library does not pull GoogleTest into your
build or compile terminal code you cannot run.

### Build Instructions

1.  **Clone the repository:**
    ```bash
    git clone [https://github.com/your-username/Clash-Royale-Clone.git](https://github.com/your-username/Clash-Royale-Clone.git)
    cd Clash-Royale-Clone
    ```

2.  **Create a build directory:**
    ```bash
    mkdir build
    cd build
    ```

3.  **Configure the project with CMake:**
    ```bash
    cmake ..
    ```

4.  **Compile the project:**
    ```bash
    cmake --build .
    ```
    This will create two executables inside the `build` directory: `clash_royale` (the game) and `run_tests` (the test suite).

### Running the Game
To play the game, run the `clash_royale` executable from the `build` directory:
```bash
./clash_royale
```

## Using It as a Library

Install it, then consume the package from any CMake project:

```cmake
find_package(ClashRoyale REQUIRED)
target_link_libraries(your_target PRIVATE ClashRoyale::core)
```

```cpp
#include "clash_royale/ai/random_controller.hpp"
#include "clash_royale/sim/simulation.hpp"

cr::MatchConfig config;
config.deterministic = true;   // same seed replays the same match, everywhere
config.seed = 2024;

cr::Simulation sim{config};
cr::RandomAiController ai;

while (sim.isRunning()) {
    ai.update(sim, /*isPlayerOne=*/false, cr::kDefaultTimeStep);
    sim.deploy(cr::EntityType::KNIGHT, cr::Lane::LEFT, /*isPlayerOne=*/true);
    sim.step(cr::kDefaultTimeStep);   // or any dt you like
}
```

`Simulation` is headless and performs no I/O, so it runs far faster than real
time -- useful for bots, balance sweeps and training. Plug in your own opponent
by implementing `cr::AiController`.

### Adding Your Own Card

Units are data. Define a `CardSpec`, register it, and it is immediately
deployable, targetable, and picked up by the AI -- no library changes:

```cpp
cr::CardSpec harpy;
harpy.id          = "harpy";
harpy.displayName = "Harpy";
harpy.symbol      = 'H';
harpy.health      = 240;
harpy.damage      = 30;
harpy.attackRange = 2;
harpy.moveSpeed   = 1.6f;            // tiles per second
harpy.attackSpeed = 1.0f / 0.9f;     // 0.9s hit speed
harpy.elixirCost  = 3.0f;
harpy.domain      = cr::MovementDomain::Air;
harpy.movement    = cr::MovementStyle::Diagonal;
harpy.targets     = cr::TargetFilter{/*ground=*/true, /*air=*/true};

// Double damage against heavy armor.
harpy.damageModifiers = {cr::DamageModifier{2.0f, {cr::ArmorClass::Heavy}}};

cr::MatchConfig config;
config.cards.define(harpy);

cr::Simulation sim{config};
sim.deploy("harpy", cr::Lane::LEFT, /*isPlayerOne=*/true);
```

Conditions on a `DamageModifier` can match the target's armor class, movement
domain, specific card ids, or whether it is a building; an empty condition
matches anything. For behavior that cannot be expressed as data, set
`CardSpec::factory` to return your own `cr::Entity` subclass and override
`move()`, `findTarget()` or `update()` while still declaring stats as data.

Both paths are covered by the test suite, and CI builds a separate project
against the installed package to check that a card can still be added from
outside the library.

Or vendor it directly:

```cmake
include(FetchContent)
FetchContent_Declare(ClashRoyale
  GIT_REPOSITORY https://github.com/kusum-bhattarai/Clash-Royale-Clone.git
  GIT_TAG main)
FetchContent_MakeAvailable(ClashRoyale)
target_link_libraries(your_target PRIVATE ClashRoyale::core)
```

### Running the Tests
To run the unit test suite, use the ctest command from the build directory. Use the --verbose flag for detailed output.
```bash
ctest --verbose
```

### Test Coverage
- **Entity Creation & State:** Initial stats from the card table, damage application, health clamping and arena bounds.
- **Combat Mechanics:** Targeting rules (air vs. ground, buildings-only), data-driven damage modifiers and armor scaling, attack range, and attack-speed cooldowns.
- **Card Registry:** Registering custom cards, rebalancing existing ones, spec-reference stability, the `factory` escape hatch, and rejection of unknown or non-deployable ids.
- **Core Game Logic:** Elixir economy across varying timesteps, deployment affordability, AI bounds, and all win conditions (King Tower, tower health at full time, draws).
- **Determinism:** A seeded match replays to an identical final board, and concurrent simulations do not perturb each other.

### Future Improvements
- **Arena Terrain:** Add a river and bridges so the arena has structure, and make `Lane` govern which crossing a ground unit uses.
- **Pathfinding:** Replace straight-line chasing with A* and a shared flow field over that terrain, behind a pluggable `Pathfinder` interface.
- **Advanced AI:** Build a strategic controller on top of `cr::AiController` -- threat evaluation, elixir tracking and counter-picking -- to replace random deployment.
- **Card Files:** Load `CardSpec` rosters from JSON or TOML, now that cards are data rather than code.