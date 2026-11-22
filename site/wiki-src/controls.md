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
| Esc | Back. Goes to the screen you came from. In the lab it first closes whatever is open (a card, the More menu, the parts bin, the import list), then cancels a wire you are drawing, and only then leaves the lab. In Plate 0 it skips the tutorial. |
| Delete or Backspace | On the bench, deletes the selection (one or several neuron groups, a connection or an added input; a level's own sensor and motor cards stay). Ctrl+Z brings it back. |
| Tab and Shift+Tab | Move the focus to the next and previous control. |
| Up, Down, Page Up, Page Down | Scroll the Notebook, What is real and Credits screens. |

### In the lab

These work when no text field or button has the focus (a focused button takes Space and Enter itself), and not while a menu or card is open.

| Key | What it does |
|---|---|
| Space | Run or pause (it acts when you let go). Hold Space and drag the bench to pan it; a pan does not run or pause. |
| C | Check (only where the Check button is shown: not in the sandbox or on a diagnosis case). |
| R | Reset. |
| Ctrl+Z | Undo. Every change to the circuit is a step: adding, wiring, deleting, a slider, a drag, a pasted or loaded circuit. Up to 100 steps. |
| Ctrl+Shift+Z or Ctrl+Y | Redo. |
| Ctrl+A | Select every neuron group. |
| Ctrl+C, Ctrl+V | Copy the selected groups with the connections between them, then paste them centred on the pointer (or in the middle of the bench if the pointer is off it). The pasted groups are selected. Connections to sensor and motor cards are not copied; Import from a passed level brings those. A paste is one Undo step. |
| Ctrl+D | Duplicate the selection, offset down and to the right. |
| +, -, F | Zoom in, zoom out (also = and the keypad plus and minus), fit the circuit to the bench. |
| I | Open the inspector on the selection and move the focus into it. |

Click the bench (or Tab onto it) to give it the focus. A bench cursor (a double ring) then walks the cards, groups and connections, in reading order: sensor cards, groups, motor cards, then connections:

| Key | What it does |
|---|---|
| Tab, Shift+Tab | Next and previous card or group. |
| Arrow keys | The nearest card or group in that direction. |
| Enter | Select what the cursor is on (on a bus card it opens or closes the card). While a wire is open, finish it there. In the lesion lab it cuts or silences what the cursor is on. |
| W | Start a wire from the cursor (a neuron group or a sensor card). Move the cursor to the target (a group, or a motor card) and press Enter; Esc cancels. |
| Shift+arrow keys | Nudge the selected group or groups. |

Keyboard navigation turns on when you press a navigation key (an arrow, Tab, Enter or Esc). It turns off again when you move the mouse or click. While it is on, controls show a focus ring and a small hint at the bottom of the screen reads "ARROWS MOVE, ENTER SELECT, ESC BACK".

### Building without a mouse

1. Tab to **+ Excitatory** (or **+ Inhibitory**) and press Enter. The new group appears on the bench.
2. Tab onto the bench, then press Tab or an arrow key to place the cursor on a card or group.
3. Move the cursor to a sensor card or a group and press **W**. A note says "Wire from ...".
4. Move the cursor to the target (a group, or a motor card) and press **Enter**. The connection is made.
5. Press **I** to open the inspector on what is selected, and use the arrows and Enter there to change a weight or a model.

The same works on a gamepad with the D-pad, **A**, **X** and **Y**.

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

Sticks are not used for navigation. A small hint ("D-PAD MOVE, A SELECT, B BACK") appears at the bottom when pad navigation is on. B also cancels a wire in progress and skips Plate 0. The pad layer also works on the Steam Deck, where the game picks the handheld layout.

## Mouse and touch

| Gesture | Where | What it does |
|---|---|---|
| Click or tap | Anywhere | Press a button, select a part. |
| Drag a disc from its middle | Bench | Moves the neuron group. |
| Drag from the rim of a disc | Bench | Starts a connection. Drop it on another disc (or on a motor card). |
| Drag from the right edge of a sensor card | Bench | Starts a connection from the sensor. Drop it on a disc. |
| Press **Connect**, click source, click target | Bench | Connects without dragging. Good for touch. Click empty space to cancel. |
| Right-click a part | Bench | Selects and deletes it (Ctrl+Z brings it back). Not in the lesion lab or on a diagnosis case. |
| Shift+click a group | Bench | Adds it to the selection, or takes it out. |
| Drag a box over the empty bench | Bench | Selects every group inside the box. Drag one of them to move them all. |
| Wheel, or pinch | Bench | Zooms (15% to 300%) about the pointer. |
| Middle-drag, Space-drag, or drag the empty bench with a finger | Bench | Pans. The sideways wheel pans too. |
| Two-finger pinch and move | Bench | Zooms and pans together. |
| Click the zoom readout (bottom left) | Bench | Fits the circuit to the bench. |
| Click a bus card (a run of numbered inputs) | Bench | Opens it into one card per channel; click its head tab to close it. Dragging from its right edge wires every channel at once. |
| Click a connection | Bench | Selects it. The inspector shows its weight, delay, type and spread. |
| Click the number beside a slider | Inspector | Type a value. Enter or leaving the field applies it. It is clamped to the slider's range. |
| Click the window label (top right of a strip) | Raster and trace strips | Cycles the time window. |
| Drag | Lists and the inspector on touch screens | Scrolls. |
| Click a pin | Campaign map | Selects a level and shows its card. |

In lesion mode, a click cuts or silences instead of selecting, and dragging is off. In a diagnosis case the bench is read-only: you cannot add, move or delete parts.

## Buttons by screen

### Title

Play, Laboratory, What is real, Settings, Credits and Quit (Quit is hidden on phones and the web). On a fresh save Play starts the tutorial instead of the map.

### Campaign map

| Control | What it does |
|---|---|
| Back | Returns to the title. |
| Mode stepper | Guided, Standard or Researcher. |
| Notebook | Opens the notebook. |
| Parts bin | Browse every cell model. |
| Sandbox | Opens the free bench. |
| Tutorial | Replays Plate 0, the interactive first-neuron tutorial. |
| Wing selector | Body, Bestiary or Theory. The row of chapter tabs below shows the chapters of that wing (a locked chapter is marked "(locked)"). On a phone the chapters are a strip that scrolls sideways. |
| Continue | At the right of the wing selector, "Continue: <level>". Opens the first open level you have not passed, in campaign order. Hidden when there is none. |
| Pins | Select a level and show its card. |
| Build | Opens the lab on the selected level. Reads Locked if it is not open. |
| Reference | Opens the level with the reference circuit already wired. Appears after you pass, or in Guided mode. It earns no stars. |

### The lab

See [how to read the lab](how-to-read-the-lab.html) for each control. In short: Back, Run or Pause, Reset, Check, Hint, Brief or Case, Test x5, Undo and Redo, More; the palette (Parts, + Excitatory, + Inhibitory, Connect, and + Noise in the sandbox); the inspector's Trace, E / I, Delete, neurons in group, Advanced, Reset parameters. More holds Speed, Skip, Fit, Mode, Variant, Test x5, Results, Reference, Empty bench, Restore previous bench, the solution slots (Save to A, B, C and Load A, B, C), Lesions, Analysis, Load, Save, Copy circuit as JSON, Paste circuit JSON and Import from a passed level. Several of these appear only once the level or the campaign has reached them.

### Lesion lab

Run lesion, Sweep all singles, Clear set, Exit; Copy Markdown, Copy CSV, Save files; Cancel while a run is going.

### Diagnosis case

LESION LAB, the candidate list, SUBMIT DIAGNOSIS, REVEAL ANSWER (after three wrong guesses), Back to the case.

### Analysis

A tab stepper (3-node, 4-node, HRV, PRC, bursts, energy, STA), a graph stepper (populations or neurons), an edges stepper (all, excitatory, inhibitory), a nulls stepper (20, 100, 500), a population stepper, a record-length stepper (10 s, 30 s, 60 s, full), and RECORD.

### Notebook

Notes and Concepts tabs, What is real, Back.

### Settings

Display, Audio, Controls (a read-only list of the keys above) and Game tabs. The Game tab holds Mode, Record my play with EXPORT JSON and CLEAR, and the save buttons Export save, Import save, Open log folder and Reset progress. See [settings and performance](settings-and-performance.html).

## Phone and handheld

On a phone-sized window the lab uses three tabs, Circuit, World and Scopes. The More button reads Menu and also holds Brief, Hint and the parts button. The inspector is a sheet at the bottom. Tap targets are enlarged. Force a layout in Settings, Display, Layout.

## Related pages

- [Settings and performance](settings-and-performance.html)
- [How to read the lab](how-to-read-the-lab.html)
