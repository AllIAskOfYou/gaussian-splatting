struct Uniforms {
    projectionMatrix: mat4x4f,
    viewMatrix: mat4x4f,
    modelMatrix: mat4x4f,
	splatSize: f32,
	cutOff: f32,
	fov: f32,
};

struct Splat {
	transform: mat4x4f,
	color: vec4f,
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;
@group(0) @binding(1) var<storage, read> splats: array<Splat>;
@group(0) @binding(2) var<storage, read> sortedIndex: array<u32>;

struct VertexOutput {
	@builtin(position) position: vec4f,
	@location(0) color: vec4f,
    @location(1) uv: vec2f,
};

fn eigenvectors(a: mat2x2<f32>) -> mat2x2<f32> {
    let a11 = a[0][0];
    let a22 = a[1][1];
    let a1221 = a[0][1] * a[1][0];
    let det = sqrt(a11*a11 + 4 * a1221 - 2 * a11*a22 + a22*a22);
    let lam1 = 0.5 * (a11 + a22 + det);
    let lam2 = 0.5 * (a11 + a22 - det);
    let phi = 0.5 * atan2(2 * a[0][1], a11 - a22);
    return mat2x2f(
        vec2f(cos(phi), sin(phi)) * sqrt(lam1),
        vec2f(-sin(phi), cos(phi)) * sqrt(lam2)
    );
}

@vertex
fn vs_main(@builtin(instance_index) instanceIndex: u32, @location(0) quadPosition: vec2f,) -> VertexOutput {
	var s = uniforms.splatSize;
	var cut_off = uniforms.cutOff;
	var vm = uniforms.viewMatrix;
	var mm = uniforms.modelMatrix;
	var wt = vm * mm;
	var wt_xyz = mat3x3f(wt[0].xyz, wt[1].xyz, wt[2].xyz);

	var instance = splats[sortedIndex[instanceIndex]];
	var sm = instance.transform;
	var s_center = wt * sm[3];
	var s_rm = mat3x3f(sm[0].xyz, sm[1].xyz, sm[2].xyz);

	var s_wt = wt_xyz * s_rm;
	var s_var_t = s_wt * transpose(s_wt);
	var s_var_t_xy = mat2x2f(s_var_t[0].xy, s_var_t[1].xy);

	var s_var_proj = eigenvectors(s_var_t_xy);

	var v_offset = vec4f(s_var_proj * quadPosition * cut_off * s, 0.0, 0.0);				// Uncomment for non-uniform splats
	//var v_offset = vec4f(vertex.position * s, 0.0, 0.0);							// Comment for non-uniform splats
	var v_pos = s_center + v_offset;

	var out: VertexOutput; // create the output struct
	out.position =  uniforms.projectionMatrix * v_pos;// + v_offset * v_pos.w;
	out.color = instance.color; 
    out.uv = quadPosition*cut_off;
	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	var uv = in.uv;
	var dist = uv.x * uv.x + uv.y * uv.y;
	var falloff = exp(- 0.5*dist);
	var color = vec4f(in.color.xyz, in.color.w * falloff);
	return color;
}
