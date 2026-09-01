# WS-SPIRV: Emit Vulkan SPIR-V binaries internally

Status: Planned

## Goal

Remove the Vulkan backend's runtime/build dependency on shaderc by replacing
the textual SPIR-V assembly step with a deterministic internal binary emitter.

The OpenGL ES backend already sends generated GLSL ES source to the driver and
is outside this workstream.

## Intended architecture

```text
Current:
accelerator IR -> SPIR-V assembly text -> shaderc assembler -> SPIR-V words

Target:
accelerator IR -> internal SPIR-V binary emitter -> SPIR-V words
```

The Vulkan backend must continue to pass portable SPIR-V to
`vkCreateShaderModule()`. Direct GLSL submission through vendor-specific
Vulkan extensions is not an acceptable replacement.

## Scope

- Replace textual instruction formatting with 32-bit SPIR-V word emission.
- Encode module headers, instruction word counts, operands, IDs, and strings.
- Preserve deterministic section ordering and ID allocation.
- Preserve the existing Vulkan 1.2 / SPIR-V 1.5 contract.
- Extend validation sufficiently to catch malformed internally generated
  modules before they reach the Vulkan driver.
- Remove shaderc discovery, includes, initialization, linking, and diagnostics.
- Update Vulkan plan tests to consume the internal emitter directly.

## Milestones

1. Inventory every opcode, capability, decoration, and execution mode emitted
   by the current generator.
2. Introduce a bounded SPIR-V word-buffer builder and instruction helpers.
3. Port declarations, annotations, entry points, and function bodies in
   independently testable sections.
4. Compare disassembled old and new modules for representative kernels.
5. Remove shaderc from production code and CMake.
6. Run optional `spirv-val` verification in CI without making it a runtime
   dependency.

## Acceptance criteria

- Vulkan accelerator builds no longer require or link shaderc.
- Generated modules pass the internal validator and `spirv-val` in CI.
- Existing Vulkan plan and device tests pass without semantic changes.
- Strict floating-point capabilities and decorations remain unchanged.
- Invalid accelerator IR fails deterministically before pipeline creation.

## Risks and open decisions

- Decide whether to vendor SPIR-V opcode definitions or maintain a generated,
  version-pinned subset.
- Keep validation independent enough that the emitter cannot validate its own
  encoding mistake by repeating the same assumption.
- Bound all allocation and word-count arithmetic before removing shaderc's
  assembler checks.
