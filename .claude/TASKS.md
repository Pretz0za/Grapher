# TASKS

## Epic 1: Fix right-click face detection

### Track outer face index after enumeration
- [x] Add `size_t outerFaceIdx` (SIZE_MAX = unknown) to `gvizLayerPolyTutte` in the header.
  Initialize to `SIZE_MAX` in `gvizLayerPolyTutteInit`.
- [x] At the end of `pt_enumerateFaces` (after the face copy loop), compute and store
  `self->outerFaceIdx = pt_findOuterFace(self)` so it is always consistent with the
  current faces array.

### Skip outer face in the ray-cast loop
- [x] In the right-click handler, change the face search loop to skip `self->outerFaceIdx`.
- [x] If `hit == SIZE_MAX` after the loop: set `hit = self->outerFaceIdx`.

### Add epsilon jitter to ray-casting
- [x] In `pt_pointInFace`, add `py += 1e-10;` at the top before the loop (after the
  `n < 3` guard).

### Clear stale faces after re-embedding
- [x] In the `KEY_ENTER` handler, after `pt_clearHighlights`, call `releaseFaces(self)`
  and set `self->outerFaceIdx = SIZE_MAX`.
- [x] In the `KEY_R` handler, after `pt_clearHighlights`, call `releaseFaces(self)`
  and set `self->outerFaceIdx = SIZE_MAX`.

### Remove per-face debug flood
- [x] Remove the `fprintf` inside `pt_pointInFace` that prints for every face on every
  right-click.

## Build
- [x] `cd build && cmake .. && make`, fix any errors.
