#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_buffer_reference2 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

// ------- Parameters ------- //

#define SOFT_SHADOWS
#define MAX_RECURSION_DEPTH 6


// ------- Structs ------- //

// Vertex structure matching C++ Vertex struct
struct Vertex {
    vec3 pos;    float pad0;
    vec3 normal; float pad1;
};

// Instance data structure matching C++ InstanceData struct
// Note: uint64_t addresses are split into uvec2 (low, high) for GLSL compatibility
struct InstanceData {
    uvec2 vertexBufferAddress;  // 64-bit address as uvec2
    uvec2 indexBufferAddress;   // 64-bit address as uvec2
    uint  materialType;
    vec3  color;
    float metallic;
    float roughness;
    float transparency;  // 0 = opaque, 1 = fully transparent
    float ior;           // Index of refraction
    vec3  absorbance;    // Used for semi-translucent objects (Beer-Lambert law)
    float pad;           
};

// Ray payload
struct RayPayload {
    vec3 color;
    uint depth;
};



// ------- Shader Resources ------- //

// Buffer reference types for device address access
layout(buffer_reference, scalar) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(buffer_reference, scalar) readonly buffer IndexBuffer {
    uint indices[];
};

// Uniform 0: Acceleration structure for shadow ray tracing
layout(binding = 0, set = 0) uniform accelerationStructureEXT TLAS;

// Uniform 1: Storage Image (we dont need that here)

// Uniform 2: Buffer for Scene data
layout(binding = 2, set = 0) uniform SceneUBO
{
	mat4 viewInverse;
	mat4 projInverse;
	vec3 camPosition;   float pad0;
	vec3 lightPosition; float pad1;
    vec3 lightU; float pad2;
    vec3 lightV; float pad3;
} scene;

// Uniform 3: Instance data buffer
layout(binding = 3, set = 0, scalar) readonly buffer InstanceDataBuffer {
    InstanceData instances[];
} instanceDataBuffer;


// ------- Ray Payloads ------- //

layout(location = 0) rayPayloadInEXT RayPayload incomingPayload;
layout(location = 1) rayPayloadEXT float shadowPayload;

hitAttributeEXT vec2 attribs;



// ------- Utility Functions ------- //

const float PI = 3.14159265;


// A helper to initialize the seed based on pixel position
uint initRandomSeed(uint val0, uint val1) {
    uint v0 = val0;
    uint v1 = val1;
    uint s0 = 0;

    // "TEA" (Tiny Encryption Algorithm) Iterations
    // This spreads the bits around so similar pixels have very different seeds
    for (uint n = 0; n < 16; n++) {
        s0 += 0x9e3779b9;
        v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
        v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
    }
    return v0;
}

// Linear Congruential Generator (LCG)
// Super fast, good enough for ray tracing noise
float rand(inout uint seed) {
    seed = seed * 1664525u + 1013904223u;
    return float(seed & 0x00FFFFFF) / float(0x01000000);
}


vec3 randomUnitVector(inout uint seed) {
    // 1. Pick a random height (z) and a random angle (a) around the pole
    float z = rand(seed) * 2.0 - 1.0; // Range [-1, 1]
    float a = rand(seed) * 2.0 * PI;  // Range [0, 2PI]
    
    // 2. Calculate the radius at this height (pythagoras)
    float r = sqrt(1.0 - z * z);
    float x = r * cos(a);
    float y = r * sin(a);
    
    return vec3(x, y, z);
}

// ------- PBR Helper Functions ------- //

// Fresnel Equation (F)
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Normal Distribution Function (D)
float distributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

// Geometry Function (G)
float geometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0; // Note: usage differs for IBL (k = a^2 / 2)

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = geometrySchlickGGX(NdotV, roughness);
    float ggx1 = geometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}




