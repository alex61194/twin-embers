# Contributing

This is the public source repository for Twin Embers Port. Contributions are
welcome through issues and pull requests. Creating releases and uploading binary
artifacts are maintainer responsibilities, outside an ordinary contribution.
Keep the existing architecture and release-decision checks.

Contributions must not contain ROMs, `.pak`, `.3dsx`, ELF/CIA, saves, game
screenshots, extracted Pokémon graphics, sprites, audio/music/cries, maps,
ROM-derived binary blobs, copied pret source context, Base64 game/media payloads,
build artifacts or files of uncertain origin. Do not attach those materials to
issues, pull requests, comments, logs or CI artifacts.

For every proposed reused component, identify:

- Code origin and upstream repository.
- Exact revision/commit and source path.
- Applicable license and required attribution.
- Whether it is original, adapted or third-party material, and what changed.

Original work must also identify its author and license scope. Credit alone does
not grant permission to redistribute someone else's code or game material.
Preserve existing copyright/license notices and update `NOTICE.md`, provenance
and `docs/review-manifest.json` when relevant. Never mark uncertain material as
reviewed merely to make the audit pass.

`pret/pokefirered` must stay external and commit-pinned. Port edit recipes may
reference hash-verified local source lines; they must not copy upstream context
or game data into this repository. Keep reconstruction recipes structural, with
no literal payloads, and do not change entry counts or ABI without explicit review.

Use small, reversible commits and normal pushes. No reset, rebase, force-push,
history rewrite or squash of existing history. Run the source/history audit and
the applicable synthetic tests. Own-ROM tests are local opt-in only; CI must not
receive a ROM or publish any generated game/binary artifact. The owner will
decide README, branding and publication changes separately.
