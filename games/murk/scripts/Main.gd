extends Node3D
## Parkour practice scene over animated lava. Two maps (N to switch): MAP A is a
## platform course, MAP B adds gapped staircases. Fall in the lava and you
## respawn at the start. Fog, flashlight and difficulty all apply; M cycles
## movement-feel modes.

const PlayerScript := preload("res://scripts/Player.gd")
const MovingFog := preload("res://shaders/moving_fog.gdshader")
const Lava := preload("res://shaders/lava.gdshader")
const STAMINA_BAR_W := 180.0
const KILL_Y := -1.5

# MAP A: platform course as [top-centre, size] pairs.
const COURSE_A := [
	[Vector3(0.0, 1.0, 16.0), Vector3(6.0, 1.0, 6.0)],
	[Vector3(0.0, 1.4, 12.5), Vector3(3.0, 1.0, 3.0)],
	[Vector3(2.5, 1.7, 9.8), Vector3(2.2, 1.0, 2.2)],
	[Vector3(-0.5, 2.1, 7.0), Vector3(2.2, 1.0, 2.2)],
	[Vector3(-3.0, 1.8, 4.0), Vector3(2.0, 1.0, 2.0)],
	[Vector3(-0.5, 2.3, 1.0), Vector3(2.0, 1.0, 2.0)],
	[Vector3(2.5, 2.6, -2.0), Vector3(2.0, 1.0, 2.0)],
	[Vector3(5.0, 3.0, -5.0), Vector3(2.0, 1.0, 2.0)],
	[Vector3(2.0, 3.2, -8.0), Vector3(2.0, 1.0, 2.0)],
	[Vector3(-1.5, 2.8, -10.5), Vector3(2.0, 1.0, 2.0)],
	[Vector3(-4.5, 2.3, -13.0), Vector3(2.2, 1.0, 2.2)],
	[Vector3(-8.0, 1.8, -16.0), Vector3(6.0, 1.0, 6.0)],
]

var _env: Environment
var _fog_material: ShaderMaterial
var _hud: Label
var _mode_label: Label
var _player
var _stamina_fill: ColorRect
var _course_root: Node3D
var _map: int = 0
var _spawn_point := Vector3(0.0, 2.0, 16.0)

func _ready() -> void:
	_build_environment()
	_build_lava()
	_build_lights()
	_rebuild_map()        # builds the course + sets the spawn point
	_build_player()
	_build_fog_volume()
	_build_hud()
	Difficulty.changed.connect(_apply_difficulty)
	_apply_difficulty()

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and not event.echo and event.keycode == KEY_N:
		_map = (_map + 1) % 2
		_rebuild_map()

func _process(_delta: float) -> void:
	if _player and _stamina_fill:
		_stamina_fill.size.x = STAMINA_BAR_W * _player.stamina_ratio()
		_stamina_fill.color = Color(1.0, 0.5, 0.2, 0.9) if _player.stamina_blocked() \
			else Color(0.45, 0.78, 1.0, 0.9)
	if _player and _mode_label:
		var map_name := "A" if _map == 0 else "B"
		_mode_label.text = "MOVE MODE: %s   |   MAP %s   (M / N)" % [_player.movement_mode_name(), map_name]
	if _player and _player.global_position.y < KILL_Y:
		_player.global_position = _spawn_point
		_player.velocity = Vector3.ZERO

# ---- world ----------------------------------------------------------------

func _build_environment() -> void:
	_env = Environment.new()
	_env.background_mode = Environment.BG_COLOR
	_env.background_color = Color(0.02, 0.02, 0.035)
	_env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	_env.ambient_light_color = Color(0.45, 0.50, 0.62)
	_env.ambient_light_energy = 0.20
	_env.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	_env.volumetric_fog_enabled = true
	_env.volumetric_fog_density = 0.035
	_env.volumetric_fog_albedo = Color(0.62, 0.64, 0.72)
	_env.volumetric_fog_length = 64.0
	_env.volumetric_fog_temporal_reprojection_enabled = true
	_env.volumetric_fog_temporal_reprojection_amount = 0.95
	_env.volumetric_fog_ambient_inject = 1.5
	_env.volumetric_fog_gi_inject = 0.0
	var world_env := WorldEnvironment.new()
	world_env.environment = _env
	add_child(world_env)

