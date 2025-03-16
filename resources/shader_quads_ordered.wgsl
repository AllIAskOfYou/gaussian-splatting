/**
 * A structure for the uniforms
 */
struct Uniforms {
    projectionMatrix: mat4x4f,
    viewMatrix: mat4x4f,
    modelMatrix: mat4x4f,
	_pad1: vec3f,
	splatSize: f32,
};

struct Splat {
	transform: mat4x4f,
	color: vec4f,
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;
@group(0) @binding(1) var<storage, read> splats: array<Splat>;
@group(0) @binding(2) var<storage, read> sortedIndex: array<u32>;

/**
 * A structure with fields labeled with vertex attribute locations can be used
 * as input to the entry point of a shader.
 */

struct VertexInput {
    @location(0) position: vec2f,
    @location(1) uv: vec2f,
};

/**
 * A structure with fields labeled with builtins and locations can also be used
 * as *output* of the vertex shader, which is also the input of the fragment
 * shader.
 */
struct VertexOutput {
	@builtin(position) position: vec4f,
	// The location here does not refer to a vertex attribute, it just means
	// that this field must be handled by the rasterizer.
	// (It can also refer to another field of another struct that would be used
	// as input to the fragment shader.)
	@location(0) color: vec4f,
    @location(1) uv: vec2f,
};

fn var_to_trans(mat: mat2x2f) -> mat2x2f {
	var a = mat[0][0];
	var b = mat[1][0];
	var c = mat[0][1];
	var d = mat[1][1];
	var trace = a + d;
	var det = a * d - b * c;
	var sqrt_term = sqrt(max(0.0f, trace * trace - 4.0 * det));
	var lambda1 = (trace + sqrt_term) / 2.0;
	var lambda2 = (trace - sqrt_term) / 2.0;
	var v1: vec2f;
	var v2: vec2f;
	let err = 1e-6;
	if (abs(b) < err && abs(c) < err) {
		v1 = vec2f(1.0, 0.0);
		v2 = vec2f(0.0, 1.0);
	} else if (abs(b) < err) {
		v1 = vec2f(lambda1 - d, c);
		v2 = vec2f(lambda2 - d, c);
	} else {
		v1 = vec2f(b, lambda1 - a);
		v2 = vec2f(b, lambda2 - a);
	}
		
	v1 = normalize(v1);
	v2 = normalize(v2);

	if (v1.x < 0.0) {
		v1 = -v1;
	}
	if (v2.x < 0.0) {
		v2 = -v2;
	}

	var trans = mat2x2f(sqrt(lambda1) * v1, sqrt(lambda2) * v2);
	return trans;
}

@vertex
fn vs_main(@builtin(instance_index) instanceIndex: u32, vertex: VertexInput) -> VertexOutput {
	var s = uniforms.splatSize;
	var vm = uniforms.viewMatrix;
	var mm = uniforms.modelMatrix;
	var wt = vm * mm;

	var instance = splats[sortedIndex[instanceIndex]];
	var sm = instance.transform;
	var s_vm = wt * sm;
	var s_center = s_vm[3];
	var z = s_center.z;
	var d = 1.0 / max(1e-6, z);
	var s_vm_xyz = mat3x3f(s_vm[0].xyz, s_vm[1].xyz, s_vm[2].xyz);

	var s_var = s_vm_xyz * transpose(s_vm_xyz);
	var s_var_xy = mat2x2f(s_var[0].xy, s_var[1].xy);

	var s_var_proj = var_to_trans(s_var_xy);


	//var v_offset = vec4f(s_var_proj * vertex.position * s, 0.0, 0.0);		->		// Uncomment for non-uniform splats
	var v_offset = vec4f(vertex.position * s, 0.0, 0.0);							// Comment for non-uniform splats
	var v_pos = s_center + v_offset;

	var out: VertexOutput; // create the output struct
	out.position = uniforms.projectionMatrix * v_pos;
	out.color = instance.color; 
    out.uv = vertex.uv;
	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	var uv = in.uv * 2.0 - 1.0;
	uv = uv * 4.0;
	var dist = uv.x * uv.x + uv.y * uv.y;
	dist = sqrt(dist);
	var falloff = exp(- 0.5*dist * dist);
	var color = vec4f(in.color.xyz, in.color.w * falloff);
	//color = vec4f(in.color.xyz, 1);
	return color; // use the interpolated color coming from the vertex shader
}
