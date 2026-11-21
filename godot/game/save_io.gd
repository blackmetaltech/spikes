class_name SaveIO
extends RefCounted
## Every file the game keeps for the player (progress, settings, goals, play log, circuit autosaves) goes through here.
##
## WRITING  write_text(path, text): the text goes to <path>.tmp, is read back for its length, three rotating backups (<path>.bak1 .. .bak3) are
##          kept, and the temp file is renamed over the original (atomic on one file system): a kill or power loss leaves the old file or the new one,
##          never half of one. A failed write (a read-only disk, a full one) is reported ONCE per file as a notice with the path (warn_hook shows it).
## READING  load_cfg(path) / load_json(path): the file, then its backups in turn. A file that does not parse, is empty, or lacks its end marker is damaged:
##          it is moved aside (<path>.corrupt-<time>), the first good backup is used, and if there is none the file stays untouched in its quarantine,
##          saving to that path is switched off for the session and a notice names where the file is (the title screen shows `notices`).
## VERSION  [meta] save_version (VERSION below). A file written by a NEWER game is read but never written: read-only, with a notice.
## HOOKS    Test hooks (SP_*) never write into the real save: enabled() is false when any hook other than SP_SCREEN / SP_LAYOUT is set, unless SP_ALLOW_SAVE=1.

const VERSION := 1
const BACKUPS := 3
const END_SECTION := "zz_end"
## The SP_* hooks that do not turn saving off (they only choose what is shown).
const HOOKS_QUIET := ["SP_SCREEN", "SP_LAYOUT", "SP_ALLOW_SAVE"]
## Every other hook the game knows (main.gd header). OS has no way to list the environment, so they are named.
const HOOKS := ["SP_TASK", "SP_REF", "SP_SPEED", "SP_SEL", "SP_SMOKE", "SP_SHOT", "SP_SHOT_AT", "SP_CHAPTER", "SP_NODE", "SP_MOCK", "SP_UNLOCK_ALL", "SP_PROGRESS", "SP_MODE",
	"SP_CATALOG", "SP_LESION", "SP_TAB", "SP_UIGRAPH", "SP_UIGRAPH_ONLY", "SP_NODISPLAY", "SP_NOTHROTTLE", "SP_PAD", "SP_CIRCUIT", "SP_CHECK_EXPORT", "SP_CHECK", "SP_HINT", "SP_VERDICT",
	"SP_HISTO", "SP_CELEB", "SP_SKIP", "SP_FAILS", "SP_BRIEF", "SP_DXPICK", "SP_DXSUBMIT", "SP_FAKEHINTS", "SP_TESTNOW", "SP_VARIANT", "SP_LABTAB", "SP_ADV", "SP_PARTS", "SP_MENU",
	"SP_SIM_UNTIL", "SP_ATAB", "SP_NB_TAB", "SP_NB_SCROLL", "SP_REAL_SCROLL", "SP_CREDITS_SCROLL", "SP_MODDIALS", "SP_ALLTOOLS", "SP_AREC", "SP_LESIONS", "SP_TIMING", "SP_TIMING_QUIT",
	"SP_FPS", "SP_BUDGET_US"]

static var notices: Array = []            # messages the title screen shows (damaged or newer files, a read-only disk)
static var readonly := {}                 # path -> true: never written this session
static var status := {}                   # path -> "missing" | "ok" | "recovered" | "bad" | "future"
static var write_errors := 0
static var warn_hook := Callable()        # called once per notice (main.gd shows it as a banner)
static var _warned := {}

static func forget_state() -> void:
	notices.clear(); readonly.clear(); status.clear(); _warned.clear()
	SaveIO.write_errors = 0

## Saving is on: not a test run (Settings.no_save, SP_NOAUTOSAVE) and no dev hook other than SP_SCREEN / SP_LAYOUT (unless SP_ALLOW_SAVE=1).
static func enabled() -> bool:
	if Settings.no_save or OS.has_environment("SP_NOAUTOSAVE"): return false
	if OS.get_environment("SP_ALLOW_SAVE") == "1": return true
	for h in HOOKS:
		if OS.get_environment(h) != "": return false
	return true

