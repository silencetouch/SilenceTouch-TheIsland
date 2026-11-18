# The Island — a spatial panner for multi-channel mixing

Part of the **Silence Touch** plugin for VCV Rack 2 (plugin slug `SilenceTouch`), by **[Silence Touch](https://silencetouch.com)**.

The Island places up to 12 stereo sources inside a 3D room and renders them to a multi-channel speaker layout
(**4.0 / 5.1 / 7.1 / 7.1.4**), with outputs in standard **SMPTE channel order**. It was built for live
immersive performance from VCV Rack and works with any multi-channel audio interface.

繁體中文說明：[README.zh-TW.md](README.zh-TW.md) · 面板功能一覽：[面板功能一覽.md](面板功能一覽.md)

**Support this project:** if The Island is useful to you, you can [buy me a coffee via PayPal](https://www.paypal.com/ncp/payment/F6ENFTYRVXBT6). Thank you!

## Modules

### The Island (50 HP)
- **12 stereo tracks.** Each track has L/R inputs, level, X / Y / Z position knobs with CV inputs, stereo **WIDTH**,
  and an **LFE** send (120 Hz low-pass). Patching only one input treats the track as mono.
- **Cube display.** Every source is drawn at its real position (including CV, orbit and rotation): stereo tracks as a
  capsule (L and R at the ends), mono tracks as a dot. Click to switch view: ISO / TOP / FRONT / SIDE.
- **Orbit (ORB).** Sends a track around the room and back. One-turn or loop mode, left/right direction,
  horizontal or vertical axis, per-track speed (**SPD**), circle size (**DIA**) and a global **ORB RATE**.
  Patch an LFO into the track's **LFO** jack and the LFO drives the angle while ORB is on.
- **Split (SP).** L and R separate from the stereo centre and circle in mirror image (L to the left, R to the right).
- **Mute / Solo** per track, with click-free fades.
- **ROTATE** the whole sound field around the X, Y or Z axis, with CV.
- **FOCUS** and **BLUR** control how tightly sources are localised.
- **Outputs:** 12 mono outputs plus one 12-channel polyphonic output. Choose the layout from the context menu
  (4.0, 5.1, 7.1, 7.1.4); unused outputs are muted and dimmed on the panel.
- **Speaker test.** TEST plays a 1 kHz tone (80 Hz on LFE) on one channel at a time, either stepping
  automatically (AUTO) or on the channel chosen with TEST CH.
- **Level LEDs** per track and per output (green / yellow / red) and an optional output safety limiter.
- **Four panel themes:** Sky (default), Mist, Sand, Dark, plus Auto (follows Rack's "Use dark panels").

| Output | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Channel | L | R | C | LFE | Ls | Rs | Lrs | Rrs | Ltf | Rtf | Ltr | Rtr |

### The Island Room (6 HP expander)
Place it directly to the left or right of The Island (the LINK light turns on).
It changes the room used for panning from a cube to the shape of your space.
- **WIDTH / DEPTH / HEIGHT**: left-to-right wall, front-to-rear wall, and ear-level speakers to ceiling speakers.
- Units: metres, centimetres, feet or plain numbers (context menu). Only the ratio affects the panning.
- Click the **RATIO** display to type a ratio directly, e.g. `1:2:0.5`.
- **ORBIT**: FIT (orbits fill the room) or ROUND (true circles in the real room).
- Without the expander The Island behaves exactly as a cube room.

## Panning algorithm
Distance-based amplitude panning (DBAP): each speaker's gain is `1 / (d² + blur²)^(focus/2)`, then the gains are
energy-normalised so the overall level stays constant as sources move. DBAP needs no speaker triangulation,
moves smoothly between speakers and does not assume the listener sits in the sweet spot.
The LFE channel is not spatialised; it only receives each track's LFE send.

## CV conventions
Position CV is bipolar and added to the knob: 0 V = knob position, ±5 V = one full wall-to-centre span
(±10 V option in the context menu). ROT CV and the orbit LFO input: ±5 V = ±180°.

## Building
```
make RACK_DIR=/path/to/Rack-SDK
make RACK_DIR=/path/to/Rack-SDK install
```

## License
Source code: GPL-3.0-or-later (see `LICENSE-GPLv3.txt`).
Font: Jost, SIL Open Font License (`res/fonts/Jost-OFL.txt`).
The Silence Touch name and logo are trademarks of Silence Touch and are not covered by the GPL.
