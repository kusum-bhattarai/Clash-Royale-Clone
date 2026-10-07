# Clash Royale Clone

![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)
![CMake](https://img.shields.io/badge/build-CMake-green.svg)
![GoogleTest](https://img.shields.io/badge/tested%20with-GoogleTest-red.svg)

A command-line, simplified clone of the popular strategy game "Clash Royale," built from the ground up in modern C++. This project demonstrates a strong understanding of object-oriented design, modern build systems, and professional software development practices, including unit testing.

---

## Key Features

* **Real-time Strategy Gameplay:** Deploy troops in real-time against an AI opponent in a terminal-based arena.
* **Object-Oriented by Design:** Utilizes a robust class hierarchy with polymorphism to manage different troops and buildings, each with unique stats and behaviors (e.g., Golems prioritizing towers, Dragons flying).
* **Modern C++ Practices:** Written in C++17, using smart pointers for memory management, and organized into a professional, scalable project structure.
* **CMake Build System:** Employs a clean, modern CMake configuration that handles dependencies and builds the project efficiently.
* **Unit Tested:** Features a comprehensive test suite built with the Google Test framework, ensuring the correctness and reliability of core game logic, combat mechanics, and AI behavior.

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
#include "clash_royale/sim/board.hpp"
#include "clash_royale/sim/entity_factory.hpp"

cr::Board board;
board.addEntity(cr::EntityFactory::create(
    cr::EntityType::KNIGHT, 10, 10, /*isPlayer=*/true, cr::Lane::LEFT));
board.updateEntities();   // advance the simulation one tick
```

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
- **Entity Creation & State:** Verification of initial stats, damage application, and health states.
- **Combat Mechanics:** Targeting rules (air vs. ground, tower-prioritization), damage modifiers (armor types, bonus damage), and attack range validation.
- **Core Game Logic:** Elixir generation and capping, AI deployment logic, and primary win/game end conditions (King Tower destruction).

### Future Improvements
- **Advanced AI:** Enhance the AI from random deployments to a more strategic system that reacts to the player's moves.
- **Data-Driven Design:** Move entity stats (health, damage, etc.) out of the code and into external data files (like JSON or XML) to be loaded at runtime.
- **Expanded Card Pool:** Add more troops and spells with unique mechanics to deepen the strategic gameplay.