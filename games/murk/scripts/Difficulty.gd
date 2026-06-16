extends Node
## Difficulty is expressed through VISIBILITY: harder = thicker fog and a
## shorter flashlight reach. This autoload is the single source of truth; the
## level listens to `changed` and re-applies fog + light settings live.

signal changed

enum Level { EASY, NORMAL, HARD, NIGHTMARE, DREAD, ABYSS, PITCH }

var level: int = Level.NORMAL

# The darker tiers thicken the fog and drop the ambient light, but the
# flashlight stays strong on purpose: darkness should feel oppressive, never
# "I literally can't see, this is just annoying". PITCH is the floor of
# viability, not a black screen.
const PRESETS := {
	Level.EASY: {
		"name": "EASY",
		"fog": 0.012, "fog_color": Color(0.56, 0.61, 0.67),
		"light_range": 24.0, "light_energy": 6.0, "ambient": 0.32,
	},
	Level.NORMAL: {
		"name": "NORMAL",
		"fog": 0.030, "fog_color": Color(0.48, 0.51, 0.58),
		"light_range": 17.0, "light_energy": 5.5, "ambient": 0.22,
	},
	Level.HARD: {
		"name": "HARD",
		"fog": 0.050, "fog_color": Color(0.42, 0.44, 0.52),
		"light_range": 13.0, "light_energy": 5.0, "ambient": 0.15,
	},
	Level.NIGHTMARE: {
		"name": "NIGHTMARE",
		"fog": 0.075, "fog_color": Color(0.37, 0.39, 0.48),
		"light_range": 10.0, "light_energy": 4.8, "ambient": 0.10,
	},
	Level.DREAD: {
		"name": "DREAD",
		"fog": 0.100, "fog_color": Color(0.33, 0.35, 0.44),
		"light_range": 9.0, "light_energy": 4.8, "ambient": 0.075,
	},
	Level.ABYSS: {
		"name": "ABYSS",
		"fog": 0.130, "fog_color": Color(0.30, 0.32, 0.41),
		"light_range": 8.0, "light_energy": 4.8, "ambient": 0.055,
	},
	Level.PITCH: {
		"name": "PITCH",
		"fog": 0.160, "fog_color": Color(0.27, 0.29, 0.38),
		"light_range": 7.0, "light_energy": 4.8, "ambient": 0.040,
	},
}

func current() -> Dictionary:
	return PRESETS[level]

func cycle() -> void:
	level = (level + 1) % PRESETS.size()
	changed.emit()