func _build_lava() -> void:
	var plane := PlaneMesh.new()
	plane.size = Vector2(160.0, 160.0)
	var mi := MeshInstance3D.new()
	mi.mesh = plane
	mi.position = Vector3(0.0, -3.0, 0.0)
	var sm := ShaderMaterial.new()
	sm.shader = Lava
	mi.material_override = sm
	add_child(mi)
	_add_lava_glow(Vector3(0.0, -0.5, 8.0))
	_add_lava_glow(Vector3(2.0, -0.5, -3.0))
	_add_lava_glow(Vector3(-5.0, -0.5, -14.0))

func _add_lava_glow(pos: Vector3) -> void:
	var glow := OmniLight3D.new()
	glow.position = pos
	glow.light_color = Color(1.0, 0.45, 0.12)
	glow.light_energy = 2.5
	glow.omni_range = 16.0
	add_child(glow)

func _build_lights() -> void:
	_add_lamp(Vector3(0.0, 5.0, 12.0), Color(0.7, 0.8, 1.0), 4.0, 18.0)
	_add_lamp(Vector3(1.0, 6.0, -3.0), Color(0.8, 0.85, 1.0), 4.0, 20.0)
	_add_lamp(Vector3(-6.0, 5.0, -16.0), Color(0.7, 0.8, 1.0), 4.0, 18.0)

func _add_lamp(pos: Vector3, color: Color, energy: float, range_m: float) -> void:
	var lamp := OmniLight3D.new()
	lamp.position = pos
	lamp.light_color = color
	lamp.light_energy = energy
	lamp.omni_range = range_m
	lamp.set_meta("base_energy", energy)
	lamp.add_to_group("lamp")
	add_child(lamp)

# ---- maps -----------------------------------------------------------------

func _platform_material() -> StandardMaterial3D:
	var mat := StandardMaterial3D.new()
	mat.albedo_color = Color(0.22, 0.23, 0.27)
	mat.roughness = 1.0
	return mat

func _add_platform(parent: Node3D, top_centre: Vector3, size: Vector3, mat: Material) -> void:
	var block := CSGBox3D.new()
	block.size = size
	block.position = top_centre - Vector3(0.0, size.y * 0.5, 0.0)
	block.material = mat
	block.use_collision = true
	parent.add_child(block)

# A flight of steps from base_top in `dir`, each step `rise` higher and `run`
# further. Indices in `missing` are skipped, leaving gaps to jump.
func _add_staircase(parent: Node3D, base_top: Vector3, dir: Vector3, count: int,
		rise: float, run: float, missing: Array, mat: Material) -> void:
	for i in range(1, count + 1):
		if (i - 1) in missing:
			continue
		var top := base_top + dir * (run * float(i)) + Vector3(0.0, rise * float(i), 0.0)
		_add_platform(parent, top, Vector3(2.2, 0.3, 1.4), mat)

func _rebuild_map() -> void:
	if _course_root:
		_course_root.queue_free()
	_course_root = Node3D.new()
	add_child(_course_root)
	var mat := _platform_material()
	if _map == 0:
		_spawn_point = Vector3(0.0, 2.0, 16.0)
		for entry in COURSE_A:
			_add_platform(_course_root, entry[0], entry[1], mat)
	else:
		_spawn_point = Vector3(0.0, 2.0, 18.0)
		_build_map_b(mat)
	if _player:
		_player.global_position = _spawn_point
		_player.velocity = Vector3.ZERO

# MAP B: gapped staircases + harder jumps.
func _build_map_b(mat: Material) -> void:
	_add_platform(_course_root, Vector3(0.0, 1.0, 18.0), Vector3(6.0, 1.0, 6.0), mat)   # start
	# Ascending stairs with two missing steps (gaps to jump over).
	_add_staircase(_course_root, Vector3(0.0, 1.0, 15.0), Vector3(0.0, 0.0, -1.0), 9, 0.35, 1.2, [3, 6], mat)
	# Floating jumps off the top of the stairs.
	_add_platform(_course_root, Vector3(2.8, 4.2, 1.5), Vector3(2.2, 1.0, 2.2), mat)
	_add_platform(_course_root, Vector3(-1.0, 4.6, -2.0), Vector3(2.0, 1.0, 2.0), mat)
	# Descending stairs with a gap, leading down to the goal.
	_add_staircase(_course_root, Vector3(-1.0, 4.6, -3.5), Vector3(0.0, 0.0, -1.0), 7, -0.4, 1.2, [2, 5], mat)
	_add_platform(_course_root, Vector3(-1.0, 1.6, -14.0), Vector3(6.0, 1.0, 6.0), mat)  # goal

