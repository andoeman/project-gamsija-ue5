# Third-party notices and scope

The MIT license covers the project-owned code and documentation included in this repository. It does not license external products or assets.

| External dependency/reference | Treatment |
|---|---|
| Unreal Engine and built-in modules/plugins | Required separately. No engine headers, source implementation or binaries are redistributed here. Engine licensing governs their use. |
| GASPALS 1.2 | Optional runtime integration reference. No vendor plugin, Blueprint implementation, content, documentation copy or license grant is bundled. Obtain it separately and follow its terms. |
| Epic Game Animation Sample, ALS, MetaHuman or other game content | No assets or source content from these packages are included. Any host integration must obtain and observe its own applicable terms. |
| Project montage/projectile/input paths in C++ | Identifiers and integration hooks only. The corresponding assets are absent and are not made available by the MIT license. |

The repository does not include `.uasset`/`.umap`, FBX/model files, archives, the private game project, private conversation transcripts, personal logs or the private design document. References to external API/class names identify interoperability contracts; they do not imply endorsement, affiliation or included content rights.

Before publishing a derived host project, review every separately acquired dependency. Keep this repository's source export independent from private/third-party content folders.
