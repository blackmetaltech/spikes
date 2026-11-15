class_name Screen
extends Control
## Base of every full-screen page. Code-built: `refresh()` clears and rebuilds the children for the current layout.
## `app` is the shell (main.gd). Override `default_focus()` to say where the focus ring starts in nav mode, and `on_back()` to handle B / Esc.
var app

func _init() -> void:
	set_anchors_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_PASS

func refresh() -> void:
	for c in get_children():
		remove_child(c); c.queue_free()
	build()

func build() -> void: pass

func default_focus() -> Control: return null

## Return true when the screen consumed the back action; false lets the shell go to the previous screen.
func on_back() -> bool: return false

## Design scale for the current layout (1.0 at 1080p, down to 0.5 on phones).
func f() -> float: return Kit.f()

## Pad / key hint is useful everywhere except gameplay.
func wants_hint() -> bool: return true
