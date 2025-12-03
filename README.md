# Vulkan Ray Tracer

A real-time ray tracing renderer built with Vulkan and the VK_KHR_ray_tracing_pipeline extension. Features hardware-accelerated ray tracing with support for dynamic scene updates, area lights, and soft shadows.

![Soft Shadows Demo](doc/shadow.gif)

## Features

### Ray Tracing
- **Hardware-accelerated ray tracing** using Vulkan's native ray tracing extensions
- **Dynamic scene updates** with efficient TLAS (Top-Level Acceleration Structure) rebuilds
- **Multiple geometry types**: spheres, boxes, pyramids, cylinders, cones, toruses, prisms, icosahedrons, and custom meshes

### Lighting & Shadows
- **Area lights** for realistic soft shadows
- **Grid-based stratified sampling** for high-quality shadow edges
- **Configurable shadow quality** with adjustable sample counts
- **Emissive materials** for light source visualization
- **Dynamic light animation** with automatic scene updates

### Rendering
- **Supersampling anti-aliasing (SSAA)** at 2x resolution
- **Custom shader pipeline** with closest-hit and miss shaders
- **Per-instance material system** with support for:
  - Diffuse materials
  - Emissive materials
  - Procedural patterns (checkerboard)
- **Real-time rendering** with efficient buffer management

## Technical Details

### Architecture
- **Acceleration Structures**:
  - BLAS (Bottom-Level AS) for each geometry type
  - TLAS with dynamic update support for animated objects
  - Efficient instance-based rendering

- **Shader Pipeline**:
  - Ray generation shader ([raygen.rgen](shaders/raygen.rgen))
  - Closest hit shader with soft shadow sampling ([closesthit.rchit](shaders/closesthit.rchit))
  - Miss shaders for primary rays and shadow rays

- **Memory Management**:
  - Custom buffer wrapper with device address support
  - Efficient GPU memory allocation
  - Per-frame uniform buffer updates

### Soft Shadow Implementation

The renderer uses **grid-based stratified sampling** on rectangular area lights:

1. **Area Light Definition**: Defined by center position + two perpendicular vectors (U, V)
2. **Stratified Grid**: Light surface divided into NxN grid cells (e.g., 4�4 = 16 samples)
3. **Jittered Sampling**: Random offset within each cell to reduce aliasing
4. **Multiple Shadow Rays**: One ray per grid cell to sample light visibility
5. **Averaging**: Shadow factor computed as average of all samples

This approach provides high-quality soft shadows with smooth penumbra regions while maintaining good performance.

## Building

### Prerequisites
- Vulkan SDK (1.3+)
- C++17 compatible compiler
- CMake 3.15+
- GPU with ray tracing support (NVIDIA RTX, AMD RDNA 2+, or Intel Arc)

### Build Instructions
```bash
mkdir build
cd build
cmake ..
cmake --build .
```