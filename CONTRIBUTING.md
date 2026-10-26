# Contributing

This is an early source-only plugin. Public standalone build/runtime validation is pending.

- Open an issue describing the engine patch, target, compiler/SDK, command and minimal reproduction. Remove private paths, credentials and personal logs before sharing diagnostics.
- Contribute only code/documentation you may publish under MIT. Do not submit Unreal Engine source, GASPALS source, `.uasset` files, animations, models, archives or other separately licensed content.
- Keep changes narrow. Preserve null/invalid-contract guards and local input ownership. Explain any change to runtime reflection contracts or module names.
- Run `scripts/Validate-Source.ps1`. For C++ changes, also report a real clean build and relevant host tests; static checks alone are not a build result.
- Update `docs/validation.md` only with reproducible results. Do not invent usage statistics, testimonials, deployments or support-program outcomes.

By submitting a contribution, you confirm that you have permission to provide it under the repository's MIT license. External engine/content licenses remain separate.
