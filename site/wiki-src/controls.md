---
title: Keyboard and controls
section: Reference
order: 55
summary: Every way to drive Spike: mouse and touch gestures on the bench, keyboard keys, gamepad buttons, and the screen-by-screen button list.
---

# Keyboard and controls

Spike is mouse and touch first. Every menu can also be driven by keyboard or gamepad, and the lab has shortcuts for running, checking, editing and moving around the bench.

## Keyboard

| Key | What it does |
|---|---|
| Arrow keys | Move the focus ring between controls. On a stepper (a control with a value and arrows), Left and Right change the value. |
| Enter (or Space) | Press the focused control. On a stepper, steps it forward. |
| Esc | Back. Goes to the screen you came from. Closes a menu or card first, if one is open. |
| Delete or Backspace | On the bench, deletes the selection (one or several neuron groups, a connection or an added input). |
| Tab and Shift+Tab | Move the focus to the next and previous control. |
| Up, Down, Page Up, Page Down | Scroll the Notebook, What is real and Credits screens. |

### In the lab

These work when no text field or button has the focus (a focused button takes Space and Enter itself), and not while a menu or card is open.

| Key | What it does |
|---|---|
| Space | Run or pause. Hold Space and drag the bench to pan it. |
| C | Check. |
| R | Reset. |
| Ctrl+Z | Undo. Every change to the circuit is a step: adding, wiring, deleting, a slider, a drag, a pasted or loaded circuit. Up to 100 steps. |
| Ctrl+Shift+Z or Ctrl+Y | Redo. |
| Ctrl+A | Select every neuron group. |
| Ctrl+C, Ctrl+V | Copy the selected groups (with the connections between them, and their connections to sensors and motors), paste them beside the originals. |
| Ctrl+D | Duplicate the selection. |
| +, -, F | Zoom in, zoom out, fit the circuit to the bench. |
| I | Open the inspector on the selection. |

Click the bench (or Tab onto it) to give it the focus. A bench cursor (a double ring) then walks the cards and groups:

| Key | What it does |
|---|---|
| Tab, Shift+Tab | Next and previous card or group. |
| Arrow keys | The nearest card or group in that direction. |
| Enter | Select what the cursor is on. While a wire is open, finish it there. |
| W | Start a wire from the cursor. Move to the target and press Enter; Esc cancels. |
| Shift+arrow keys | Nudge the selected group. |

Keyboard navigation turns on when you press a navigation key. It turns off again when you move the mouse. While it is on, controls show a focus ring.

## Gamepad

| Button | What it does |
|---|---|
| D-pad | Move the focus. |
| A | Press. |
| B | Back. |
| D-pad (bench focused) | Move the bench cursor between cards and groups. |
| A (bench focused) | Select, or finish an open wire. |
| X (bench focused) | Start a wire from the cursor. |
| Y (bench focused) | Open the inspector. |
| Start | Run or pause in the lab. |
| LB, RB (bench focused) | Zoom out, zoom in. |

Sticks are not used for navigation. A small hint ("D-PAD MOVE, A SELECT, B BACK") appears at the bottom when pad navigation is on. The pad layer also works on the Steam Deck, where the game picks the handheld layout.

## Mouse and touch

| Gesture | Where | What it does |
|---|---|---|
| Click or tap | Anywhere | Press a button, select a part. |
| Drag a disc from its middle | Bench | Moves the neuron group. |
| Drag from the rim of a disc | Bench | Starts a connection. Drop it on another disc (or on a motor card). |
| Drag from the right edge of a sensor card | Bench | Starts a connection from the sensor. Drop it on a disc. |
| Press **Connect**, click source, click target | Bench | Connects without dragging. Good for touch. Click empty space to cancel. |
| Right-click a part | Bench | Selects and deletes it (Ctrl+Z brings it back). |
| Shift+click a group | Bench | Adds it to the selection, or takes it out. |
| Drag a box over the empty bench | Bench | Selects every group inside the box. Drag one of them to move them all. |
| Wheel, or pinch | Bench | Zooms (15% to 300%) about the pointer. |
| Middle-drag, or drag the empty bench with a finger | Bench | Pans. |
| Click the zoom readout (bottom left) | Bench | Fits the circuit to the bench. |
| Click a bus card (several numbered inputs) | Bench | Opens it into one card per channel; click its head to close it. |
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

See [how to read the lab](how-to-read-the-lab.html) for each control. In short: Back, Run or Pause, Reset, Check, Hint, Brief or Case, Test x5, More; the palette (Parts, + Excitatory, + Inhibitory, + Noise, Connect); the inspector's Trace, E / I, Delete, neurons in group, Advanced, Reset parameters; Undo and Redo (on the play bar, or in the palette on a phone); More, with Speed, Skip, Fit, Mode, Variant, Test x5, Results, Reference, Empty bench, the solution slots (Save to A, B, C and Load A, B, C), Lesions, Analysis, Load and Save, Copy circuit as JSON, Paste circuit JSON and Import from a passed level.

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
