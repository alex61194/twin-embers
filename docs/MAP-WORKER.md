# HOME map worker

HOME's town map is built from the game's Region Map in a few steps (`3ds_port/src/3ds_home_map_impl.h`). On a console the whole build (reading the art,
then every step) runs on a worker thread of the lowest priority, so the game's thread is never held by it
(`3ds_port/src/3ds_home_map_job_impl.h`). This file says what the two threads share, how the worker starts and stops, and what was checked where.

## What the threads share

| Word | Written by | Read by | Access |
|---|---|---|---|
| build stage (`sHm.stage`) | the worker (the game's thread only in `HmBegin`, with no worker running) | both | atomic: `HM_STAGE()` (acquire), `HM_SET_STAGE()` (release) |
| stop request (`sMapAbort`) | the game's thread | the worker | atomic, relaxed (a flag, no data goes through it) |
| worker alive (`sMapRunning`) | the game's thread before creating the worker, the worker as its last act | the game's thread | atomic: release store of 0 by the worker, acquire load by the game's thread |

Everything else is owned by one thread at a time: the art copies and the image's working memory are written by the game's thread only while no worker
runs, and by the worker only while it runs; `threadJoin` orders the hand-over.

## Life of the worker

- **Start.** A map is asked for each frame (`MapJobPrepare(group, version)`). When it is a new one, the art is copied, `HmBegin` clears the working
  memory and the worker is created, but only if no other worker is still running.
- **Region changed during a build.** The game's thread sets the stop request and goes on; it does not wait. The worker looks at the request between
  the rows of the feed and between the steps, so it ends within a row or a step. The next frames see `sMapRunning` at 0, join and free it, and start the
  new map. Until then nothing is shown of the new map and nothing of the old one (`MapJobImageReady`).
- **No wait, no mix.** The image is reported ready only when the build of the map last asked for is the one that is done. A map changed again and again
  can leave several workers stopping one after another, never two at once.
- **Failure.** A map without art is decided at once (`sMap.ok` 0) and starts no worker; a build that runs out of memory ends in `HMS_FAILED`. A worker
  that cannot be created (no memory) leaves the build to the game's frames, one step each, as on a computer.
- **End of the process.** `CtrPokeTouch_Shutdown` (an `atexit` handler, run before the data pack's) asks the worker to stop and joins it, which takes
  at most one row of the feed or one step; then nothing of the worker's is touched.

## What was checked

- `tools/test_map_worker.py` (`3ds_port/tests/map_worker_test.c`): the console's own code on a computer, with POSIX threads behind
  `3ds_port/tests/shim/3ds/thread.h`. It checks that a worker's image equals a plain single-threaded build, that a stop asked of a worker that has not
  begun costs the game's thread no wait, 600 frames with the region changing at random moments (every ready image is the one of the map last asked
  for; the slowest frame is under a millisecond), a map with no art, a new version of a map, and 40 ends of the process at different points of a build
  (every worker joined and freed, the job able to start again). A mutation that makes the game's thread join the worker to cancel it fails the test.
- The same test under a thread sanitizer (`-fsanitize=thread`) when the compiler has one; this MinGW build does not, so it was **not run here**: run
  `python3 tools/test_map_worker.py` on Linux or macOS.
- The renderer's own test (`poketouch_render_test`, in both styles) still builds the map frame by frame on the host and compares incremental redraws
  with full ones.

## What needs a console or an emulator

- That the worker (priority 0x3F, on the second core of a New 3DS, time-shared on an Old 3DS) really runs beside the game and ends when asked: change
  the region on HOME's map (the key at its top left) several times in a second and see that the game does not stall, and the map shown is the last one.
- Quit the game (HOME menu) in the middle of a region change: it must close at once and leave no hung process.
- On an Old 3DS, how long a map takes to appear and whether the stop is as quick as on the host (a step there is longer).
- Azahar emulates the threads on the host's, so it shows ordering problems but not the console's timing; the real console is the reference.
