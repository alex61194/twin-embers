# Distribution policy

This repository distributes reviewed text source, licenses, synthetic tests,
context-free port edits and structural reconstruction recipes only. It has its
own Git root and imports no history from either previous repository.

ROMs, PNG/WAV, sprites, `intro_native`, generated font pixels, `.pak`, `.3dsx`,
ELF, RomFS, builds, saves and uncertain-origin blobs are excluded. External game
source and SDK dependencies remain local and retain their original rights.
Licenses and copyright notices must remain intact where required.

The target is an engine candidate with NOBITS game data, plus a pack generated
on the user's computer from the supported ROM. Builder/runtime/REX and the full
reference recipe are provided. A source-only audit pass is not binary approval
or a guarantee that every derivative use is authorized.

## Gates

Ordinary source files retain a 256 KiB size cap. The single
`docs/review-manifest.json` governance inventory has a separate 512 KiB cap
as its reviewed file records grow. Its schema, provenance entries, exact
source hashes, text/payload checks and every historical snapshot are still
validated; this allowance does not apply to other files or binary content.

`tools/source_audit.py --history` verifies every reachable commit against its
own reviewed manifest. Paths, file types, Git modes, text encoding, sizes,
encoded payload patterns and reviewed hashes are checked. Optional `--rom`
uses no uploaded ROM. File hashes follow Git's canonical LF text format; the
first Windows-reviewed import also proves exact LF/CRLF equivalence against
its original review hash. Other whitespace/content differences fail. `--rom`
compares nonuniform 64-byte aligned ROM windows with source blobs at every byte
offset. Shorter, transformed or unaligned ROM fragments can be missed.

`tools/audit_clean_release.py --release` inspects ELF/map, embedded RomFS,
required runtime symbols, engine ABI and size/CRC coverage of every expected
pack path. A local supported ROM and finished 3DSX are mandatory for release
verification; the scan checks nontrivial 32-byte windows of reconstructed
payloads. It is a supplemental exact-byte detector, not a rights determination.

`tools/source_audit.py --release` is a separate fail-closed publication policy:
it always returns status 2 with `BINARY RELEASE BLOCKED`. Passing a technical
binary audit does not lift this policy. CI verifies that exact blocked outcome,
uses no real ROM, and neither uploads artifacts nor creates releases.

Before lifting the gate, record evidence tied to the new binary hash: complete
ROM-to-pack byte equivalence, REX pointer verification and engine-exception review,
zero embedded/unknown game-data findings, clean-profile RomFS, pack coverage,
source and binary ROM scans, exact linked-license inventory, and emulator/hardware
acceptance including missing/corrupt pack screens. Any allowed residual derived
constant needs an explicit rights decision; the reference's exception list alone
is insufficient. The 336-byte game-derived exception group is externalized to the
user-built pack; the separate 56-byte port-authored group remains under review.

Workflow/manifest changes require human review. Administrators can bypass CI or
manually upload releases, so these checks are not a server-side upload ban.
