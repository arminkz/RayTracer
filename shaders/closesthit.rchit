#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_buffer_reference2 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

// Vertex structure matching C++ Vertex struct
struct Vertex {
    vec3 pos;  float pad0;
    vec3 normal; float pad1;
};

// Buffer reference types for device address access
layout(buffer_reference, scalar) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(buffer_reference, scalar) readonly buffer IndexBuffer {
    uint indices[];
};

// Instance data structure matching C++ InstanceData struct
// Note: uint64_t addresses are split into uvec2 (low, high) for GLSL compatibility
struct InstanceData {
    uvec2 vertexBufferAddress;  // 64-bit address as uvec2
    uvec2 indexBufferAddress;   // 64-bit address as uvec2
    uint materialType;
    vec3 color;
    float metallic;
    float roughness;
    float _pad0;
    float _pad1;
};

// Uniform Buffer for Scene data
layout(binding = 2, set = 0) uniform SceneUBO
{
	mat4 viewInverse;
	mat4 projInverse;
	vec3 camPosition; float pad0;
	vec3 lightPosition; float pad1;
} scene;

// Instance data buffer
layout(binding = 3, set = 0, scalar) readonly buffer InstanceDataBuffer {
    InstanceData instances[];
} instanceDataBuffer;


layout(location = 0) rayPayloadInEXT vec3 hitValue;
layout(location = 1) rayPayloadEXT float shadowPayload;
hitAttributeEXT vec2 attribs;

// Acceleration structure for shadow ray tracing
layout(binding = 0, set = 0) uniform accelerationStructureEXT TLAS;


void main()
{
    // Get instance data using custom index
    const InstanceData instanceData = instanceDataBuffer.instances[gl_InstanceCustomIndexEXT];

    // Convert uvec2 addresses to uint64_t and create buffer references
    uint64_t vertexAddr = packUint2x32(instanceData.vertexBufferAddress);
    uint64_t indexAddr = packUint2x32(instanceData.indexBufferAddress);

    VertexBuffer vertexBuffer = VertexBuffer(vertexAddr);
    IndexBuffer indexBuffer = IndexBuffer(indexAddr);

    // Calculate barycentric coordinates
    const vec3 barycentricCoords = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);

    // Get triangle indices
    const uint primitiveID = gl_PrimitiveID;
    const uint i0 = indexBuffer.indices[primitiveID * 3 + 0];
    const uint i1 = indexBuffer.indices[primitiveID * 3 + 1];
    const uint i2 = indexBuffer.indices[primitiveID * 3 + 2];

    // Get vertices
    const Vertex v0 = vertexBuffer.vertices[i0];
    const Vertex v1 = vertexBuffer.vertices[i1];
    const Vertex v2 = vertexBuffer.vertices[i2];

    // Interpolate position using barycentric coordinates
    const vec3 position = v0.pos * barycentricCoords.x +
                          v1.pos * barycentricCoords.y +
                          v2.pos * barycentricCoords.z;

    // Interpolate normal using barycentric coordinates
    const vec3 normal = normalize(v0.normal * barycentricCoords.x +
                                  v1.normal * barycentricCoords.y +
                                  v2.normal * barycentricCoords.z);

    // Transform position and normal to world space
    vec3 worldPosition = vec3(gl_ObjectToWorldEXT * vec4(position, 1.0));
    vec3 worldNormal = normalize(mat3(gl_ObjectToWorldEXT) * normal);

    vec3 L = normalize(scene.lightPosition - worldPosition); // Light direction
    vec3 V = normalize(scene.camPosition - worldPosition);   // View direction
    vec3 R = reflect(-L, worldNormal);                       // Reflection direction

    // Cast shadow ray toward the light
    float tmin = 0.005; // Small offset to avoid self-intersection
    float tmax = length(scene.lightPosition - worldPosition); // Distance to light
    shadowPayload = 0.0; // Assume occluded

    traceRayEXT(
        TLAS,                                     // Acceleration structure
        gl_RayFlagsOpaqueEXT |
        gl_RayFlagsTerminateOnFirstHitEXT |       // Stop at first hit (we only care if blocked)
        gl_RayFlagsSkipClosestHitShaderEXT,       // Don't need closest hit for shadows
        0xFF,                                     // Cull mask
        0,                                        // SBT record offset
        1,                                        // SBT record stride
        1,                                        // Miss shader index (shadow miss shader)
        worldPosition + worldNormal * 0.005 ,      // Ray origin (slightly offset along normal)
        tmin,                                     // Min distance
        L,                                        // Ray direction (toward light)
        tmax,                                     // Max distance
        1                                         // Payload location
    );

    // Apply lighting: ambient always present, diffuse/specular only if not in shadow
    float diff = 0.65 * max(dot(worldNormal, L), 0.0);
    float spec = 0.3 * pow(max(dot(R, V), 0.0), 8.0);
    float amb = 0.05;

    vec3 baseColor = instanceData.color;

    if (instanceData.materialType == 999) {
        // Checkerboard pattern based on world position
        float scale = 1.0;
        float checker = mod(floor(worldPosition.x * scale) + floor(worldPosition.y * scale) + floor(worldPosition.z * scale), 2.0);
        baseColor = mix(vec3(0.5), vec3(1.0), checker);
    }

    // shadowPayload is 1.0 if light is visible, 0.0 if occluded
    // Final shading with instance color
    hitValue = (amb + shadowPayload * (diff + spec)) * baseColor;
}