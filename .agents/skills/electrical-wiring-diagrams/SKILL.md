---
name: electrical-wiring-diagrams
description: Draw, revise, and review electronics wiring diagrams as deterministic repo-native SVG or similar vector assets. Use for requests involving wire routing, ESP32/Arduino boards, relay modules, connectors, power rails, grounds, pin labels, harness diagrams, README wiring images, or converting an existing wiring image into an updated technical diagram.
---

# Electrical Wiring Diagrams

Create wiring diagrams that are electrically accurate first and visually polished second. Prefer deterministic SVG/vector output for final project assets; use generated raster images only for visual exploration, never as the source of truth for pin labels.

## Workflow

1. Read the authoritative pin map and hardware contract before drawing.
2. List nets explicitly: GPIO/control, relay contacts, power rails, standby rails, monitor inputs, grounds, data lines.
3. Choose the diagram style:
   - **Board-layout wiring** when the user asks for a diagram like a photo/mockup with wires over a board.
   - **Block schematic** when clarity matters more than matching a physical board.
   - **Connector pinout** when the target is a harness or plug.
4. Place hardware first: controller board, relay module, connectors, load, external supplies, fuses, MOSFET/load switch.
5. Route safety-critical wires with minimal crossings. Keep unrelated power domains visually separated.
6. Label every wire at one end with the firmware symbol or electrical net name.
7. Include safety notes directly in the asset when GPIO must not source load current.
8. Validate the artifact:
   - XML/SVG parses.
   - Required pin labels appear exactly.
   - Power rails and grounds are not ambiguous.
   - Relay pins are not reused for unrelated loads.

## Drawing Rules

- Use `pins.h` or the equivalent project pin map as the authority, not old logs or screenshots.
- Do not claim hardware validation from a drawing.
- Do not route LED strip/fan current through an MCU GPIO.
- Show external high-current LED loads through a fused supply and MOSFET/load switch.
- Show common ground explicitly when a GPIO controls an external driver.
- Distinguish:
  - control/GPIO wires;
  - relay contact paths;
  - positive power rails;
  - ground returns;
  - monitor/sense inputs;
  - future or optional data lines.
- Use short direct labels such as `GPIO16 OPTO_PIN`, `GPIO17 EXTRA_PIN`, `GPIO4 PC_MONITOR_PIN`, `LED_POWER_PIN`.

## References

Read `references/svg-wiring-style.md` before creating or substantially revising a repo-local SVG wiring diagram.