static func g(path: String) -> String: return ProjectSettings.globalize_path(path)

static func notice(msg: String, key := "") -> void:
	if key != "":
		if _warned.has(key): return
		_warned[key] = true
	notices.append(msg)
	log_line(msg)
	if warn_hook.is_valid(): warn_hook.call(msg)

# ---------------------------------------------------------------- log
const LOG := "user://logs/spike.log"
const LOG_KEEP := 3

## Rotate spike.log -> spike.1.log -> spike.2.log and start a new one with `first`.
static func log_start(first: String) -> void:
	if not enabled(): return
	DirAccess.make_dir_recursive_absolute(g("user://logs"))
	for i in range(LOG_KEEP - 1, 0, -1):
		var a := LOG if i == 1 else "user://logs/spike.%d.log" % (i - 1)
		var b := "user://logs/spike.%d.log" % i
		if FileAccess.file_exists(a): DirAccess.rename_absolute(g(a), g(b))
	var f := FileAccess.open(LOG, FileAccess.WRITE)
	if f != null: f.store_string(first + "\n"); f.close()

static func log_line(msg: String) -> void:
	if not enabled(): return
	var f := FileAccess.open(LOG, FileAccess.READ_WRITE)
	if f == null: return
	f.seek_end(); f.store_string("[%s] %s\n" % [Time.get_datetime_string_from_system(), msg]); f.close()

static func log_dir() -> String: return g("user://logs")

# ---------------------------------------------------------------- writing
static func _fail(path: String, why: String) -> bool:
	write_errors += 1
	notice("Could not save %s (%s). Anything changed since the last good save will be lost when the game closes. Check that the folder is writable and the disk is not full." % [g(path), why], "w:" + path)
	return false

static func _bak(path: String, i: int) -> String: return "%s.bak%d" % [path, i]

static func _rotate(path: String) -> void:
	var last := _bak(path, BACKUPS)
	if FileAccess.file_exists(last): DirAccess.remove_absolute(g(last))
	for i in range(BACKUPS - 1, 0, -1):
		if FileAccess.file_exists(_bak(path, i)): DirAccess.rename_absolute(g(_bak(path, i)), g(_bak(path, i + 1)))
	DirAccess.copy_absolute(g(path), g(_bak(path, 1)))

## Atomic write. `rotate_secs`: keep a new backup only when the newest one is older than that (0 = at every write; autosaves use minutes, -1 = no backups).
## Returns true when the file on disk now holds `text`. Not enabled() or read-only paths return false without a notice.
static func write_text(path: String, text: String, rotate_secs := 0.0) -> bool:
	if readonly.has(path): return false
	DirAccess.make_dir_recursive_absolute(g(path.get_base_dir()))
	var tmp := path + ".tmp"
	var f := FileAccess.open(tmp, FileAccess.WRITE)
	if f == null: return _fail(path, error_string(FileAccess.get_open_error()))
	var ok := f.store_string(text); f.flush(); var err := f.get_error(); f.close()
	if not ok or err != OK or FileAccess.get_file_as_bytes(tmp).size() != text.to_utf8_buffer().size():
		DirAccess.remove_absolute(g(tmp)); return _fail(path, "the write did not complete" if err == OK else error_string(err))
	if FileAccess.file_exists(path) and rotate_secs >= 0.0:
		var b1 := _bak(path, 1)
		if rotate_secs == 0.0 or not FileAccess.file_exists(b1) or Time.get_unix_time_from_system() - float(FileAccess.get_modified_time(b1)) >= rotate_secs: _rotate(path)
	var rn := DirAccess.rename_absolute(g(tmp), g(path))
	if rn != OK:
		DirAccess.remove_absolute(g(tmp)); return _fail(path, error_string(rn))
	return true

# ---------------------------------------------------------------- reading
static func _stamp() -> String: return Time.get_datetime_string_from_system().replace(":", "").replace("-", "")