void main()
{
    // Initialize random seed based on pixel and primitive ID
    uint pixelIndex = gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x;
    uint seed = initRandomSeed(pixelIndex, 0u); // instead of 0u, could use a frame index in UBO

    // Get instance data using custom index
    const InstanceData instanceData = instanceDataBuffer.instances[gl_InstanceCustomIndexEXT];

    // Check if this is an emissive material (solid color - no shading)
    if (instanceData.materialType == 1) {
        incomingPayload.color = instanceData.color;
        return;
    }

    // Convert uvec2 addresses to uint64_t and create buffer references
    uint64_t vertexAddr = packUint2x32(instanceData.vertexBufferAddress);
    uint64_t indexAddr = packUint2x32(instanceData.indexBufferAddress);

    // Get Index/Vertex Buffers
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

    vec3 rayDir = normalize(gl_WorldRayDirectionEXT);
    float NdotR = dot(worldNormal, rayDir);


    // Shadow ray
    vec3 lightDir = scene.lightPosition - worldPosition;
    float lightDistance = length(lightDir);
    lightDir = normalize(lightDir);

    float shadowFactor;

#ifdef SOFT_SHADOWS
    // Random number generator
    uint rngState = gl_LaunchIDEXT.x
              ^ (gl_LaunchIDEXT.y << 8)
              ^ (gl_LaunchIDEXT.z << 16)
              ^ (gl_PrimitiveID * 9781u)
              ^ 0x68bc21ebu;

    // Grid-based stratified sampling for soft shadows
    const uint numSamples = 16; // 8x8 grid
    const uint gridSize = 4;    // sqrt(numSamples)
    float shadowSum = 0.0;
    float tmin = 0.001;

    float angle = rand(seed) * 6.28318530718; // random rotation
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

        // Add jitter within the cell for better quality
        u += (rand(seed) - 0.5) * cellSizeU;
        v += (rand(seed) - 0.5) * cellSizeV;

        // Convert to [-0.5, 0.5] range for centered sampling
        u = u - 0.5;
        v = v - 0.5;
        u = clamp(u, -0.49, 0.49);
        v = clamp(v, -0.49, 0.49);

        // Calculate point on area light
        vec3 lightSamplePos = scene.lightPosition
                            + (u * scene.lightU)
                            + (v * scene.lightV);

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
            gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT,
            0xFF,
            0,
            1,
            1,                               // Miss Index (Use miss shader index 1 for shadows)
            worldPosition + worldNormal * 0.001,
            tmin,
            L,
            lightDistance,
            1                                // shadow ray payload is located at layout(location=1)
        );

        shadowSum += shadowPayload;
    }

    // Average shadow factor across all samples
    shadowFactor = shadowSum / float(numSamples);
#else
    // Simple hard shadow - single ray cast
    // Cast shadow ray toward light
    shadowPayload = 0.0;
    traceRayEXT(
        TLAS,
        gl_RayFlagsTerminateOnFirstHitEXT| gl_RayFlagsSkipClosestHitShaderEXT,
        0xFF,
        0,
        1,
        1,                                    // Miss Index (Use miss shader index 1 for shadows)
        worldPosition + worldNormal * 0.001,
        0.001,
        lightDir,
        lightDistance,
        1                                     // shadow ray payload is located at layout(location=1)
    );
    shadowFactor = shadowPayload;
