# Hybrid Renderer

A project testing the combination of hybrid rendering (rasterization + ray tracing) and stochastic LOD sampling. The implementation provides a reference path tracer implemented using RTX raytracing, and a Hybrid Rendering pipeline that combines a standard deferred shading setup with raytraced direct illumination.

The stochastic LOD parameters can be adjusted in the UI, and temporal reprojection for sample accumulation can be enabled or disabled.

## Building the project

The project is based on CMake, all dependencies are included as git submodules. The project should build correctly out of the box using both MSVC and GCC.

The only depency not included is the Vulkan SDK (version 1.3.280).
Hardware raytracing MUST be supported on your device in order to run the demo.

![Example image rendered using path tracer](render_example.png?raw=true "Example render rendered using reference path tracer")
