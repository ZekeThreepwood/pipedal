---
name: Small screen primary target
description: UI must work on 480x320 (Pi 5 TFT) as primary viewport, larger screens secondary
type: project
---

All UI work targets a Raspberry Pi 5 with a 480x320 touchscreen as the PRIMARY viewport. Larger screens (desktop, tablet) are secondary.

**Why:** The device is a standalone guitar effects unit — the screen on the box IS the UI, not a browser on a laptop.

**How to apply:**
- Design for 480px wide first; desktop is an enhancement not the baseline
- No canvas-based drag-and-drop node editors — too small for precise interaction at 480px
- Prefer vertical scrollable lists over horizontal layouts
- Touch targets must be large enough for finger interaction (≥ 44px)
- The routing graph view on small screen should be a structured list/slots view, not a free-form graph canvas
- `TINY_SCREEN_WIDTH` in MainPage.tsx is currently set to 1920 — this forces small-screen mode for all screens during development; real threshold for Pi display should be ≤ 480
- `MainPageSmallScreen.tsx` is the small-screen entry point