# ---- player + hud ---------------------------------------------------------

func _build_player() -> void:
	var player := CharacterBody3D.new()
	player.position = _spawn_point
	player.set_script(PlayerScript)
	player.add_to_group("player")

	var col := CollisionShape3D.new()
	col.name = "Collision"
	var capsule := CapsuleShape3D.new()
	capsule.radius = 0.4
	capsule.height = 1.8
	col.shape = capsule
	col.position = Vector3(0.0, 0.9, 0.0)
	player.add_child(col)

	var cam := Camera3D.new()
	cam.name = "Camera"
	cam.position = Vector3(0.0, 1.6, 0.0)
	cam.current = true
	player.add_child(cam)

	var flashlight := SpotLight3D.new()
	flashlight.position = Vector3(0.25, -0.2, 0.0)
	flashlight.spot_range = 15.0
	flashlight.spot_angle = 35.0
	flashlight.light_energy = 5.0
	flashlight.shadow_enabled = true
	flashlight.add_to_group("flashlight")
	cam.add_child(flashlight)

	_player = player
	add_child(player)

func _build_fog_volume() -> void:
	var fog := FogVolume.new()
	fog.shape = RenderingServer.FOG_VOLUME_SHAPE_BOX
	fog.size = Vector3(80.0, 16.0, 80.0)
	fog.position = Vector3(0.0, 4.0, 0.0)
	_fog_material = ShaderMaterial.new()
	_fog_material.shader = MovingFog
	fog.material = _fog_material
	add_child(fog)

func _build_hud() -> void:
	var canvas := CanvasLayer.new()
	add_child(canvas)
	_hud = Label.new()
	_hud.position = Vector2(20.0, 18.0)
	_hud.add_theme_font_size_override("font_size", 22)
	canvas.add_child(_hud)

	var help := Label.new()
	help.position = Vector2(20.0, 50.0)
	help.text = "WASD - Shift sprint - Ctrl crouch - Space jump - M mode - N map - F light - TAB difficulty - lava = respawn"
	canvas.add_child(help)

	_mode_label = Label.new()
	_mode_label.position = Vector2(20.0, 72.0)
	canvas.add_child(_mode_label)

	var stam_label := Label.new()
	stam_label.position = Vector2(20.0, 98.0)
	stam_label.text = "STAMINA"
	canvas.add_child(stam_label)
	var stam_bg := ColorRect.new()
	stam_bg.position = Vector2(110.0, 100.0)
	stam_bg.size = Vector2(STAMINA_BAR_W, 14.0)
	stam_bg.color = Color(0.10, 0.10, 0.13, 0.8)
	canvas.add_child(stam_bg)
	_stamina_fill = ColorRect.new()
	_stamina_fill.position = Vector2(110.0, 100.0)
	_stamina_fill.size = Vector2(STAMINA_BAR_W, 14.0)
	_stamina_fill.color = Color(0.45, 0.78, 1.0, 0.9)
	canvas.add_child(_stamina_fill)

func _apply_difficulty() -> void:
	var d := Difficulty.current()
	_env.volumetric_fog_density = d["fog"]
	_env.volumetric_fog_albedo = d["fog_color"]
	_env.ambient_light_energy = d["ambient"]

	if _fog_material:
		_fog_material.set_shader_parameter("base_density", float(d["fog"]) * 0.6)
		var c: Color = d["fog_color"]
		_fog_material.set_shader_parameter("fog_albedo", Vector3(c.r, c.g, c.b))

	for light in get_tree().get_nodes_in_group("flashlight"):
		light.spot_range = d["light_range"]
		light.light_energy = d["light_energy"]

	var lamp_scale: float = d.get("lamp", 1.0)
	for lamp in get_tree().get_nodes_in_group("lamp"):
		lamp.light_energy = float(lamp.get_meta("base_energy")) * lamp_scale

	if _hud:
		_hud.text = "DIFFICULTY: %s    (fog %.3f  -  light %.0fm)" % [
			d["name"], d["fog"], d["light_range"]]
