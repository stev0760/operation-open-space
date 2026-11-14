# Flying a recorded session of the original

Some oracle runs need real flying skill that no script has. `fly.sh` starts the original on your
desktop where you play it with your own keyboard, and records everything the way the scripted runs do.

## The command

    oracle/scenarios/fly.sh <run-id> [SHIP=<name>] [INI_SET="Key=Value ..."] [WATCH_FILE=<path>]

for example `oracle/scenarios/fly.sh patrol-1 SHIP=WARBIRD`. The default `DISPLAY_MODE=nested` shows the
game in a 1024x768 window of its own (a rootful Xwayland, so the recording never sees the rest of your
desktop). `DISPLAY_MODE=desktop` runs the game on the desktop's X display and records its window;
`DISPLAY_MODE=headless` shows nothing and is for tests.

Everything lands in `work/oracle/<run-id>/`.

## What you will see

1. A window titled Xwayland or similar opens, black. The script launches the game, clicks through the
   title, Restore Game, New Game and the Navigation window into wave 1 and answers the Command Mode box
   with No, as the scripted runs do (about 45 s). Keep your hands off the mouse until the terminal
   prints `fly: YOUR TURN`. (`AUTOSTART=0` leaves the menus to you.)
2. You play wave 1 of the registered game with the ship in `SHIP=`. Click the window once if the keys do
   not reach it. Arrow keys fly, Space fires the gun, Z the selected weapon, Tab selects, F1 is help.
3. When the game ends (see below) the script stops the recording and the watchers by itself and prints
   `fly: done`. Closing the terminal or Ctrl-C also stops them cleanly.

## First run: fight the Patrolman or Constable

1. Start the session. In wave 1 either commit a crime in front of an officer or wait: a Patrol Class
   Enforcer (Patrolman, Constable) enters from time to time. It shows in the radar and in the message
   lines. Destroying a non-virused icon, or firing on the officer, counts as a crime.
2. Fire on the officer. Watch the message lines: write down whether a Terminator comes in.
3. Kill the officer. Fly into its resource pack to collect it.
4. Open the weapon list (Tab) and check whether HANDCUFFS is there.
5. Quit with the game's own exit: Escape, then Exit (back to the intro), then Alt-X and Exit. Do not
   close the window or kill the process: a clean exit leaves the .INI and the save files in their
   final state. If you are arrested, play on through the Hall of Justice screens to the end: they are
   what the recording is for.

Say in the chat which of these happened and roughly when (the clock on the terminal lines helps).

## Is the recording good?

At the end `ls -la work/oracle/<run-id>/` shows:

- `vid-fly.mkv` of tens of MB per minute (lossless, 60 fps, timestamps are the wall clock; ffprobe
  shows about 60 frames per second and no gap over 0.034 s), and `vid-fly.start`.
- `trace.log` of many MB, `watch-*.txt` (about 35 of them, `watch-fuel.txt` etc., each with `init` and
  `chg` lines), `steps.txt` and `timeline.txt`.
- `INSPACE.INI`, `files.txt`, `files/` with the saves and .ded the run wrote, and the first menu pictures.

Quick checks: `tail work/oracle/<run-id>/steps.txt` ends with `game process gone`, `rec fly off`,
`done`; `grep -c . work/oracle/<run-id>/timeline.txt` is in the thousands; the text files have
`<redacted>` where the pilot's name stood (the video, the pictures and the save files still show it:
they stay in `work/`). To look at the video: `mpv work/oracle/<run-id>/vid-fly.mkv`.

## Extra watchers

`WATCH_FILE=<path>` is a text file of `NAME:DGROUP-OFFSET:LENGTH:MS` lines (hex offsets, at most 64
bytes), watched in INSPACE.EXE's data segment and written to `watch-NAME.txt`.

## What was and was not tested

The whole chain (launch, menus, watchers, 60 fps video with wall-clock timestamps, the trace, the
timeline, the redaction, the clean stop and the exit detection) was proven end to end on a headless
Xvfb with scripted keys standing in for you (`DISPLAY_MODE=headless SCRIPT=<bash file>`). The nested
Xwayland window and `DISPLAY_MODE=desktop` were not opened, because nothing may open on the desktop
while you are away. If the nested window does not appear or stays black, stop the session
(Ctrl-C) and use `DISPLAY_MODE=desktop`; the xserver.log in the run directory says why.
