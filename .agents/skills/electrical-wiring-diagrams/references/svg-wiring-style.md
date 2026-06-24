# SVG Wiring Style

## Board-layout wiring

Use this when a user asks for a diagram like a real wiring image or asks to redraw an existing board photo/mockup.

- Use a light grid background.
- Put the primary controller or relay board in the center.
- Draw a simplified but recognizable board footprint: PCB rectangle, ESP32 module, relay blocks, screw terminals, power terminal.
- Put external connectors around the board edges.
- Route colored wires over the board, with bends and anchor points like a real harness diagram.
- Keep labels in white or pale boxes near their connector.
- Prefer fewer, thicker wires over many thin lines.
- Avoid long text inside the board area.

## Recommended colors

- Green: `PS_ON`, relay control, safe low-voltage control.
- Red or orange: +5 V LED/load power.
- Black or dark gray: ground.
- Purple: +5VSB or standby rail.
- Blue/cyan: GPIO control.
- Yellow dashed: optional or future data line.
- Brown: monitor/sense return or legacy signal when needed.

## Layout checklist

- Existing power-control relay wiring must remain visually separate from optional LED power.
- LED power gate should include: external 5 V PSU, fuse, MOSFET/load switch, LED/ARGB load, shared ground, and `LED_POWER_PIN`.
- If `LED_POWER_PIN` is disabled by default, state that in the diagram or caption.
- If ARGB data is not implemented, show it only as dashed optional/future data or omit it.
- Every connector block should have a title.
- Every changed feature should be visible in the first viewport of README.

## Validation checklist

- Run an XML parser for SVG.
- Search the SVG text for every required firmware symbol.
- Check that no safety-critical relay net is reused as LED power.
- Check that external LED power has a fuse and ground return.
- Check that the diagram does not imply ESP32 GPIO powers LEDs directly.