## Move a damaged file out of the way (never delete it). Returns where it went ("" when it could not be moved).
static func quarantine(path: String, suffix := "corrupt") -> String:
	if not FileAccess.file_exists(path): return ""
	var to := "%s.%s-%s" % [path, suffix, _stamp()]
	var n := 1
	while FileAccess.file_exists(to): to = "%s.%s-%s-%d" % [path, suffix, _stamp(), n]; n += 1
	return g(to) if DirAccess.rename_absolute(g(path), g(to)) == OK else ""

## Why `text` is not a good save: "" = fine, "future:<v>" = written by a newer game, else a reason.
static func check_cfg(text: String) -> String:
	if text.strip_edges() == "": return "the file is empty"
	var c := ConfigFile.new()
	if c.parse(text) != OK: return "the file does not parse (cut short or damaged)"
	var v := int(c.get_value("meta", "save_version", 0))
	if v > VERSION: return "future:%d" % v
	if not (c.has_section("meta") or c.has_section("best") or c.has_section("a") or c.has_section("goals")): return "the file holds no save data"
	if v >= 1 and not c.has_section(END_SECTION): return "the file is cut short (no end marker)"
	return ""

static func check_json(text: String) -> String:
	if text.strip_edges() == "": return "the file is empty"
	var v: Variant = JSON.parse_string(text)
	if not v is Dictionary: return "the file does not parse (cut short or damaged)"
	var sv := int((v as Dictionary).get("save_version", 0))
	if sv > VERSION: return "future:%d" % sv
	return ""

## Load `path` or its newest good backup. Returns {status, text, notice}. status: missing | ok | recovered | bad | future.
static func load_text(path: String, check: Callable, what: String) -> Dictionary:
	if not FileAccess.file_exists(path):
		status[path] = "missing"; return {"status": "missing", "text": ""}
	var text := FileAccess.get_file_as_string(path)
	var why: String = check.call(text)
	if why == "":
		status[path] = "ok"; return {"status": "ok", "text": text}
	if why.begins_with("future"):
		readonly[path] = true; status[path] = "future"
		notice("%s was saved by a newer version of the game (%s). It is open read-only: nothing will be written to it, so a newer game can still use it. File: %s" % [what, why.get_slice(":", 1), g(path)], "f:" + path)
		return {"status": "future", "text": text}
	var moved := quarantine(path)
	for i in range(1, BACKUPS + 1):
		var b := _bak(path, i)
		if not FileAccess.file_exists(b): continue
		var bt := FileAccess.get_file_as_string(b)
		if str(check.call(bt)) == "":
			status[path] = "recovered"
			notice("%s was damaged (%s). The last good backup (%d save%s ago) was used. The damaged file is kept at %s" % [what, why, i, "" if i == 1 else "s", moved if moved != "" else g(path)], "r:" + path)
			return {"status": "recovered", "text": bt}
	readonly[path] = true; status[path] = "bad"
	notice("%s is damaged (%s) and no backup could be read. The file was moved to %s and saving it is switched off for this session so it cannot be overwritten. You can still copy it somewhere safe." % [what, why, moved if moved != "" else g(path)], "b:" + path)
	return {"status": "bad", "text": ""}

## A ConfigFile from `path` (or a backup); null when there is none (fresh start) or it is unusable (see status[path]).
static func load_cfg(path: String, what := "A save file") -> ConfigFile:
	var r := load_text(path, check_cfg, what)
	if r["status"] in ["missing", "bad"]: return null
	var c := ConfigFile.new()
	return c if c.parse(str(r["text"])) == OK else null

## A Dictionary from a JSON file (or backup); {} when there is none or it is unusable.
static func load_json(path: String, what := "A save file") -> Dictionary:
	var r := load_text(path, check_json, what)
	if r["status"] in ["missing", "bad"]: return {}
	var v: Variant = JSON.parse_string(str(r["text"]))
	return v if v is Dictionary else {}

## Write a ConfigFile with its version and end marker.
static func save_cfg(path: String, c: ConfigFile, rotate_secs := 0.0) -> bool:
	c.set_value("meta", "save_version", VERSION)
	c.set_value(END_SECTION, "ok", true)
	return write_text(path, c.encode_to_text(), rotate_secs)

