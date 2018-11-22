
## v2.3.c (in progress)

* Implemented explicit resource state transitions
* API Changes
  * Added `DRAW_FLAGS` and `DISPATCH_FLAGS` enums that control state transitions of vertex,
    index and indirect draw arguments buffers
  * Added `Uint8 Flags` member to `DrawAttribs` structure (values from `DRAW_FLAGS`)
  * Added `Uint8 Flags` member to `DispatchComputeAttribs` structure (values from `DISPATCH_FLAGS`)
  * Added `RESOURCE_STATE` enum
  * Added `StateTransitionDesc` structure that describes resource state transition
  * Added `IDeviceContext::TransitionResourceStates(Uint32 BarrierCount, StateTransitionDesc* pResourceBarriers)` method
  * Added `IBuffer::SetState()`, `IBuffer::GetState()`, `ITexture::SetState()`, `ITexture::GetState()` methods

## v2.3.b

* Enabled Vulkan backend on Linux
* API Changes
  * Implemented separate texture samplers: 
    * Added `UseCombinedTextureSamplers` and `CombinedSamplerSuffix` members to `ShaderCreationAttribs` structure
    * When separate samplers are used (`UseCombinedTextureSamplers == false`), samplers are set in the same way as other shader variables
      via shader or SRB objects
  * Removed `BIND_SHADER_RESOURCES_RESET_BINDINGS` flag, renamed `BIND_SHADER_RESOURCES_KEEP_EXISTING` to `BIND_SHADER_RESOURCES_KEEP_EXISTING`.
	Added `BIND_SHADER_RESOURCES_UPDATE_STATIC`, `BIND_SHADER_RESOURCES_UPDATE_MUTABLE`, `BIND_SHADER_RESOURCES_UPDATE_DYNAMIC`, and
	`BIND_SHADER_RESOURCES_UPDATE_ALL` flags
* Using glslang to compile HLSL to SPIRV in Vulkan backend instead of relying on HLSL->GLSL converter

## v2.3.a

* API Changes:
  * Added `IFence` interface and `IDeviceContext::SignalFence()` method to enable CPU-GPU synchronization
  * Added `GetType`, `GetArraySize`, `GetName`, and `GetIndex` methods to `IShaderVariable` interface;
    Added `GetVariableCount` and `GetShaderVariable(Uint32 Index)` methods to `IShader` interface; 
    Added `GetVariableCount` and `GetVariable(SHADER_TYPE ShaderType, Uint32 Index)` to `IShaderResourceBinding` interface.
  * Added `BUFFER_MODE_RAW` mode allowing raw buffer views in D3D11/D3D12.
  * Moved `Format` member from `BufferDesc` to `BufferViewDesc`
  * Removed `IsIndirect` member from `DrawAttrbis` as setting `pIndirectDrawAttribs` to a non-null buffer already indicates indirect rendering

## v2.3

* Implemented Vulkan backend
* Implemented hardware adapter & display mode enumeration in D3D11 and D3D12 modes
* Implemented initialization in fullscreen mode as well as toggling between fullscreen and windowed modes in run time
* Added sync interval parameter to ISwapChain::Present()
* Fixed issues with relative paths in headers
* API Changes:
  * Math library functions `SetNearFarClipPlanes()`, `GetNearFarPlaneFromProjMatrix()`, `Projection()`,
    `OrthoOffCenter()`, and `Ortho()` take `bIsGL` flag instead of `bIsDirectX`
  * Vertex buffer strides are now defined by the pipeline state as part of the input layout description (`LayoutElement::Stride`)
  * Added `COMMIT_SHADER_RESOURCES_FLAG_VERIFY_STATES` flag
  * Added `NumViewports` member to `GraphicsPipelineDesc` structure

## v2.2.a

### New Features

* **Implemented Vulkan backend**
* Implemented PSO compatibility: if two pipeline states share the same shader resource layout,
  they can use SRB objects interchangeably.
* Added `IPipelineState::IsCompatibleWith(const IPipelineState *pPSO)` method that returns true if two 
  pipeline states are compatible.
* Added sync interval parameter to ISwapChain::Present()
* API Changes
  * Added `NumViewports` member to `GraphicsPipelineDesc` struct
  * Removed `PRIMITIVE_TOPOLOGY_TYPE` type
  * Replaced `PRIMITIVE_TOPOLOGY_TYPE GraphicsPipelineDesc::PrimitiveTopologyType` 
    with `PRIMITIVE_TOPOLOGY GraphicsPipelineDesc::PrimitiveTopology`
  * Removed `DrawAttribs::Topology`
  * Removed `pStrides` parameter from `IDeviceContext::SetVertexBuffers()`. Strides are now defined
    through vertex layout.

## v2.2

* Added MacOS and iOS support
* Improved GLSL2HLSL converter to fix multiple issues on GLES

## v2.1.b

* Removed legacy Visual Studio solution and project files
* Added API reference

## v2.1.a

* Refactored build system to use CMake
* Added support for Linux platform

## v2.1

### New Features

#### Core

* Interoperability with native API
  * Accessing internal objects and handles
  * Createing diligent engine buffers/textures from native resources
  * Attaching to existing D3D11/D3D12 device or GL context
  * Resource state and command queue synchronization for D3D12
* Integraion with Unity
* Geometry shader support
* Tessellation support
* Performance optimizations

#### HLSL->GLSL converter
* Support for structured buffers
* HLSL->GLSL conversion is now a two-stage process:
  * Creating conversion stream
  * Creating GLSL source from the stream
* Geometry shader support
* Tessellation control and tessellation evaluation shader support
* Support for non-void shader functions
* Allowing structs as input parameters for shader functions


## v2.0 (alpha)

Alpha release of Diligent Engine 2.0. The engine has been updated to take advantages of Direct3D12:

* Pipeline State Object encompasses all coarse-grain state objects like Depth-Stencil State, Blend State, Rasterizer State, shader states etc.
* New shader resource binding model implemented to leverage Direct3D12

* OpenGL and Direct3D11 backends
* Alpha release is only available on Windows platform
* Direct3D11 backend is very thoroughly optimized and has very low overhead compared to native D3D11 implementation
* Direct3D12 implementation is preliminary and not yet optimized

### v1.0.0

Initial release