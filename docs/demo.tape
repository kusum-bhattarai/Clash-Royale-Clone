# Records the gameplay GIF in the README.
#
#   vhs docs/demo.tape
#
# Needs vhs (github.com/charmbracelet/vhs) and a build in ./build.
# The timings assume the shipped elixir economy: you start on 5 and regain one
# every 2.8 seconds, so the deploys below are spaced to be affordable.

Output docs/gameplay.gif

Set Shell bash
Set FontSize 15
Set Width 560
Set Height 800
Set Padding 12
Set Framerate 12
Set PlaybackSpeed 1.0
Set Theme "Builtin Dark"

# Start the game without showing the command.
Hide
Type "clear && ./build/clash_royale"
Enter
Sleep 1200ms
Show

# Opening: the arena, the river across the middle, a bridge in each lane.
Sleep 2s

# Archers down the left lane. Cheap at 2, so affordable immediately.
Type "a"
Sleep 600ms
Type "l"
Sleep 4s

# Knight down the right lane once elixir has recovered.
Type "k"
Sleep 600ms
Type "r"
Sleep 5s

# Goblins left, to show two pushes at once.
Type "b"
Sleep 600ms
Type "l"
Sleep 6s

# Let the fight play out over the bridges.
Sleep 5s

# Quit cleanly so the final frame is the end-of-match message.
Type "q"
Sleep 1500ms
