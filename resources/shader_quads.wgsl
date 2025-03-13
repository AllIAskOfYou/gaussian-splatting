/**
 * A structure for the uniforms
 */
struct TransfromMatrix {
    projectionMatrix: mat4x4f,
    viewMatrix: mat4x4f,
    modelMatrix: mat4x4f,
};

@group(0) @binding(0) var<uniform> transform: TransfromMatrix;

/**
 * A structure with fields labeled with vertex attribute locations can be used
 * as input to the entry point of a shader.
 */
struct InstanceInput {
	@location(0) position: vec3f,
	@location(1) size: vec3f,
	@location(2) color: vec4f,
	@location(3) rotation: vec4f,
};

struct VertexInput {
    @location(4) position: vec2f,
    @location(5) uv: vec2f,
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

@vertex
fn vs_main(instance: InstanceInput, vertex: VertexInput) -> VertexOutput {
	//                         ^^^^^^^^^^^^ We return a custom struct
	var out: VertexOutput; // create the output struct
	let ratio = 640.0 / 480.0; // The width and height of the target surface
    let scale = 0.1;
	//let offset = vec2f(-0.6875, -0.463); // The offset that we want to apply to the position
	//out.position = vec4f(in.position.x + offset.x, (in.position.y + offset.y) * ratio, 0.0, 1.0);
	//out.position = vec4f(in.position.x/2, in.position.y * ratio/2 - 1, 0, 1.0);
    var center = transform.viewMatrix * 
					transform.modelMatrix * 
					vec4f(instance.position, 1.0);
    var z = center.z;
    center = transform.projectionMatrix * center;
    out.position = center + scale * transform.projectionMatrix *  vec4f(vertex.position, 0.0, 0.0) / z;
	//out.position.y = ratio * out.position.y;
	out.color = instance.color; // forward the color attribute to the fragment shader
    out.uv = vertex.uv;
	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	//     ^^^^^^^^^^^^^^^^ Use for instance the same struct as what the vertex outputs
	return vec4f(in.color.xyz, 1); // use the interpolated color coming from the vertex shader
}
