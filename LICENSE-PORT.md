# Port licensing scope

This MIT grant applies only to original port contributions owned by alex61194,
including the new governance, audit and edit-application tooling, and the owned
FireRed-specific additions. The maintainer authorized this grant on 2026-10-07.
It does not expand the rights in third-party or game-derived material.

Zallax-derived portions retain the original MIT copyright and scope in
licenses/ZallaxDev-LICENSE-PORT.md and licenses/ZallaxDev-MIT.txt. Both notices
must accompany copies or substantial portions of the respective code.

This license does not cover Pokémon, pret game/decompilation source, ROM-derived
material, referenced upstream lines, SDKs, dependency runtimes or trademarks.
Their existing rights and notices are preserved. No game source license is
inferred. See NOTICE.md and PROVENANCE.md.

## Game engine and locally generated data

A locally built `twinembers.3dsx` includes compiled game logic from the pinned
`pret/pokefirered` decompilation. Externalizing graphics, text, audio, maps and
game tables to the player's locally generated `twinembers.pak` does not remove
that compiled logic from the executable or place it under this MIT grant.

This license does not relicense Pokemon FireRed's original code or content,
the decompilation, a modified upstream file as a whole, or the resulting game
executable as a whole. In a modified upstream file, the MIT grant applies only
to original owned port contributions; inherited Zallax portions keep their
separate MIT scope. Third-party components retain their own license terms.

The same distinction appears in ZallaxDev's release `v0.3.0`: its port license
excludes the game and decompilation, while its provenance identifies compiled
pret game logic in the 3DSX. That publication is a technical precedent, not a
grant of rights to Twin Embers. No general upstream redistribution license or
rightsholder permission for the compiled FireRed engine is established here.

No ROM or pre-generated game data pack is distributed. The Builder generates
the pack locally from the player's own supported ROM and does not contain the
3DSX. These packaging choices and attribution do not themselves establish
authorization to redistribute the compiled game logic.

## MIT License for owned contributions

Copyright (c) 2026 alex61194

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
