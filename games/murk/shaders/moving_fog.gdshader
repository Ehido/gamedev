shader_type fog;
// Dynamic, drifting fog: animated 3D value-noise drives the density so the
// murk rolls and breathes instead of sitting still. base_density and color are
// set from the current difficulty.

uniform float base_density = 0.04;
uniform vec3 fog_albedo = vec3(0.5, 0.52, 0.6);

float hash(vec3 p) {
	p = fract(p * 0.3183099 + 0.1);
	p *= 17.0;
	return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float vnoise(vec3 x) {
	vec3 i = floor(x);
	vec3 f = fract(x);
	f = f * f * (3.0 - 2.0 * f);
	float n000 = hash(i + vec3(0.0, 0.0, 0.0));
	float n100 = hash(i + vec3(1.0, 0.0, 0.0));
	float n010 = hash(i + vec3(0.0, 1.0, 0.0));
	float n110 = hash(i + vec3(1.0, 1.0, 0.0));
	float n001 = hash(i + vec3(0.0, 0.0, 1.0));
	float n101 = hash(i + vec3(1.0, 0.0, 1.0));
	float n011 = hash(i + vec3(0.0, 1.0, 1.0));
	float n111 = hash(i + vec3(1.0, 1.0, 1.0));
	float nx00 = mix(n000, n100, f.x);
	float nx10 = mix(n010, n110, f.x);
	float nx01 = mix(n001, n101, f.x);
	float nx11 = mix(n011, n111, f.x);
	float nxy0 = mix(nx00, nx10, f.y);
	float nxy1 = mix(nx01, nx11, f.y);
	return mix(nxy0, nxy1, f.z);
}

void fog() {
	vec3 p = WORLD_POSITION * 0.15;
	p += vec3(TIME * 0.08, TIME * 0.02, TIME * 0.05);
	float n = vnoise(p) + 0.5 * vnoise(p * 2.0 + 3.0);
	n /= 1.5;
	DENSITY = base_density * smoothstep(0.35, 0.9, n);
	ALBEDO = fog_albedo;
}
