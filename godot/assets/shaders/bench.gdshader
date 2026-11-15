shader_type canvas_item;
// Lab bench backdrop: near-black navy, a faint graph-paper grid and a soft vignette. Resolution-independent (grid in logical px).
uniform vec4 base_color : source_color = vec4(0.027, 0.043, 0.075, 1.0);
uniform vec4 grid_color : source_color = vec4(0.12, 0.2, 0.3, 1.0);
uniform vec2 area = vec2(1920.0, 1080.0);
uniform float cell = 48.0;
void fragment() {
	vec2 p = UV * area;
	vec2 g = abs(fract(p / cell - 0.5) - 0.5) * cell;
	float line = 1.0 - smoothstep(0.0, 1.2, min(g.x, g.y));
	float major = 1.0 - smoothstep(0.0, 1.4, min(abs(fract(p.x / (cell * 4.0) - 0.5) - 0.5) * cell * 4.0, abs(fract(p.y / (cell * 4.0) - 0.5) - 0.5) * cell * 4.0));
	vec3 col = base_color.rgb + grid_color.rgb * (line * 0.18 + major * 0.22);
	float v = smoothstep(1.1, 0.25, length((UV - 0.5) * vec2(1.25, 1.0)));
	COLOR = vec4(col * mix(0.55, 1.0, v), 1.0);
}
