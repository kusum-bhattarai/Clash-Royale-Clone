# Clash Royale Clone

![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)
![CMake](https://img.shields.io/badge/build-CMake-green.svg)
![GoogleTest](https://img.shields.io/badge/tested%20with-GoogleTest-red.svg)

A simplified Clash Royale written in C++20. It is two things at once: a playable
terminal game, and a library you can build your own game or bot on top of.

The simulation is headless. It does no I/O, it runs on a timestep you choose,
and a seed replays a match exactly, so you can run thousands of matches in a few
seconds to test a strategy or rebalance a card.

## Example gameplay

<video controls src="SampleGameplay.mp4" title="Gameplay" height="480" width="280"></video>

## What is in here

* **Real-time play.** Deploy troops down the left or right lane against an AI
  opponent while elixir regenerates.
* **An arena with terrain.** A river splits the board, with a bridge in each
  lane. Ground troops have to reach a crossing. Fliers go straight over.
* **Pathfinding.** Units follow routes from A\* or a shared flow field, avoid
  each other, and stop once their target is in range. Air units take the
  straight line.
* **Cards are data.** A troop is a `CardSpec` value in a registry. Stats, armor,
  targeting rules and damage bonuses are all data, so you can add your own units
  without changing the library.
* **Two opponents included.** A random one, and a strategic one that defends,
  saves elixir and commits pushes. It wins about 82% of seeded matches against
  the random one.
* **Deterministic.** Randomness comes from a seeded generator the simulation
  owns, not from global `rand()`. The same seed gives the same match on every
  platform.
* **141 tests** with GoogleTest, plus a benchmark for the pathfinding.

## Quick start

You need a C++20 compiler, CMake 3.16 or newer, and Git. GoogleTest is
downloaded automatically when you build the tests.

```bash
git clone https://github.com/kusum-bhattarai/Clash-Royale-Clone.git
cd Clash-Royale-Clone
cmake -S . -B build
cmake --build build
./build/clash_royale
```

That gives you `clash_royale` (the game), `run_tests` (the suite), and the two
example programs.

### Playing

Pick a troop, then pick a lane. The number in brackets is the elixir cost.

| Key | Troop | Key | Troop |
| --- | --- | --- | --- |
| `K` | Knight (4) | `D` | Dragon (5) |
| `G` | Golem (5) | `W` | Wizard (4) |
| `P` | Pekka (4) | `A` | Archers (2) |
| `B` | Goblins (3) | `C` | Canon (3) |

After choosing a troop, press `L` or `R` for the lane, or `X` to cancel. `Q`
quits.

Water is drawn as `~` and bridges as `=`. Your units are uppercase, the
opponent's are lowercase.

## Using it as a library

Install it, then pull it into any CMake project:

```cmake
find_package(ClashRoyale REQUIRED)
target_link_libraries(your_target PRIVATE ClashRoyale::core)
```

Or vendor it without installing:

```cmake
include(FetchContent)
FetchContent_Declare(ClashRoyale
  GIT_REPOSITORY https://github.com/kusum-bhattarai/Clash-Royale-Clone.git
  GIT_TAG main)
FetchContent_MakeAvailable(ClashRoyale)
target_link_libraries(your_target PRIVATE ClashRoyale::core)
```

Pulling the project in as a subdirectory will not build the terminal front-end,
download GoogleTest, or compile the examples. Those are on only when this
project is the top-level one.

### Running a match with no terminal

```cpp
#include "clash_royale/ai/strategic_controller.hpp"
#include "clash_royale/sim/simulation.hpp"

cr::MatchConfig config;
config.deterministic = true;   // same seed, same match, every platform
config.seed = 2024;

cr::Simulation sim{config};
cr::StrategicAiController opponent;

while (sim.isRunning()) {
    opponent.update(sim, /*isPlayerOne=*/false, cr::kDefaultTimeStep);
    sim.deploy("knight", cr::Lane::LEFT, /*isPlayerOne=*/true);
    sim.step(cr::kDefaultTimeStep);   // or any dt you like
}

const int lead = sim.towerHealth(true) - sim.towerHealth(false);
```

`deploy` checks whether the player can afford the card and spends the elixir
itself, so it is safe to call every step. It returns `false` and changes nothing
if the card is unknown, not deployable, or unaffordable.

A full 120 second match takes about two milliseconds, so running a few thousand
of them is a reasonable thing to do in a test. See
`examples/headless_match.cpp`.

### Adding your own card

Define a `CardSpec`, register it, and it is immediately deployable, targetable,
and picked up by the AI. Nothing in the library changes.

```cpp
cr::CardSpec harpy;
harpy.id          = "harpy";
harpy.displayName = "Harpy";
harpy.symbol      = 'H';
harpy.health      = 240;
harpy.damage      = 30;
harpy.attackRange = 2;
harpy.moveSpeed   = 1.6f;            // tiles per second
harpy.attackSpeed = 1.0f / 0.9f;     // 0.9s between hits
harpy.elixirCost  = 3.0f;
harpy.domain      = cr::MovementDomain::Air;
harpy.movement    = cr::MovementStyle::Diagonal;
harpy.targets     = cr::TargetFilter{/*ground=*/true, /*air=*/true};

// Double damage against heavy armor.
harpy.damageModifiers = {
    cr::DamageModifier{.multiplier = 2.0f, .againstArmor = {cr::ArmorClass::Heavy}},
};

cr::MatchConfig config;
config.cards.define(harpy);

cr::Simulation sim{config};
sim.deploy("harpy", cr::Lane::LEFT, /*isPlayerOne=*/true);
```

A `DamageModifier` can match the target's armor class, its movement domain,
specific card ids, or whether it is a building. Conditions you leave empty match
anything, and every condition you do set has to match. Write them with
designated initializers, as above, so you only name the fields you care about.

Built-in card ids are available as constants in `sim/default_cards.hpp` if you
would rather not write string literals.

If a unit needs behavior that does not fit in data, set `CardSpec::factory` to
return your own `cr::Entity` subclass. You can then override `move()`,
`findTarget()` or `update()` while still declaring the stats as data. See
`examples/custom_card.cpp`.

You can also redefine an existing card to rebalance it. Registering an id that
already exists replaces it in place.

### Writing your own opponent

Implement `cr::AiController`, which has one method:

```cpp
class MyController : public cr::AiController {
public:
    void update(cr::Simulation& sim, bool isPlayerOne, float dt) override {
        if (sim.canAfford("archers", isPlayerOne)) {
            sim.deploy("archers", cr::Lane::RIGHT, isPlayerOne);
        }
    }
};
```

A controller acts only through `Simulation`'s public interface, so it cannot see
or do anything a player could not. `RandomAiController` and
`StrategicAiController` are both written this way, and both read the roster from
the registry, so they pick up cards you add without knowing anything about them.

`StrategicAiController::Tuning` exposes its weights if you want to change how
cautiously it plays.

## How it works

Short version here. `Architecture_documentation.md` has the detail.

The build produces two libraries:

| Target | Alias | Contents |
| --- | --- | --- |
| `cr_core` | `ClashRoyale::core` | The simulation. No I/O, no platform dependencies. |
| `cr_tui` | `ClashRoyale::tui` | POSIX terminal rendering and input. Optional. |

`Simulation` owns everything about a match: the board, both elixir pools, the
clock, the win condition and the random generator. `step(dt)` advances it.
Nothing in it knows what a terminal is, which is why the whole test suite runs
with the front-end switched off.

Routing goes through `cr::Navigator`, which picks a strategy per request. Air
units get a straight line. Ground units share a flow field per goal, so
everything heading for the same tower shares one search instead of running one
each. A unit that local avoidance leaves stuck gets an A\* route that accounts
for other units.

CI checks that `cr_core` builds with the front-end off, that no core header
reaches the front-end, that GoogleTest is not downloaded when tests are off, and
that a separate project can consume the installed package and add a card to it.

### Layout

```
Clash-Royale-Clone/
├── include/clash_royale/   # Public headers, all under namespace cr
│   ├── core/               # Point, Lane, Arena and terrain, Rng
│   ├── sim/                # CardSpec, CardRegistry, Entity, Board, Simulation, combat
│   ├── path/               # Pathfinder, A*, flow fields, Navigator
│   ├── ai/                 # AiController and the two implementations
│   └── tui/                # Terminal front-end: Renderer, InputHandler, Game
├── src/                    # Implementations, mirroring the headers
├── apps/tui/               # The playable terminal game
├── examples/               # Headless match, and adding a custom card
├── benchmarks/             # Pathfinding benchmark
├── tests/                  # Unit and characterization tests
├── cmake/                  # Package-config template
└── CMakeLists.txt
```

## Build options

| Option | Default | Effect |
| --- | --- | --- |
| `CR_BUILD_TUI` | on at top level | Terminal front-end and the playable game |
| `CR_BUILD_TESTING` | on at top level | Test suite, which downloads GoogleTest |
| `CR_BUILD_EXAMPLES` | on at top level | The example programs |
| `CR_INSTALL` | on at top level | Install and package-config rules |
| `CR_BUILD_BENCHMARKS` | off | The pathfinding benchmark |

## Tests

```bash
ctest --test-dir build --output-on-failure
```

What the 141 tests cover:

- **Entities:** stats read from the card table, damage, health clamping, arena
  bounds, and movement cadence for each unit speed.
- **Combat:** targeting rules for air, ground and buildings-only, data-driven
  damage bonuses, armor scaling, attack range, and attack-speed cooldowns.
- **Card registry:** registering custom cards, rebalancing existing ones,
  reference stability as more cards arrive, the `factory` escape hatch, and
  refusing unknown or non-deployable ids.
- **Arena and routing:** terrain layout, passability per movement domain, that
  no fixed position sits in the river, that ground units never stand on water,
  that air units fly over it, and that units cross at the nearest bridge.
- **Pathfinding:** that A\* returns a genuinely shortest route, that neither
  search cuts a diagonal corner between two blocked tiles, that a walled-off
  goal is reported rather than looped on, that following a flow field always
  reduces cost, and that A\* and the flow field agree on route cost.
- **Collision:** no two ground units share a tile, air and ground do not block
  each other, buildings obstruct ground movement, and units find a gap in a wall
  of other units.
- **Match rules:** the elixir economy across different timesteps, deploy
  affordability, and every win condition, including King Tower destruction,
  tower health at full time, and draws.
- **The strategic AI:** that it answers a flier with something that can hit air,
  never defends with a card that ignores troops, holds elixir when there is
  nothing to answer, leaves a weak threat to the towers, and reasons correctly
  about a roster made entirely of cards it has never seen.
- **Determinism:** a seeded match replays to an identical final board, and two
  simulations running in one process do not affect each other.

A few tests are named with a `Bug_` prefix. Those pin behavior that is wrong, so
that fixing it shows up as a visible change to the test rather than quietly
disappearing.

## Benchmarks

```bash
cmake -S . -B build-bench -DCR_BUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-bench
./build-bench/path_benchmark
```

This compares per-unit A\* against shared flow fields, and the result is less
one-sided than you might expect. A flow field settles far more tiles than a
single A\* search expands, so one field costs more than one search. It pays off
when several units share it, or when it survives more than one frame:

| | A\* per unit | shared fields |
| --- | --- | --- |
| 40 units on the shipped 40x35 arena | ~0.6 ms | ~0.02 ms |
| 200 units on a 160x140 arena | ~128 ms | ~0.4 ms |

Timings are from one machine and will differ on yours, so run it rather than
trusting the table. The shape of the result is the point: on the arena the game
ships with, per-unit A\* is already fine against a 100 ms frame, and the flow
field is what makes a bigger arena or a bigger army viable.

## Known gaps and ideas

- **Matches are decided on tower health rather than by a King Tower falling.**
  Queen towers do get destroyed, around 0.6 of one per match on average against
  the random opponent, but a match only gives each side about 48 elixir across
  its 120 seconds, which is not enough to break 4000 HP behind two 1500 HP
  towers. Tower health, match length and elixir rate are the levers if you want
  shorter, more decisive games.
- **Card files.** Now that cards are data, loading a roster from JSON or TOML is
  a small addition.
- **More cards and spells.** The registry takes them without library changes,
  though area-effect damage would need new combat support.
- **`Canon` is spelled with one `n`.** It is a card id now rather than an enum
  value, so renaming it is a one-line change.
