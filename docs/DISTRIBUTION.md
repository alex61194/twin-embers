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
`docs/review-manifest.json` governance inventory has a separate 1 MiB cap
as its reviewed file records grow. Its schema, provenance entries, exact
source hashes, text/payload checks and every historical snapshot are still
validated; this allowance does not apply to other files or binary content.

The only binary files admitted are the README images, by owner decision: PNG, JPEG or
WebP files placed directly in `docs/media/`, at most 1 MiB each. Each must be one
well-formed image with nothing after its end (PNG ends at `IEND`, JPEG at `FFD9`, WebP at
its RIFF length), is hashed byte for byte in the manifest, keeps mode 100644 and is still
compared against `--rom`. `.gitattributes` marks `docs/media/*` as binary so Git never
rewrites their bytes. An image anywhere else, or any other binary type, is refused.

`tools/source_audit.py --history` verifies every reachable commit against its
own reviewed manifest. Paths, file types, Git modes, text encoding, sizes,
encoded payload patterns and reviewed hashes are checked. Optional `--rom`
uses no uploaded ROM. File hashes follow Git's canonical LF text format; the
first Windows-reviewed import also proves exact LF/CRLF equivalence against
its original review hash. Other whitespace/content differences fail. `--rom`
compares nonuniform 64-byte aligned ROM windows with source blobs at every byte
offset. Shorter, transformed or unaligned ROM fragments can be missed.

**Historical manifest repair (one commit).** The merge `bbcb09cad4` (2026-11-12) was committed with four unresolved conflict hunks in
`docs/review-manifest.json`, so that snapshot's own manifest is not JSON and no per-file review row can reach it (the audit reads the
snapshot's manifest first). `historical_manifest_repairs` in the current manifest holds the owner's record for it, and `repair_manifest` in
`tools/source_audit.py` applies it under these rules, tested in `tools/test_source_audit.py`:

- the record names the full commit and the SHA-256 of that snapshot's exact manifest bytes; any other commit or other bytes are not repaired,
  and a manifest that parses is never repaired;
- it names, hunk by hunk, the entry's path and the hash that entry takes, which must be the real SHA-256 of that path in the snapshot's Git
  tree; a hunk must be a plain disagreement about one `sha256` line (neither side held the hash of the bytes in that snapshot);
- nothing else changes: the repaired manifest goes through the same audit as any other, so every file of the snapshot is still checked for path,
  type, mode, size, encoded payloads, embedded-payload directives and ROM windows, and every other entry still has to prove its bytes;
- the four entries of the hunks (`3ds_poketouch.h`, `3ds_bottom.c`, `3ds_poketouch.c`, `poketouch_render_test.c`) are accepted on those content
  checks only. **No review of those exact bytes is claimed**; the record says so.

It repairs the audit's reading of one snapshot and does not touch history, the other 800-odd entries, or the rows still missing for other
revisions (`--history` reports those until the owner supplies them).

`tools/audit_clean_release.py --release` inspects ELF/map, embedded RomFS,
required runtime symbols, engine ABI and size/CRC coverage of every expected
pack path. A local supported ROM and finished 3DSX are mandatory for release
verification; the scan checks nontrivial 32-byte windows of reconstructed
payloads. It is a supplemental exact-byte detector, not a rights determination.

**Owner decision (2026-10-09).** The project owner decided to attach `twinembers.3dsx` and
`TwinEmbersBuilder.exe` to the GitHub release `v0.1.0-beta.1`. The executable contains compiled game logic from
`pret/pokefirered`; no licence, rightsholder permission or legal opinion for it has been identified, so
its rights are **not independently verified**: this is the owner's decision and risk, not a
clearance. The decision is recorded in `docs/release-decision.json`.

`tools/source_audit.py --release` is a separate fail-closed publication policy. It returns status 2
with `BINARY RELEASE BLOCKED` unless `docs/release-decision.json` exists, is complete, has kind
`owner-risk-acceptance`, states `rights_verified: false`, and (with `--asset FILE`) names the exact
SHA-256 of each binary. With such a record it returns status 0 and prints that rights are **not
independently verified**. A decision can never claim verified rights, and
`binary_release_approved` in the manifest stays `false`, because that flag is reserved for independently
verified clearance. Passing a technical binary audit alone does not open the gate. CI checks both
outcomes, uses no real ROM, and neither uploads artifacts nor creates releases.

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