#endif

    // ------------------------
    // Direct Lighting & Base Color
    // ------------------------

    // Base color
    vec3 baseColor = instanceData.color;
    if (instanceData.materialType == 999) {
        // Checkerboard pattern
        float scale = 1.0;
        float checker = mod(floor(worldPosition.x * scale) + floor(worldPosition.y * scale) + floor(worldPosition.z * scale), 2.0);
        baseColor = mix(vec3(1.0), vec3(0.5), checker);
    }

    vec3 lightColor = vec3(1.0);
    vec3 viewDir = normalize(scene.camPosition - worldPosition);
    vec3 halfVec = normalize(lightDir + viewDir);
    float NdotL = max(dot(worldNormal, lightDir), 0.0);
    float NdotV = max(dot(worldNormal, viewDir), 0.0);
    float NdotH = max(dot(worldNormal, halfVec), 0.0);
    float HdotV = max(dot(halfVec, viewDir), 0.0);

    // Base reflectivity (F0)
    // 0.04 is a standard value for dielectrics (plastic, wood, etc.)
    // For metals, the F0 is the albedo color itself.
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, baseColor, instanceData.metallic);

    // Cooking-Torrance BRDF components

    // 1. Normal Distribution Function (D)
    float D = distributionGGX(worldNormal, halfVec, instanceData.roughness);

    // 2. Geometry Function (G)
    float G = geometrySmith(worldNormal, viewDir, lightDir, instanceData.roughness);

    // 3. Fresnel (F)
    vec3 F = fresnelSchlick(HdotV, F0);
    

    vec3 numerator = D * G * F;
    float denominator = 4.0 * NdotV * NdotL + 0.001; // prevent divide by zero
    vec3 specular = numerator / denominator;


    // Energy conversion

    // kS is the ratio of light that gets reflected (specular)
    //vec3 kS = F;

    // kD is the ratio of light that gets refracted (diffuse)
    // Because metals absorb all refracted light, kD is 0.0 for pure metals.
    vec3 kD = vec3(1.0) - F;
    kD *= 1.0 - instanceData.metallic;


    // Direct lighting is the light coming directly from the light source
    vec3 directLighting = (kD * baseColor / PI + specular) * lightColor * NdotL * shadowFactor;


    // Terminate recursion if max depth reached
    if (incomingPayload.depth > MAX_RECURSION_DEPTH) {
        incomingPayload.color = directLighting;
        return;
    }
    
    float NdotI = dot(worldNormal, rayDir);
    float ior = instanceData.ior;
    bool isEntering = NdotI < 0.0;
    vec3 N = isEntering ? worldNormal : -worldNormal;
    // If entering: Normal is fine. Ratio is 1.0/IOR
    // If exiting: Flip normal. Ratio is IOR/1.0
    float eta = isEntering ? (1.0 / ior) : (ior / 1.0);
    float refrCosTheta = clamp(abs(NdotI), 0.0, 1.0);

    // ------------------------
    // Reflection
    // ------------------------

    vec3 reflectedDir = reflect(rayDir, N);
    vec3 reflectColor = vec3(0.0);

    // Add roughness-based jitter to reflection direction
    // vec3 jitter = randomUnitVector(seed) * instanceData.roughness * 0.05; 
    // vec3 jitteredReflect = normalize(reflectedDir + jitter);

    // if (dot(jitteredReflect, worldNormal) <= 0.0) {
    //     //Just use the pure reflection (Safer)
    //     jitteredReflect = reflectedDir; 
    // }


    incomingPayload.color = vec3(0.0);
    incomingPayload.depth += 1;

    traceRayEXT(
        TLAS,
        gl_RayFlagsOpaqueEXT,  // Skip any hit shader for regular rays
        0xFF,
        0,
        1,
        0,
        worldPosition + N * 0.001,
        0.001,
        reflectedDir,
        10000.0,
        0
    );

    reflectColor = incomingPayload.color;
    incomingPayload.depth -= 1;
    
    


    vec3 Fenv = fresnelSchlick(refrCosTheta, F0);



    // "Blur" it by just darkening it based on roughness
    // (This is physically wrong, but looks cleaner than noise)
    // noise is better if you have a history buffer to accumulate over frames
    // reflectColor *= (1.0 - instanceData.roughness);
    // float specularOcclusion = mix(0.3, 1.0, shadowFactor);
    // reflectColor *= specularOcclusion;

    // ------------------------
    // Diffuse Ambient
    // ------------------------

    vec3 ambientUp = vec3(0.1, 0.1, 0.1) * 0.25; // Sky color
    vec3 ambientDown = vec3(0.1, 0.1, 0.1) * 0.15; // Ground color

    float hemiMix = smoothstep(-1.0, 1.0, worldNormal.y);
    vec3 ambientLight = mix(ambientDown, ambientUp, hemiMix);

    vec3 diffuseAmbient = vec3(0.0);
    if (incomingPayload.depth == 0) {
        // Only add ambient at the primary ray level to avoid over-brightening
        diffuseAmbient = kD * (baseColor * ambientLight);
    }


    // ------------------------
    // Refraction and transparency
    // ------------------------

    vec3 transmissionColor = vec3(0.0);
    float transmission = instanceData.transparency;

    vec3 kS = fresnelSchlick(refrCosTheta, F0); // Reflection weight
    vec3 kT = (vec3(1.0) - kS) * transmission;  // Transmission weight


    if (transmission > 0.0) {

        // Snell's Law
        vec3 refractDir = refract(rayDir, N, eta);

        // Handle Total Internal Reflection (TIR)
        // If the angle is too shallow (like looking up from underwater), 
        // light cannot escape. refract() returns vec3(0.0) in this case.
        bool isTIR = length(refractDir) == 0.0;

        if (!isTIR) {
            incomingPayload.color = vec3(0.0);
            incomingPayload.depth += 1;

            traceRayEXT(
                TLAS,
                gl_RayFlagsOpaqueEXT, // Refraction rays should not ignore transparent hits
                0xFF,
                0,
                1,
                0,
                worldPosition + refractDir * 0.005, // Offset slightly inside the surface
                0.001,
                normalize(refractDir),
                10000.0,
                0
            );

            transmissionColor = incomingPayload.color;
            incomingPayload.depth -= 1;

            // Beer's Law for attenuation (semi-translucent materials)
            if (!isEntering) {
                float distance = gl_HitTEXT; // Scale distance for effect
                vec3 transmissionCoeff = exp(-instanceData.absorbance * distance);
                transmissionColor *= transmissionCoeff;
            }
        }
        else {
            // If TIR happens, 100% of the light reflects.
            // We handle this by setting the "reflection weight" to 1.0 later.
            kS = vec3(1.0);
            kT = vec3(0.0);
        }


    }


    // ------------------------ //
    // Combining everything
    // ------------------------ //

    vec3 directOpaque = directLighting;                                 // Diffuse + specular
    vec3 directGlass =  specular * lightColor * NdotL; //* shadowFactor; // Just specular

    vec3 indirectOpaque = (reflectColor * kS) + diffuseAmbient;            // Reflection + ambient
    vec3 indirectGlass = (transmissionColor * kT) + (reflectColor * kS);   // Reflection + refraction

    vec3 opaquePart = directLighting + reflectColor + diffuseAmbient;
    vec3 glassPart = reflectColor + transmissionColor;

    vec3 finalOpaque = directOpaque + indirectOpaque;
    vec3 finalGlass = directGlass + indirectGlass;

    incomingPayload.color = mix(finalOpaque, finalGlass, transmission);

}