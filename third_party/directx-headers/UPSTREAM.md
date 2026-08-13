# DirectX-Headers snapshot

This directory contains the minimal Direct3D 12 C header dependency closure
used by Noct's DirectX 12 accelerator backend.

- Upstream: https://github.com/microsoft/DirectX-Headers
- Version: 1.616.0
- Imported: 2026-08-13
- Source snapshot: user-provided `dx12headers` checkout
- License: MIT; see `LICENSE`

Only the five headers reached by the MinGW C compilation probe were imported.
IDL files, C++ helpers, DirectML, WSL stubs, tests, samples, build files, and
AppleDouble `._*` metadata were deliberately excluded.

File SHA-256 values at import time:

```text
001db4a002b3ddb6a4e355646507e672bef3576290f1746c095b9034fb1c6342  include/directx/d3d12.h
81ee27d0ef90086145ae9ccdc8f4241448c745ffa27851999b9f882cad64c621  include/directx/d3d12sdklayers.h
8c236985415516302faf0cb725fa78312cb1ac05ba866e29eeab23d695a0f5cb  include/directx/d3dcommon.h
10b795959eef2771c5f4d20c49b20d56353cf9439e49667c391f32b9e74b09f6  include/directx/dxgicommon.h
0ac14d7da595c528e0f14c64685d83596c392d36a0ab49ce2ecab05299e6c0d0  include/directx/dxgiformat.h
```
