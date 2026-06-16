extends CharacterBody3D
## First-person controller: look, move, sprint (stamina + anti-feather cooldown),
## crouch, jump, and flashlight toggle. Numbers are placeholders, easy to retune.

@export var speed: float = 4.5
@export var mouse_sensitivity: float = 0.0025
@export var gravity: float = 16.0
@export var jump_velocity: float = 5.0

# Sprint
@export var sprint_multiplier: float = 1.35
@export var max_stamina: float = 3.0         # seconds of sprint available
@export var stamina_recharge: float = 1.0    # refill rate once cooldown elapses
@export var stamina_cooldown: float = 0.75   # delay before refill starts (anti-feather)

# Crouch
@export var crouch_speed_multiplier: float = 0.5
const STAND_CAM_Y := 1.6
const CROUCH_CAM_Y := 0.9
const STAND_HEIGHT := 1.8
const CROUCH_HEIGHT := 1.0

var stamina: float = 3.0
var _recharge_timer: float = 0.0
var _crouch: float = 0.0  # 0 = standing, 1 = fully crouched
var _jump_queued: bool = false

var _camera: Camera3D
var _collision: CollisionShape3D
var _capsule: CapsuleShape3D
var _pitch: float = 0.0

func stamina_ratio() -> float:
	return stamina / max_stamina if max_stamina > 0.0 else 0.0

# True during the post-sprint cooldown, so the HUD can show why it isn't refilling.
func stamina_blocked() -> bool:
	return _recharge_timer > 0.0 and stamina < max_stamina

func _ready() -> void:
	_camera = get_node_or_null("Camera")
	_collision = get_node_or_null("Collision")
	if _collision:
		_capsule = _collision.shape as CapsuleShape3D
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
	# Gravity + jump.
	if is_on_floor():
		if velocity.y < 0.0:
			velocity.y = 0.0
		if _jump_queued:
			velocity.y = jump_velocity
	else:
		velocity.y -= gravity * delta
	_jump_queued = false

	# Crouch (hold Ctrl): smoothly shrink the capsule + lower the camera.
	var want_crouch := Input.is_physical_key_pressed(KEY_CTRL)
	var crouch_target: float = 1.0 if want_crouch else 0.0
	var crouch_step: float = delta * 8.0
	if _crouch < crouch_target:
		_crouch = minf(_crouch + crouch_step, crouch_target)
	else:
		_crouch = maxf(_crouch - crouch_step, crouch_target)
	_apply_crouch()
	var crouching := _crouch > 0.5

	# Movement input.
	var input_dir := Vector3.ZERO
	if Input.is_physical_key_pressed(KEY_W): input_dir.z -= 1.0
	if Input.is_physical_key_pressed(KEY_S): input_dir.z += 1.0
	if Input.is_physical_key_pressed(KEY_A): input_dir.x -= 1.0
	if Input.is_physical_key_pressed(KEY_D): input_dir.x += 1.0
	var moving := input_dir != Vector3.ZERO

	# Sprint with an anti-feather cooldown: every frame you sprint resets the
	# cooldown, so tapping Shift can never trickle-charge a permanent boost.
	var sprinting := moving and not crouching \
		and Input.is_physical_key_pressed(KEY_SHIFT) and stamina > 0.0
	if sprinting:
		stamina = max(stamina - delta, 0.0)
		_recharge_timer = stamina_cooldown
	elif _recharge_timer > 0.0:
		_recharge_timer = max(_recharge_timer - delta, 0.0)
	else:
		stamina = min(stamina + stamina_recharge * delta, max_stamina)

	var current_speed := speed
	if crouching:
		current_speed *= crouch_speed_multiplier
	elif sprinting:
		current_speed *= sprint_multiplier

	var dir := transform.basis * input_dir.normalized()
	dir.y = 0.0
	dir = dir.normalized()
	velocity.x = dir.x * current_speed
	velocity.z = dir.z * current_speed
	move_and_slide()

func _apply_crouch() -> void:
	if _camera:
		_camera.position.y = lerpf(STAND_CAM_Y, CROUCH_CAM_Y, _crouch)
	if _capsule and _collision:
		var h: float = lerpf(STAND_HEIGHT, CROUCH_HEIGHT, _crouch)
		_capsule.height = h
		_collision.position.y = h * 0.5
