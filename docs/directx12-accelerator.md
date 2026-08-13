# DirectX 12 accelerator backend

The DirectX 12 backend is a 64-bit Windows compute backend for Noct's
accelerator functions. It uses the same checked HIR/AST traversal as the GLSL
backends and stores both GLSL and HLSL in `.nb` and `.nap` bytecode. An
optimizer-enabled compiler can therefore prepare bytecode for a runtime that
was built without the optimizer.

## Build and run

```sh
./build.sh build windows-mingw-x86_64-dx12
build-mingw-x86_64-dx12/noct.exe --accel=dx12 -O2 program.noct
```

`--gpu-list` lists D3D12-capable DXGI adapters. `--gpu-name=NAME` selects the
exact UTF-8 adapter name shown by that command. The default policy excludes
software adapters and chooses the compatible adapter with the largest amount
of dedicated video memory. Set `NOCT_DX12_ALLOW_SOFTWARE=1` only for CI/WARP
testing. `NOCT_DX12_DEBUG=1` enables the D3D12 debug layer when it is installed.

## Implemented paths

- managed single- and multi-DOALL programs in one command submission;
- additive DOSUM map/fold programs, including zero-trip results;
- `_in`, `_out`, local buffers, and persistent `_ptr` resources;
- byte-range synchronous `Accel.copyToAccel()` and
  `Accel.copyFromAccel()`;
- raw `__gpu func`, shared memory, barriers, scalar parameters, and the
  currently registered GPU math operations;
- fence-backed raw dispatch and byte-range asynchronous copies through
  `Accel.dispatchAsync()`, `Accel.copyToAccelAsync()`,
  `Accel.copyFromAccelAsync()`, and `Accel.join()`;
- source, `.nb`, and `.nap` execution.

Pipeline state is cached per kernel and raw block size. Device-local buffers
use DEFAULT heaps; uploads and readbacks use staging resources. A managed
multi-step program records all dispatches, UAV barriers, and copies before one
queue submission and one fence wait.

## Tests

Compiler, serialization, and cross-build checks run on the normal development
host. HLSL syntax is also accepted by `glslc`'s HLSL frontend. Hardware
acceptance must run on Windows 10 or 11 with a D3D12-capable adapter:

```sh
NOCT=/path/to/noct.exe ./tests/test.sh accel-dx12
```

The hardware suite covers managed DOALL/DOSUM, persistent resources, raw
kernels, source, `.nb`, `.nap`, `-j0`, and `-j`, and rejects CPU fallback.
Cross-compiling the executable on Linux does not count as a hardware pass.

## Vendored headers

The build uses a minimal, unmodified subset of Microsoft DirectX-Headers
v1.616.0 under `third_party/directx-headers`. Its MIT license and file hashes
are recorded beside the snapshot. Runtime DLLs and import libraries are not
vendored; MinGW supplies the normal Windows import libraries.
