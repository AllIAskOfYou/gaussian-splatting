struct Uniforms {
    projectionMatrix: mat4x4f,
    viewMatrix: mat4x4f,
    modelMatrix: mat4x4f,
	splatSize: f32,
	fov: f32,
};

struct Splat {
	transform: mat4x4f,
	color: vec4f,
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;
@group(0) @binding(1) var<storage, read> splats: array<Splat>;
@group(0) @binding(2) var<storage, read> sortedIndex: array<u32>;

struct VertexInput {
    @location(0) position: vec2f,
    @location(1) uv: vec2f,
};

struct VertexOutput {
	@builtin(position) position: vec4f,
	// The location here does not refer to a vertex attribute, it just means
	// that this field must be handled by the rasterizer.
	// (It can also refer to another field of another struct that would be used
	// as input to the fragment shader.)
	@location(0) color: vec4f,
    @location(1) uv: vec2f,
	@location(2) det: f32,
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
fn vs_main(@builtin(instance_index) instanceIndex: u32, vertex: VertexInput) -> VertexOutput {
	var s = uniforms.splatSize;
	var vm = uniforms.viewMatrix;
	var mm = uniforms.modelMatrix;
	var wt = vm * mm;
	var wt_xyz = mat3x3f(wt[0].xyz, wt[1].xyz, wt[2].xyz);

	var instance = splats[sortedIndex[instanceIndex]];
	var sm = instance.transform;
	var s_center = wt * sm[3];
	var s_rm = mat3x3f(sm[0].xyz, sm[1].xyz, sm[2].xyz);
	//s_rm[0][1] *= s;
	//s_rm[1][1] *= s;
	//s_rm[2][1] *= s;

	/// Jacobi:
	/*
	let l = length(s_center.xyz);
    let f = tan(uniforms.fov / 2);
	let asp = 1.0;
    let tz2 = s_center.z*s_center.z;
    let jacobi = mat3x3f(
        1.0 / s_center.z, 0.0, s_center.x / l,
        0.0, 1.0 / s_center.z, s_center.y / l,
        s_center.x / tz2, s_center.y / tz2, s_center.z / l
    );
	*/

	var s_wt = wt_xyz * s_rm;
	var s_var_t = s_wt * transpose(s_wt);
	var s_var_t_xy = mat2x2f(s_var_t[0].xy, s_var_t[1].xy);

	//var s_var_proj = var_to_trans(s_var_t_xy);
	var s_var_proj = eigenvectors(s_var_t_xy);

	var det = s_var_t_xy[0][0] * s_var_t_xy[1][1] - s_var_t_xy[0][1] * s_var_t_xy[1][0];

	var v_offset = vec4f(s_var_proj * vertex.position * s, 0.0, 0.0);				// Uncomment for non-uniform splats
	//var v_offset = vec4f(vertex.position * s, 0.0, 0.0);							// Comment for non-uniform splats
	var v_pos = s_center + v_offset;

	var out: VertexOutput; // create the output struct
	out.position =  uniforms.projectionMatrix * v_pos;// + v_offset * v_pos.w;
	out.color = instance.color; 
    out.uv = vertex.uv;
	out.det = det;
	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	var uv = in.uv * 2.0 - 1.0;
	uv = uv * 4.0;
	var dist = uv.x * uv.x + uv.y * uv.y;
	let pi = 3.14159265359;
	var det = in.det;
	var falloff = exp(- 0.5*dist);// * (1/(2*pi * sqrt(abs(det)))) *  * 0.001;
	var color = vec4f(in.color.xyz, in.color.w * falloff);
	//color = vec4f(in.color.xyz, 1);
	return color; // use the interpolated color coming from the vertex shader
}
