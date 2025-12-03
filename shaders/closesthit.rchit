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
    vec3 lightU; float pad2;
    vec3 lightV; float pad3;
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



float randomFloat(inout uint state) {
    state = state * 747796405u + 2891336453u;
    uint result = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    result = (result >> 22u) ^ result;
    return float(result) / 4294967295.0;
}


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

    // Check if this is an emissive material (light source)
    if (instanceData.materialType == 1) {
        // Return bright emissive color (no lighting calculation needed)
        hitValue = instanceData.color; // Bright emission
        return;
    }

    // Random number generator
    uint rngState = gl_LaunchIDEXT.x 
              ^ (gl_LaunchIDEXT.y << 8)
              ^ (gl_LaunchIDEXT.z << 16)
              ^ (gl_PrimitiveID * 9781u)
              ^ 0x68bc21ebu;

    // Grid-based stratified sampling for soft shadows
    const uint numSamples = 16; // 4x4 grid
    const uint gridSize = 4;    // sqrt(numSamples)
    float shadowSum = 0.0;
    float tmin = 0.001;

    float angle = randomFloat(rngState) * 6.28318530718; // random rotation
    float ca = cos(angle);
    float sa = sin(angle);

    for (uint i = 0; i < numSamples; i++) {
        // Calculate grid cell coordinates
        uint gridX = i % gridSize;
        uint gridY = i / gridSize;

        // Stratified sampling: divide light into grid, sample within each cell
        float cellSizeU = 1.0 / float(gridSize);
        float cellSizeV = 1.0 / float(gridSize);

        // Base position in grid cell
        float u = (float(gridX) + 0.5) * cellSizeU;
        float v = (float(gridY) + 0.5) * cellSizeV;

        // Optional: Add jitter within the cell for better quality
        u += (randomFloat(rngState) - 0.5) * cellSizeU;
        v += (randomFloat(rngState) - 0.5) * cellSizeV;

        // Convert to [-0.5, 0.5] range for centered sampling
        u = u - 0.5;
        v = v - 0.5;

        // u = clamp(u, -0.49, 0.49);
        // v = clamp(v, -0.49, 0.49);

        // float uu = ca * u - sa * v;
        // float vv = sa * u + ca * v;

        // Calculate point on area light
        vec3 lightSamplePos = scene.lightPosition + u * scene.lightU + v * scene.lightV;

        // Direction and distance to this light sample
        vec3 L = lightSamplePos - worldPosition;
        float lightDistance = length(L);
        L = normalize(L);

        if (dot(L, worldNormal) <= 0.0) {
            shadowSum += 1.0; // Light is behind → treat as fully lit for shadow only
            continue;
        }

        // Cast shadow ray
        shadowPayload = 0.0;
        traceRayEXT(
            TLAS,
            gl_RayFlagsTerminateOnFirstHitEXT |
            gl_RayFlagsSkipClosestHitShaderEXT,
            0xFF,
            0,
            1,
            1,
            worldPosition + worldNormal * 0.001,
            tmin,
            L,
            lightDistance,
            1
        );

        shadowSum += shadowPayload;
    }

    // Average shadow factor across all samples
    float shadowFactor = shadowSum / float(numSamples);

    // Calculate lighting to center of area light for diffuse/specular
    vec3 L = normalize(scene.lightPosition - worldPosition);
    vec3 V = normalize(scene.camPosition - worldPosition);
    vec3 R = reflect(-L, worldNormal);

    float diff = max(dot(worldNormal, L), 0.0);
    float spec = 0.3 * pow(max(dot(R, V), 0.0), 8.0);
    float amb = 0.1;

    vec3 baseColor = instanceData.color;

    if (instanceData.materialType == 999) {
        // Checkerboard pattern based on world position
        float scale = 1.0;
        float checker = mod(floor(worldPosition.x * scale) + floor(worldPosition.y * scale) + floor(worldPosition.z * scale), 2.0);
        baseColor = mix(vec3(0.5), vec3(1.0), checker);
    }

    // Apply soft shadows
    hitValue = (amb + shadowFactor * (diff + spec)) * baseColor;

    //hitValue = baseColor * shadowFactor;
}