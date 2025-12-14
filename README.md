# Vulkan Ray Tracer

A real-time physically-based ray tracer built with Vulkan's ray tracing extensions. This project implements advanced rendering techniques including PBR materials, reflections, refractions, and soft shadows.


## Shadows
This renderer uses shadow rays to determine light visibility at intersection points, resulting in realistic soft shadows. Area lights are sampled to create smooth penumbra effects.


| Hard Shadows (Point Light) | Soft Shadows (Area Light) |
|----------------------------|---------------------------|
| ![Hard Shadows](doc/shadow_hard.png) | ![Soft Shadows](doc/shadow_soft.png) |


## Reflection
This renderer employs recursive ray tracing to achieve realistic reflections on surfaces. It accurately simulates how light interacts with reflective materials, including the effects of surface roughness.


<div align="center">
    <img src="doc/reflectence.png" alt="Reflection Diagram" width="600"/>
</div>


<div align="center">

| Reflection |
|------------|
| ![Reflection](doc/teapot_metalic.png) |

</div>


## Transparency & Refraction
Using recursive ray tracing, this renderer simulates realistic light behavior through transparent materials. Shadowing takes into account the transparency of objects.

<div align="center">
    <img src="doc/transparency.png" alt="Transparency Diagram" width="600"/>
</div>

### Key Features
- **Snell's Law**: Accurate light bending through transparent materials
- **Fresnel Reflectance**: Angle-dependent reflection and transmission
- **Beer-Lambert Absorption**: Realistic color attenuation in volumetric materials

| Tranparency | Absorption (Beer-Lambert) |
|-------------|---------------------------|
| ![Transparency](doc/teapot_glass.png) | ![Absorption](doc/teapot_beers.png) |


## Physically Based Rendering (PBR)
- **Cook-Torrance BRDF**: Industry-standard microfacet-based lighting model
- **Fresnel Effect**: Realistic reflection behavior at grazing angles
- **GGX Normal Distribution**: Advanced specular highlights
- **Smith Geometry Function**: Accurate light occlusion from surface roughness
- **Metallic/Roughness workflow**: Intuitive material authoring

| Diffuse | Metallic | Glass | Marble | Semi-Transparent |
|---------|----------|-------|--------|------------------|
| ![Diffuse](doc/pbr_plastic.png) | ![Metallic](doc/pbr_silver.png) | ![Glass](doc/pbr_glass.png) | ![Marble](doc/pbr_marble.png) | ![Semi-transparent](doc/pbr_gas.png)



