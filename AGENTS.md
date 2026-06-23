# BC-250 development guidance

This repository contains ESP32/Arduino firmware and a LittleFS-hosted web UI for controlling BC-250 power.

## Repository map

- `ota_pc_remote.ino`: lifecycle, main loop, Wi-Fi and controller persistence.
- `pc_control.h`: GPIO initialization, debounce, PC-state tracking, and power state machine.
- `gamepad_controller.h`: generic Bluepad32 controller identity, enrollment, filtering, and power-on trigger handling.
- `web_server.h`: HTTP API, static assets, and OTA handlers.
- `data/`: LittleFS-hosted HTML, CSS, JavaScript, and SVG.
- `pins.h`: hardware pin assignments; changes are hardware-critical.
- `version.h`: firmware version exposed by the API.

## Required rules

- Preserve non-blocking `millis()`-based transitions. Do not add long waits to `loop()` or request handlers.
- Treat relay levels, pulse durations, and GPIO assignments as safety-critical.
- Keep `web_server.h` endpoints and every `fetch()` call in `data/` synchronized.
- Preserve AP-mode operation at `http://192.168.4.1`; do not require internet access or HTTPS.
- Never expose stored Wi-Fi passwords through logs or API responses.
- Treat `LittleFS.begin(true)` as destructive because it may format the filesystem.
- Do not claim hardware validation unless tested on the ESP32 and BC-250 wiring.
- Preserve unrelated user changes.
- Start every task that will change files on a dedicated git branch before editing, unless the user explicitly asks for read-only analysis only.
- Finish each file-changing task by committing the scoped changes, pushing the branch, and opening a pull request.

## Skills and delegation

- Start every new work request by creating an explicit task record before planning or editing.
- Run work through these phases by default: task creation, planning, implementation with tests for new code, validation, and final testing.
- Allow each phase to be invoked independently when the user asks for only task creation, only planning, only implementation, only validation, or only testing.
- In the task creation phase, capture the requested outcome, scope, affected surfaces, constraints, acceptance criteria, test obligations, and whether hardware or network access is required.
- In the planning phase, break the task into ordered steps, choose the responsible agent or skill for each surface, identify required tests, and call out validation that cannot run locally.
- In the implementation phase, make the scoped code changes and add or update deterministic tests covering new behavior before moving to validation.
- In the validation phase, run the relevant static, contract, compile, browser, and host-side checks for the changed surfaces and report skipped hardware checks separately.
- In the final testing phase, run the repository verification baseline where available, summarize pass/fail/skipped results, and list remaining hardware-only checks without claiming they ran.
- Use `task_orchestrator` to coordinate the full pipeline or any independently requested phase.
- Use `$bc250-firmware` for GPIO, buttons, PC monitoring, and power sequencing.
- Use `$bc250-connectivity` for Wi-Fi, Bluepad32, LittleFS, OTA, and versioning.
- Use `$bc250-gamepad-support` for generic controller architecture, identity, enrollment, trigger policies, and compatibility testing.
- Use `$bc250-web-ui` for HTTP routes and files under `data/`.
- Use `$wled-core` for WLED core firmware and PlatformIO work.
- Use `$wled-effects` for WLED effects, palettes, and hot-path rendering.
- Use `$wled-usermod` for WLED extensions and custom effects.
- Use `$wled-web-api` for WLED JSON, WebSocket, HTTP, and embedded UI work.
- Use `$bc250-wled-integration` when adapting behavior between the two repositories.
- Use `$embedded-build-validation` to select build and test matrices across both projects.
- Use `$embedded-security-review` for defensive review across both projects.
- Delegate task creation and phase coordination to `task_orchestrator`.
- Delegate to `power_firmware`, `platform_connectivity`, or `web_contract` for clear owned areas.
- Delegate completed changes to `code_reviewer` for an independent, findings-first review.
- Delegate test design and test infrastructure to `test_engineer`.
- Delegate host-side regression coverage and contract tests to `coverage_engineer`.
- Delegate authorized defensive analysis to `security_tester`; keep it read-only and local by default.
- For cross-cutting changes, assign disjoint files and verify shared contracts explicitly.

## Review and security boundaries

- Review agents do not edit production code unless explicitly assigned a remediation task.
- Security testing is limited to this repository and explicitly authorized local test instances.
- Do not scan external hosts, GitHub, the upstream owner, or network devices outside the stated scope.
- Do not upload firmware, format LittleFS, change credentials, operate relays, or run disruptive tests without explicit authorization.
- Security findings must separate confirmed vulnerabilities, design risks, and hypotheses.

## Verification baseline

- Run `git diff --check`.
- Run `python3 tools/run_host_tests.py` when Python 3 is available.
- Match every frontend endpoint to an existing route and HTTP method.
- Match UI-consumed JSON fields to firmware responses.
- Review idle, power-on, normal shutdown, forced shutdown, timeout, and reboot transitions.
- Compile with the ESP32 Bluepad32 board package when the toolchain is available.
- Report static, compile, browser, and hardware checks separately.
