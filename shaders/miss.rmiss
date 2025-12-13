#version 460
#extension GL_EXT_ray_tracing : enable

layout(location = 0) rayPayloadInEXT struct{
    vec3 color;
    int depth;
} hitValue;

void main()
{
    // 1. Get the direction of the ray. 
    // This is normalized automatically by the ray generation shader usually, 
    // but safe to assume it's a direction vector.
    vec3 direction = normalize(gl_WorldRayDirectionEXT);
    
    // 2. Map the Y component (-1.0 to 1.0) to a 0.0 to 1.0 range (t).
    // Using 0.5 * (y + 1.0) maps the full sphere from bottom to top.
    float t = 0.5 * (direction.y + 1.0);
    
    // 3. Define your colors
    vec3 gradientStart = vec3(1.0, 1.0, 1.0); // White (Horizon/Bottom)
    vec3 gradientEnd   = vec3(0.5, 0.7, 1.0); // Sky Blue (Top)
    
    // 4. Interpolate
    hitValue.color = mix(gradientStart, gradientEnd, t);
}