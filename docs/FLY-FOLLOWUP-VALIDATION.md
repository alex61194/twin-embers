# Fly follow-up: complete strip and synchronized handoffs

## Evidence and correction

The supplied 7.9-second follow-up recording confirms that the Pokemon is
translated and the streak strip appears, but BG0 ends at native X=256.
The prior placement test stubbed DrawTextBg and therefore missed its internal
tilemap-width cap. The actual DrawTextBg walker now extends the invocation's
BG0 to 400px; ordinary narrow backgrounds still stop at 256px.

The recording also exposes the boarding/arrival discontinuity. The outgoing
swoop now waits at angle 64 underneath the player until boarding completes.
The incoming bird retains the carried-motion callback when its affine scaling
finishes. Previously that transition changed the radius and doubled the angle
step while the 33-frame landing timer continued unchanged. Detachment now waits
until the preceding sprite update has reached angle 64 (next angle 66); only
then does the unoccupied bird resume its departing swoop.

These changes preserve the previous signed OAM positions, matching-frame
publication, native Pokemon centre and original controls. Game artwork,
save/data paths, ROM validation and menu ownership are unchanged.

## Checks (9 October 2026)

- Fly presentation: 7 synthetic production-function/policy tests passed.
- The new actual-BG-walker test was run against bf5b4d7's implementation and
  rejected it at the 400px assertion, confirming it catches the recorded defect.
- Phase test: pickup stays at the midpoint; affine completion retains the same
  carried-motion callback; 33 two-step frames reach next angle 66 before
  detaching; the free bird then resumes its four-step swoop.
- Field/Flash/session tests: 3 passed.
- Storage controller/lifecycle: 4 passed.
- Oak presentation: 2 passed.
- First battle: 3 passed.
- REX pipeline: 3 passed.
- Full CLEAN_RELEASE=1 verify-game-3dsx rebuild: passed, including Azahar
  loader-format relocation simulation and ARM11 m4a checks.
- Structural binary audit: zero embedded/unknown game payload bytes and zero
  game payload files in RomFS.
- Source/history validation uses unchanged production checks with cached
  immutable Git blobs; its final result is reported with the branch update.

Candidate code: 7f6997642acc350353d2ad5a7dfed19f9c388501 (clean identity).
The subsequent documentation commit does not change the runtime.
Candidate bytes: 3768368
SHA-256: bf0e19416157427125a20d4e398d341f49cfb9cb19882e0a4d69caeb0befdcf9

No ROM, pack, save or binary is uploaded. This remains a local test candidate.
No new emulator/hardware gameplay run was performed; visual acceptance requires
repeating the supplied sequence with the user's own locally generated pack.
