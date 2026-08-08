# storyfuzz — exploratory system testing for remacs

Coherent editor-operation **stories** (typing, isearch, SKK input and
conversion, window juggling, kill/yank, query-replace, minibuffer
prompts, paste bursts, ...) are synthesized from a random seed,
randomly interleaved, and randomly aborted mid-story with C-g. Many
seeded sessions run in parallel against a real pty; a session fails
when the editor

- **CRASH**: dies on a signal,
- **RT_ERROR**: trips `noct_error()` / `rt_error()` (the launcher
  prints `file:line: Error: ...` on stderr and exits nonzero),
- **EXIT(n)**: exits nonzero any other way,
- **HANG**: does not exit within the grace period after C-x C-c.

## Usage

```
# explore: 200 sessions, 8 workers, random base seed
python3 storyfuzz.py --runs 200 --jobs 8

# fixed sweep (repeatable)
python3 storyfuzz.py --runs 400 --jobs 8 --base-seed 20000

# reproduce one failure, printing every key sent
python3 storyfuzz.py --seed 12345 --verbose
```

The byte stream is deterministic per seed. Failures land in
`failures/seed-<N>/` with `keylog.txt` (labeled steps + replay
command), `stderr.txt`, and `screen-raw.txt` (the last 8KB of
terminal output — the final frame shows where the session was).

Knobs: `--stories` (stories per session), `--abort-p` (per-step C-g
switch probability), `--time-limit`, `--remacs`.

## Debugging a failure

1. `--seed N --verbose` to watch the story; `keylog.txt` names each
   step.
2. `screen-raw.txt` tail = the last frame the editor drew; the frozen
   echo line usually names the active mode (e.g. `C-u-`).
3. For stalls, `tools/profile_lines.py` spawns remacs with
   `REMACS_NO_JIT=1` (interpreted frames keep `env->line` fresh),
   replays a scenario, and samples the executing Noct source line via
   gdb into a histogram. Edit `drive()` to the scenario under
   investigation.

## Bugs found by this framework (2026-08-09, first two sweeps)

- **User errors killed the editor**: `error()` ("No further undo
  information", "End of buffer", "Kill ring is empty", ...) unwound
  out of `noct_enter_vm` and terminated the session. Fix: the command
  loop wraps `dispatchKey` in `System.pcall` (new core API — a
  protected call, the condition-case of the command loop); the message
  lands in the echo area and editing continues.
- **Quasi-hang: paste into a vertically split window** (regression
  test `term/case_split_paste.py`): the scroll-fit loop in
  `renderWindow` estimated one display row per logical line; with
  wrapped lines it recomputed the same start line up to the 500-pass
  guard on *every* frame (seconds per keystroke). Fix: force progress
  toward the point's line and stop when a single logical line
  overflows the window.

## Notes

- The pty transcript check tolerates timing jitter, but a failure
  seed is deterministic in the bytes it sends; a rare timing-dependent
  failure may need a few replays to fire again.
- Not part of `run-all.sh` (randomized); run sweeps manually or from
  CI with a fixed `--base-seed`.
