# Long-term workstreams

This directory records work that is intentionally deferred beyond the current
stable release. A workstream describes the intended architecture and exit
criteria; it does not imply that implementation has started.

| ID | Workstream | Status |
|----|------------|--------|
| WS-SIMD | [Separate SIMD auto-vectorization](ws-simd.md) | Planned |
| WS-SPIRV | [Emit Vulkan SPIR-V binaries internally](ws-spirv.md) | Planned |
| WS-POLY | [Full polyhedral model with bounded scheduling](ws-polyhedral.md) | Planned |

The workstreams are independent at the build-system level. WS-POLY may later
provide legality and profitability facts to ABCE, SIMD, scalar unrolling, and
accelerator planning, but those existing passes remain valid fallbacks.
