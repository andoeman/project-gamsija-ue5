# Validation record

This records the public extraction's evidence boundaries. Private-game results are not automatically portable to this source-only plugin.

| Area | Current status |
|---|---|
| Required source/descriptor/document layout | Static structural checks only |
| Module/API identity | `WatcherShooter` / `WATCHERSHOOTER_API`; runtime module entry point |
| Excluded licensed content, binaries and private conversation/log files | Source export inspection; no content is intentionally included |
| UE version | Local integration target 5.8.3; standalone extraction build pending |
| Private integration native Editor module | Compilation reported successful; this does not validate the separately extracted public plugin |
| Specific 14.x compiler/Windows SDK | Public plugin validation pending |
| BuildPlugin, clean host Editor build and packaged game | Not yet verified for this extraction |
| Standalone runtime health/sight behavior | Reproduction tests in a clean host are pending |
| Optional GASPALS combat integration | Draft contracts present; requires separately acquired assets and host wiring |
| Rendered animation, visual quality, physical input and complete combat play | Not verified for this public repository |
| Multiplayer, other platforms and other engine versions | Unsupported/unverified prototype scope |
| External OSS adoption, users, downloads and production deployment | No verified metrics currently available |

Source checks cannot prove C++ compilation, asset cooking, valid Blueprint contracts, correct damage behavior, animation quality or gameplay completeness. Re-run the checks for the exact public commit and record real results before changing these statuses.

For a support/grant application, describe this as a new source extraction under development. There is no substantiated popularity, broad adoption, established community or production usage claim. Eligibility and selection are determined by the program; no approval or six-month award is promised here.

## Suggested clean-host checks

- BuildPlugin and host Editor build with exact engine/toolchain versions recorded.
- Apply positive/invalid damage to an Actor with health; observe events and a single death transition.
- Verify sight blocked by geometry and loss of perception without replacing native range/FOV gates.
- Confirm missing montage/content returns a null/blocked result rather than assuming playback.
- Check optional combat's missing/duplicate action, contract, equipment, projectile and death paths.
- If external content is installed, separately test projectile damage, reload interruption, input lifecycle, cooked runtime paths, rendered animation and physical controls.
