# Lockstep

Verifier that keeps OpenSoup bit-identical to the original Souptoys engine.

Each scenario runs on both engines and produces normalized text output. The original's output is recorded once on a Windows machine and frozen in `scenarios/*.expected`, so verification needs neither Windows nor the original binary:

```bash
uv run tools/lockstep/main.py                      # every scenario
uv run tools/lockstep/main.py run block --diff     # one scenario, showing every difference
```

Recording requires running on a Windows machine with the original installed:

```bash
uv run tools/lockstep/main.py record               # every scenario
uv run tools/lockstep/main.py record fall drift    # named scenarios only
```

Re-record only when a scenario is added or a golden is in doubt. A golden that changes is a claim that the original itself was measured differently.

## How the original is driven

`record` patches the installed `Toybox.exe` via `tools/patch-toybox-harness.py` into a headless harness. The harness reads Ruby statements from a file at `$TOYBOX_HARNESS_IN` and writes stdout to a file at `$TOYBOX_HARNESS_OUT`.

Both sides go through normalization before comparison: drop CR, strip the `souptoys>` prompt and `=> ` echo lines, normalize exponent width, rstrip, and drop blanks.

## Writing a scenario

A scenario is a plain Ruby script evaluated as a whole file. Every scenario starts with world setup (note that `scene_top_right` is negated):

```ruby
$e = $default_engine
$e.scene_bottom_left = Vector[0.0, 0.0]
$e.scene_top_right = Vector[-12.8, -8.0]
$e.scene_walls_changed
```

Print at `%+.9e`: nine significant digits round-trip a float exactly. Fewer hides sub-1e-6 differences. More than nine risks false mismatches from C runtime formatting differences.

To test one force in isolation, zero out everything else (gravity, air resistance, joints). Grab cases additionally need mid-air placement, since floor penetration swamps the mouse spring.

Reading a sprite's rotation frame requires `$e.render` first, since the original only recomputes it in the render pass.

## The scenarios

| file | trails |
| --- | --- |
| `fall`, `one`, `two`, `pile`, `drift` | bears falling, settling, piling; `drift` runs 1200 steps |
| `block`, `stackblocks`, `perturbed`, `two_blocks` | one block, four aligned, four tilted, two stacked |
| `blocktilt`, `blocktilt_trace` | settled tilt over nine drop offsets, and the trace through settling |
| `cornerwalls` | a block wedged into all four corners: two wall responses in one slot |
| `bearcontact`, `bearblock`, `bearblockdrop` | body-body contact, and a bear landing on a grounded block |
| `response`, `termprobe` | one mid-air contact per case, one material term at a time |
| `deepblocks`, `depthprobe`, `penmeasure`, `penplace`, `pairprobe`, `stagebisect` | deep penetration: depth sweeps, the penetration divided back out, the same overlap at six world positions |
| `shapedump` | the engine's own body-local collision geometry, through `Shape#vertices` |
| `jointprobe`, `jointall`, `fixedjoint` | one joint, all eleven of a bear's, and 40 net limbs on a fixedMove rim |
| `airprobe`, `corprobe` | air resistance, and drag acting off the centre of mass |
| `magnetprobe`, `magnetguard` | magnet range, force law, damping, one-directionality, and the self-attraction guards |
| `cogprobe` | gear transmission between meshed cogs and between two anchored hubs |
| `shockorder` | `Limb#shock_order` on the World limbs and on ordinary ones |
| `grabprobe`, `worldpoint` | mouse drag; `Limb#to_world` / `#to_local` |
| `spin` | free spin, reading each limb's inverse inertia back out |
| `framesweep` | (rendering, not physics) orientation -> sprite rotation frame, 92 angles around every phase boundary |
