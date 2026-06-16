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
		"fog": 0.025, "fog_color": Color(0.52, 0.57, 0.63),
		"light_range": 18.0, "light_energy": 5.0, "ambient": 0.16, "lamp": 0.85,
	},
	Level.NORMAL: {
		"name": "NORMAL",
		"fog": 0.050, "fog_color": Color(0.45, 0.48, 0.55),
		"light_range": 14.0, "light_energy": 4.0, "ambient": 0.105, "lamp": 0.68,
	},
	Level.HARD: {
		"name": "HARD",
		"fog": 0.085, "fog_color": Color(0.40, 0.42, 0.50),
		"light_range": 11.5, "light_energy": 3.2, "ambient": 0.070, "lamp": 0.52,
	},
	Level.NIGHTMARE: {
		"name": "NIGHTMARE",
		"fog": 0.125, "fog_color": Color(0.35, 0.37, 0.46),
		"light_range": 10.0, "light_energy": 2.7, "ambient": 0.045, "lamp": 0.40,
	},
	Level.DREAD: {
		"name": "DREAD",
		"fog": 0.170, "fog_color": Color(0.31, 0.33, 0.42),
		"light_range": 8.5, "light_energy": 2.3, "ambient": 0.032, "lamp": 0.30,
	},
	Level.ABYSS: {
		"name": "ABYSS",
		"fog": 0.220, "fog_color": Color(0.28, 0.30, 0.39),
		"light_range": 7.5, "light_energy": 2.0, "ambient": 0.022, "lamp": 0.22,
	},
	Level.PITCH: {
		"name": "PITCH",
		"fog": 0.270, "fog_color": Color(0.25, 0.27, 0.36),
		"light_range": 6.5, "light_energy": 1.8, "ambient": 0.014, "lamp": 0.14,
	},
}

# The "see everything" state, reserved for DEATH: when you die the fog lifts and
# the world goes bright/clear. Not part of the playable cycle -- the death/enemy
# system will apply this once it exists.
const DEATH_VIEW := {
	"name": "DEAD",
	"fog": 0.004, "fog_color": Color(0.60, 0.65, 0.72),
	"light_range": 28.0, "light_energy": 7.5, "ambient": 0.55, "lamp": 1.30,
}

func current() -> Dictionary:
	return PRESETS[level]

func cycle() -> void:
	level = (level + 1) % PRESETS.size()
	changed.emit()
