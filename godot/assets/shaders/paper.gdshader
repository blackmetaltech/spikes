shader_type canvas_item;
// "Cajal Plate" page: aged rag paper. A flat paper tone, long horizontal fibres (anisotropic value noise), a fine tooth of grain and a warm,
// barely-there foxing toward the corners. Everything is in logical px (`area`), so the texture keeps its scale at any resolution and layout.
uniform vec4 paper : source_color = vec4(0.937, 0.902, 0.831, 1.0);   // #EFE6D4
uniform vec4 fibre : source_color = vec4(0.55, 0.45, 0.30, 1.0);
uniform vec4 foxing : source_color = vec4(0.353, 0.247, 0.110, 1.0);
uniform vec2 area = vec2(1920.0, 1080.0);
uniform float seed = 9.0;

float hash(vec2 p) { p = fract(p * vec2(123.34, 456.21) + seed); p += dot(p, p + 45.32); return fract(p.x * p.y); }
float noise(vec2 p) {
	vec2 i = floor(p); vec2 f = fract(p); vec2 u = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), u.x), mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), u.x), u.y);
}
float fbm(vec2 p) {
	float s = 0.0; float a = 0.5;
	for (int k = 0; k < 4; k++) { s += a * noise(p); p = p * 2.03 + vec2(17.1, 3.7); a *= 0.5; }
	return s;
}
void fragment() {
	vec2 p = UV * area;
	float fib = fbm(p * vec2(0.010, 0.048));                 // long, soft horizontal fibres
	float tooth = noise(p * 0.9) * 0.6 + noise(p * 0.37) * 0.4;  // fine grain
	vec3 col = paper.rgb;
	col = mix(col, fibre.rgb, (fib - 0.45) * 0.13);
	col -= (tooth - 0.5) * 0.022;
	vec2 q = (UV - vec2(0.5, 0.48)) * vec2(1.0, area.y / max(area.x, 1.0)) * 1.6;
	float vig = smoothstep(0.45, 1.05, length(q));
	col = mix(col, foxing.rgb, vig * 0.16);
	COLOR = vec4(col, 1.0);
}
