#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

// Vertex structure matching C++ Vertex struct
struct Vertex {
    vec3 pos;  float pad0;
    vec3 normal; float pad1;
};

layout(location = 0) rayPayloadInEXT vec3 hitValue;
hitAttributeEXT vec2 attribs;

// Storage buffers for vertex and index data
layout(binding = 3, set = 0, scalar) readonly buffer VertexBuffer {
    Vertex vertices[];
} vertexBuffer;

layout(binding = 4, set = 0, scalar) readonly buffer IndexBuffer {
    uint indices[];
} indexBuffer;

void main()
{
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

    // Simple shading: use normal as color (for debugging)
    hitValue = normal * 0.5 + 0.5; // Remap from [-1,1] to [0,1]
}