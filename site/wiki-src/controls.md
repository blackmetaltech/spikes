---
title: Keyboard and controls
section: Reference
order: 55
summary: Every way to drive Spike: mouse and touch gestures on the bench, keyboard keys, gamepad buttons, and the screen-by-screen button list.
---

# Keyboard and controls

Spike is mouse and touch first. Every menu can also be driven by keyboard or gamepad. There are very few single-key shortcuts. The game does not bind keys to Run, Reset or Check, so use the buttons.

## Keyboard

| Key | What it does |
|---|---|
| Arrow keys | Move the focus ring between controls. On a stepper (a control with a value and arrows), Left and Right change the value. |
| Enter (or Space) | Press the focused control. On a stepper, steps it forward. |
| Esc | Back. Goes to the screen you came from. Closes a menu or card first, if one is open. |
| Delete or Backspace | On the bench, deletes the selected neuron group, connection or added input. |
| Tab and Shift+Tab | Move the focus to the next and previous control. |
| Up, Down, Page Up, Page Down | Scroll the Notebook, What is real and Credits screens. |

Keyboard navigation turns on when you press a navigation key. It turns off again when you move the mouse. While it is on, controls show a focus ring.

## Gamepad

| Button | What it does |
|---|---|
| D-pad | Move the focus. |
| A | Press. |
| B | Back. |

Sticks are not used for navigation. A small hint ("D-PAD MOVE, A SELECT, B BACK") appears at the bottom when pad navigation is on. The pad layer also works on the Steam Deck, where the game picks the handheld layout.

## Mouse and touch

| Gesture | Where | What it does |
|---|---|---|
| Click or tap | Anywhere | Press a button, select a part. |
| Drag a disc from its middle | Bench | Moves the neuron group. |
| Drag from the rim of a disc | Bench | Starts a connection. Drop it on another disc (or on a motor card). |
| Drag from the right edge of a sensor card | Bench | Starts a connection from the sensor. Drop it on a disc. |
| Press **Connect**, click source, click target | Bench | Connects without dragging. Good for touch. Click empty space to cancel. |
| Right-click a part | Bench | Selects and deletes it. |
| Click a connection | Bench | Selects it. The inspector shows its weight, delay, type and spread. |
| Click the number beside a slider | Inspector | Type a value. Enter or leaving the field applies it. It is clamped to the slider's range. |
| Click the window label (top right of a strip) | Raster and trace strips | Cycles the time window. |
| Drag | Lists and the inspector on touch screens | Scrolls. |
| Click a pin | Campaign map | Selects a level and shows its card. |

In lesion mode, a click cuts or silences instead of selecting, and dragging is off. In a diagnosis case the bench is read-only: you cannot add, move or delete parts.

## Buttons by screen

### Title

Play, Laboratory, What is real, Settings, Credits and Quit (Quit is hidden on phones and the web).

### Campaign map

| Control | What it does |
|---|---|
| Back | Returns to the title. |
| Mode stepper | Guided, Standard or Researcher. |
| Notebook | Opens the notebook. |
| Parts bin | Browse every cell model. |
| Sandbox | Opens the free bench. |
| Chapter tabs | Switch chapters. Body chapters are on one row, the Bestiary and Theory wings on others. |
| Pins | Select a level. |
| Build | Opens the lab on the selected level. Reads Locked if it is not open. |
| Reference | Opens the level with the reference circuit already wired. Appears after you pass, or in Guided mode. |

### The lab

See [how to read the lab](how-to-read-the-lab.html) for each control. In short: Back, Run or Pause, Reset, Check, Hint, Brief or Case, Test x5, More; the palette (Parts, + Excitatory, + Inhibitory, + Noise, Connect); the inspector's Trace, E / I, Delete, neurons in group, Advanced, Reset parameters; More, with Speed, Skip, Fit, Mode, Variant, Test x5, Results, Reference, Empty bench, Lesions, Analysis, Load and Save.

### Lesion lab

Run lesion, Sweep all singles, Clear set, Exit; Copy Markdown, Copy CSV, Save files; Cancel while a run is going.

### Diagnosis case

LESION LAB, the candidate list, SUBMIT DIAGNOSIS, REVEAL ANSWER (after three wrong guesses), Back to the case.

### Analysis

A tab stepper (3-node, 4-node, HRV, PRC, bursts, energy, STA), a graph stepper (populations or neurons), an edges stepper (all, excitatory, inhibitory), a nulls stepper (20, 100, 500), a population stepper, a record-length stepper (10 s, 30 s, 60 s, full), and RECORD.

### Notebook

Notes and Concepts tabs, What is real, Back.

### Settings

Display, Audio, Controls (a read-only list of the keys above) and Game tabs. See [settings and performance](settings-and-performance.html).

## Phone and handheld

On a phone-sized window the lab uses three tabs, Circuit, World and Scopes. The More button reads Menu and also holds Brief, Hint and the parts button. The inspector is a sheet at the bottom. Tap targets are enlarged. Force a layout in Settings, Display, Layout.

## Related pages

- [Settings and performance](settings-and-performance.html)
- [How to read the lab](how-to-read-the-lab.html)
