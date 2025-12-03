#version 460
#extension GL_EXT_ray_tracing : enable

layout(location = 0) rayPayloadInEXT vec3 hitValue;

void main()
{
    // light blue
    hitValue = vec3(0.53, 0.81, 0.92);
}