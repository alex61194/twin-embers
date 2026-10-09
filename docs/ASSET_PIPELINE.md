# Clean source pipeline

1. Fetch external upstream at `upstream.lock` into an ignored fresh build tree.
2. Apply 94 reference-derived context-free edits and the pack-backed font bridge.
3. Compile using the ARM11 ABI. `asset_bundle.py` removes INCBIN storage;
   `rex_objects.py` converts the recipe's read-only data sections to NOBITS and
   records pointer sites while preserving symbol shape and alignment.
4. Keep pointer targets as linker roots. `rex_link.py` writes executable-specific
   REX metadata after linking. The pack stays independent of link addresses.
5. Stage only `engine/{abi,assets,rex,profile}.bin` into clean RomFS. The runtime
   refuses development loose-file and embedded-data backends in clean profile.
6. The Builder identifies the user's ROM by SHA-1, assembles the recipe partitions,
   executes bounded C/R operations and checks every output CRC, size and ABI.
   It verifies a temporary pack before atomically replacing the requested output.
7. Before game entry, REX copies pack bytes into NOBITS storage and remaps pointer
   sites using the new executable's link metadata. Graphics are loaded through
   the asset boundary. The interface font uses the same pack-backed data.

The reference recipe has a static superset of entries; linker garbage collection
can leave some unused by a particular binary. Every path expected by that binary
must exist with the exact size and CRC. The independent release policy stays
closed until acceptance of a real new binary is recorded.