## Write a Dictionary as JSON with save_version.
static func save_json(path: String, d: Dictionary, rotate_secs := 300.0, indent := "") -> bool:
	var o := d.duplicate(); o["save_version"] = VERSION
	return write_text(path, JSON.stringify(o, indent), rotate_secs)

## Delete a file and its backups (Reset progress, Clear play log).
static func remove_all(path: String) -> void:
	for p in [path, path + ".tmp", _bak(path, 1), _bak(path, 2), _bak(path, 3)]:
		if FileAccess.file_exists(p): DirAccess.remove_absolute(g(p))
	readonly.erase(path); status.erase(path)

# ---------------------------------------------------------------- the whole save as one zip
const SAVE_FILES := ["user://progress.cfg", "user://settings.cfg", "user://goals.cfg", "user://playlog.json"]
const MANIFEST := "spike-save.json"

static func _circuit_entry(name: String) -> bool:
	var r := RegEx.new(); r.compile("^circuits/last_[A-Za-z0-9_]{1,64}\\.json$")
	return r.search(name) != null

## Pack the player's save (progress, settings, goals, play log, every circuit autosave) into one zip. Returns "" or an error.
static func export_zip(path: String) -> String:
	var z := ZIPPacker.new()
	if z.open(path) != OK: return "could not write " + path
	var files := {}
	for p in SAVE_FILES:
		if FileAccess.file_exists(p): files[(p as String).get_file()] = p
	var d := DirAccess.open("user://circuits")
	if d != null:
		for fn in d.get_files():
			if fn.begins_with("last_") and fn.ends_with(".json"): files["circuits/" + fn] = "user://circuits/" + fn
	z.start_file(MANIFEST); z.write_file(JSON.stringify({"format": "spike-save-1", "save_version": VERSION, "files": files.keys(), "exported": Time.get_datetime_string_from_system()}).to_utf8_buffer()); z.close_file()
	for name in files:
		z.start_file(name); z.write_file(FileAccess.get_file_as_bytes(files[name])); z.close_file()
	z.close()
	return ""

## Check a save zip and, if every file in it is sound, replace the current save with it (each replaced file keeps its backups). Returns "" or why nothing was imported.
static func import_zip(path: String) -> String:
	var z := ZIPReader.new()
	if z.open(path) != OK: return "not a zip file: " + path
	var names := z.get_files()
	if not MANIFEST in names: z.close(); return "not a Spike save (no %s)" % MANIFEST
	var man: Variant = JSON.parse_string(z.read_file(MANIFEST).get_string_from_utf8())
	if not man is Dictionary or str(man.get("format", "")) != "spike-save-1": z.close(); return "not a Spike save (unknown format)"
	if int(man.get("save_version", 0)) > VERSION: z.close(); return "this save was made by a newer version of the game"
	var plan := {}
	for n in names:
		var name := str(n)
		if name == MANIFEST or name.ends_with("/"): continue
		var text := z.read_file(name).get_string_from_utf8()
		var why := ""
		if name in ["progress.cfg", "settings.cfg", "goals.cfg"]: why = check_cfg(text)
		elif name == "playlog.json": why = check_json(text)
		elif _circuit_entry(name):
			var r := Circuits.parse_checked(text)
			why = str(r["error"])
		else: z.close(); return "unexpected file in the save: " + name
		if why != "": z.close(); return "%s is not usable (%s)" % [name, why.trim_prefix("future:")]
		plan["user://" + name] = text
	z.close()
	for p in plan:
		readonly.erase(p); status.erase(p)
		if not write_text(p, plan[p], 0.0): return "could not write " + g(p)
	return ""

## The game's build: the git commit baked in by scripts/package.sh (res://build_id.txt), or "dev".
static func build_id() -> String:
	if FileAccess.file_exists("res://build_id.txt"):
		var t := FileAccess.get_file_as_string("res://build_id.txt").strip_edges()
		if t != "": return t
	return "dev"
