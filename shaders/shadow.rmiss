#version 460
#extension GL_EXT_ray_tracing : enable

// Shadow ray payload: 1.0 = light visible, 0.0 = occluded
layout(location = 1) rayPayloadInEXT float shadowPayload;

void main()
{
    // Ray missed all geometry, so light is visible
    shadowPayload = 1.0;
}
