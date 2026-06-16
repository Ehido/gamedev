extends CharacterBody3D
## First-person controller tuned for a smooth, weighty feel: momentum-based
## acceleration + friction, limited air control that preserves speed through
## jumps, coyote-time + jump-buffering for forgiving parkour, sprint (stamina
## with anti-feather cooldown), crouch, and subtle camera juice (head-bob,
## sprint FOV, landing dip). Numbers are placeholders -- easy to retune.

@export var speed: float = 4.5
@export var sprint_multiplier: float = 1.7
@export var crouch_speed_multiplier: float = 0.5
@export var mouse_sensitivity: float = 0.0025

@export var gravity: float = 18.0
@export var jump_velocity: float = 7.0
@export var coyote_time: float = 0.12       # grace to still jump after a ledge
@export var jump_buffer_time: float = 0.12  # grace to queue a jump before landing

@export var ground_accel: float = 35.0      # how fast you reach top speed
@export var ground_friction: float = 30.0   # how fast you ease to a stop
@export var air_accel: float = 9.0          # limited steering in the air

# Sprint stamina
@export var max_stamina: float = 3.0
@export var stamina_recharge: float = 1.0
@export var stamina_cooldown: float = 0.75  # delay before refill starts (anti-feather)

# Camera juice
@export var base_fov: float = 75.0
@export var sprint_fov: float = 84.0
const STAND_CAM_Y := 1.6
const CROUCH_CAM_Y := 0.9
const STAND_HEIGHT := 1.8
const CROUCH_HEIGHT := 1.0
const MAX_STEP_HEIGHT := 0.45  # walk up steps/stairs no taller than this

# Switchable feel presets (M cycles them). WEIGHTY = recommended default.
const MOVEMENT_MODES := [
	{"name": "WEIGHTY", "accel": 35.0, "friction": 30.0, "air": 9.0,  "jump": 7.0, "grav": 18.0, "sprint": 1.7},
	{"name": "TIGHT",   "accel": 62.0, "friction": 55.0, "air": 16.0, "jump": 6.6, "grav": 22.0, "sprint": 1.5},
	{"name": "FLOW",    "accel": 28.0, "friction": 14.0, "air": 22.0, "jump": 7.6, "grav": 16.0, "sprint": 1.9},
]
var _mode: int = 0

var stamina: float = 3.0
var _recharge_timer: float = 0.0
var _crouch: float = 0.0
var _pitch: float = 0.0
var _coyote: float = 0.0
var _jump_buffer: float = 0.0
var _jump_queued: bool = false
var _was_on_floor: bool = true
var _bob_time: float = 0.0
var _land_offset: float = 0.0

var _camera: Camera3D
var _collision: CollisionShape3D
var _capsule: CapsuleShape3D

func stamina_ratio() -> float:
	return stamina / max_stamina if max_stamina > 0.0 else 0.0

func stamina_blocked() -> bool:
	return _recharge_timer > 0.0 and stamina < max_stamina

func movement_mode_name() -> String:
	return str(MOVEMENT_MODES[_mode]["name"])

func cycle_movement_mode() -> void:
	_mode = (_mode + 1) % MOVEMENT_MODES.size()
	_apply_movement_mode()

func _apply_movement_mode() -> void:
	var m: Dictionary = MOVEMENT_MODES[_mode]
	ground_accel = float(m["accel"])
	ground_friction = float(m["friction"])
	air_accel = float(m["air"])
	jump_velocity = float(m["jump"])
	gravity = float(m["grav"])
	sprint_multiplier = float(m["sprint"])

func _ready() -> void:
	_camera = get_node_or_null("Camera")
	_collision = get_node_or_null("Collision")
	if _collision:
		_capsule = _collision.shape as CapsuleShape3D
	if _camera:
		_camera.fov = base_fov
	_apply_movement_mode()
	if DisplayServer.get_name() != "headless":
		Input.mouse_mode = Input.MOUSE_MODE_CAPTURED

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseMotion and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		rotate_y(-event.relative.x * mouse_sensitivity)
		_pitch = clamp(_pitch - event.relative.y * mouse_sensitivity, -1.4, 1.4)
		if _camera:
			_camera.rotation.x = _pitch
	elif event is InputEventMouseButton and event.pressed:
		if Input.mouse_mode != Input.MOUSE_MODE_CAPTURED:
			Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
	elif event is InputEventKey and event.pressed and not event.echo:
		match event.keycode:
			KEY_ESCAPE: _toggle_mouse()
			KEY_TAB: Difficulty.cycle()
			KEY_F: _toggle_flashlight()
			KEY_M: cycle_movement_mode()
			KEY_SPACE: _jump_queued = true

func _toggle_mouse() -> void:
	if Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	else:
		Input.mouse_mode = Input.MOUSE_MODE_CAPTURED

func _toggle_flashlight() -> void:
	for flashlight in get_tree().get_nodes_in_group("flashlight"):
		flashlight.visible = not flashlight.visible

