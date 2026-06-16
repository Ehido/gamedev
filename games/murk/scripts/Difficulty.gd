extends Node
## Difficulty is expressed through VISIBILITY: harder = thicker fog and a
## shorter flashlight reach. This autoload is the single source of truth; the
## level listens to `changed` and re-applies fog + light settings live.

signal changed

enum Level { EASY, NORMAL, HARD, NIGHTMARE, DREAD, ABYSS, PITCH }

var level: int = Level.NORMAL

# Darker across the board, and the hard end ramps aggressively on EVERYTHING:
# fog thickens fast, ambient + room lamps + flashlight all drop. The flashlight
# is still the one thing you can rely on, but at the bottom you're hugging it.
# Each preset: fog density, fog tint, flashlight reach (m) + energy, global
# ambient, and "lamp" = a multiplier on the room work-lamps.
const PRESETS := {
	Level.EASY: {
		"name": "EASY",
		"fog": 0.015, "fog_color": Color(0.55, 0.60, 0.66),
		"light_range": 22.0, "light_energy": 6.0, "ambient": 0.22, "lamp": 1.00,
	},
	Level.NORMAL: {
		"name": "NORMAL",
		"fog": 0.040, "fog_color": Color(0.46, 0.49, 0.56),
		"light_range": 15.0, "light_energy": 5.2, "ambient": 0.13, "lamp": 0.80,
	},
	Level.HARD: {
		"name": "HARD",
		"fog": 0.075, "fog_color": Color(0.40, 0.42, 0.50),
		"light_range": 12.0, "light_energy": 4.8, "ambient": 0.080, "lamp": 0.60,
	},
	Level.NIGHTMARE: {
		"name": "NIGHTMARE",
		"fog": 0.115, "fog_color": Color(0.35, 0.37, 0.46),
		"light_range": 10.0, "light_energy": 4.5, "ambient": 0.050, "lamp": 0.45,
	},
	Level.DREAD: {
		"name": "DREAD",
		"fog": 0.160, "fog_color": Color(0.31, 0.33, 0.42),
		"light_range": 8.5, "light_energy": 4.2, "ambient": 0.035, "lamp": 0.33,
	},
	Level.ABYSS: {
		"name": "ABYSS",
		"fog": 0.210, "fog_color": Color(0.28, 0.30, 0.39),
		"light_range": 7.5, "light_energy": 4.0, "ambient": 0.025, "lamp": 0.24,
	},
	Level.PITCH: {
		"name": "PITCH",
		"fog": 0.260, "fog_color": Color(0.25, 0.27, 0.36),
		"light_range": 6.5, "light_energy": 3.8, "ambient": 0.016, "lamp": 0.15,
	},
}

func current() -> Dictionary:
	return PRESETS[level]

func cycle() -> void:
	level = (level + 1) % PRESETS.size()
	changed.emit()
