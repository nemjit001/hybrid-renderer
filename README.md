# Hybrid Renderer

A project testing the combination of hybrid rendering (rasterization + ray tracing) and stochastic LOD sampling. The implementation provides a reference path tracer implemented using RTX raytracing, and a Hybrid Rendering pipeline that combines a standard deferred shading setup with raytraced direct illumination.

The stochastic LOD parameters can be adjusted in the UI, and temporal reprojection for sample accumulation can be enabled or disabled.

## Building the project

The project is based on CMake, all dependencies are included as git submodules. The project should build correctly out of the box using both MSVC and GCC.