func _physics_process(delta: float) -> void:
	var on_ground := is_on_floor()

	# Jump forgiveness timers.
	if on_ground:
		_coyote = coyote_time
	else:
		_coyote = maxf(_coyote - delta, 0.0)
	if _jump_queued:
		_jump_buffer = jump_buffer_time
		_jump_queued = false
	else:
		_jump_buffer = maxf(_jump_buffer - delta, 0.0)

	var incoming_fall := maxf(-velocity.y, 0.0)  # for the landing dip

	# Gravity.
	if not on_ground:
		velocity.y -= gravity * delta
	elif velocity.y < 0.0:
		velocity.y = 0.0

	# Buffered + coyote jump.
	if _jump_buffer > 0.0 and _coyote > 0.0:
		velocity.y = jump_velocity
		_jump_buffer = 0.0
		_coyote = 0.0
		on_ground = false

	# Crouch (hold Ctrl): smoothly shrink the capsule.
	var want_crouch := Input.is_physical_key_pressed(KEY_CTRL)
	var crouch_target: float = 1.0 if want_crouch else 0.0
	var crouch_step := delta * 8.0
	if _crouch < crouch_target:
		_crouch = minf(_crouch + crouch_step, crouch_target)
	else:
		_crouch = maxf(_crouch - crouch_step, crouch_target)
	_apply_crouch_collision()
	var crouching := _crouch > 0.5

	# Movement input -> world-space wish direction.
	var input_dir := Vector3.ZERO
	if Input.is_physical_key_pressed(KEY_W): input_dir.z -= 1.0
	if Input.is_physical_key_pressed(KEY_S): input_dir.z += 1.0
	if Input.is_physical_key_pressed(KEY_A): input_dir.x -= 1.0
	if Input.is_physical_key_pressed(KEY_D): input_dir.x += 1.0
	var moving := input_dir != Vector3.ZERO
	var wish_dir := transform.basis * input_dir
	wish_dir.y = 0.0
	if wish_dir.length() > 0.0:
		wish_dir = wish_dir.normalized()

	# Sprint with anti-feather cooldown.
	var sprinting := moving and not crouching \
		and Input.is_physical_key_pressed(KEY_SHIFT) and stamina > 0.0
	if sprinting:
		stamina = maxf(stamina - delta, 0.0)
		_recharge_timer = stamina_cooldown
	elif _recharge_timer > 0.0:
		_recharge_timer = maxf(_recharge_timer - delta, 0.0)
	else:
		stamina = minf(stamina + stamina_recharge * delta, max_stamina)

	var target_speed := speed
	if crouching:
		target_speed *= crouch_speed_multiplier
	elif sprinting:
		target_speed *= sprint_multiplier

	# Ground: friction + acceleration toward target. Air: preserve momentum and
	# only steer -- you never bleed speed in the air, so jumps keep their pace
	# (FLOW's high air accel makes the steering quick and floaty).
	var hvel := Vector3(velocity.x, 0.0, velocity.z)
	var target_vel := wish_dir * target_speed
	if on_ground:
		var accel := ground_accel if moving else ground_friction
		hvel = hvel.move_toward(target_vel, accel * delta)
	elif moving:
		var pre := hvel.length()
		hvel += wish_dir * air_accel * delta
		var limit := maxf(pre, target_speed)
		if hvel.length() > limit:
			hvel = hvel.normalized() * limit
	velocity.x = hvel.x
	velocity.z = hvel.z

	move_and_slide()
	if is_on_floor():
		_try_step_up()

	# Landing detection (for the camera dip).
	var now_on_floor := is_on_floor()
	var just_landed := now_on_floor and not _was_on_floor
	_was_on_floor = now_on_floor

	var hspeed := Vector2(velocity.x, velocity.z).length()
	_update_camera(delta, hspeed, sprinting, just_landed, incoming_fall, now_on_floor)

func _apply_crouch_collision() -> void:
	if _capsule and _collision:
		var h: float = lerpf(STAND_HEIGHT, CROUCH_HEIGHT, _crouch)
		_capsule.height = h
		_collision.position.y = h * 0.5

# Walk up small steps/stairs that move_and_slide would otherwise wall us against.
func _try_step_up() -> void:
	var horiz := Vector3(velocity.x, 0.0, velocity.z)
	if horiz.length() < 0.08:
		return
	# Only when we actually ran into a near-vertical wall this frame.
	var into_wall := false
	for i in range(get_slide_collision_count()):
		var c := get_slide_collision(i)
		if absf(c.get_normal().y) < 0.25 and horiz.dot(-c.get_normal()) > 0.0:
			into_wall = true
			break
	if not into_wall:
		return
	# Ghost-move up, forward, then down onto the higher surface; undo if there's
	# no floor up there (it was a real wall, not a step).
	var saved := global_transform
	var fwd := horiz.normalized() * 0.35
	move_and_collide(Vector3(0.0, MAX_STEP_HEIGHT, 0.0))
	move_and_collide(fwd)
	var down := move_and_collide(Vector3(0.0, -MAX_STEP_HEIGHT, 0.0))
	if down == null or down.get_normal().y < 0.7:
		global_transform = saved

func _update_camera(delta: float, hspeed: float, sprinting: bool,
		just_landed: bool, impact: float, on_ground: bool) -> void:
	if not _camera:
		return

	# Sprint widens the FOV slightly for a sense of speed.
	var target_fov: float = sprint_fov if sprinting else base_fov
	_camera.fov = lerpf(_camera.fov, target_fov, clampf(delta * 8.0, 0.0, 1.0))

	# Head-bob, scaled by how fast you're actually moving on the ground.
	var move_amt: float = clampf(hspeed / speed, 0.0, 1.4) if on_ground else 0.0
	_bob_time += delta * (6.0 + hspeed * 1.5)
	var bob_y := sin(_bob_time) * 0.030 * move_amt
	var bob_x := cos(_bob_time * 0.5) * 0.019 * move_amt

	# Landing dip: drop the camera on impact, then ease it back.
	if just_landed:
		_land_offset = -clampf(impact * 0.025, 0.0, 0.16)
	_land_offset = lerpf(_land_offset, 0.0, clampf(delta * 6.0, 0.0, 1.0))

	var base_y: float = lerpf(STAND_CAM_Y, CROUCH_CAM_Y, _crouch)
	_camera.position.y = base_y + bob_y + _land_offset
	_camera.position.x = bob_x
