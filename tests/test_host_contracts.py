#!/usr/bin/env python3
"""Deterministic host-side checks for BC-250 firmware/UI contracts.

These tests intentionally parse source files and mirror small validation rules.
They do not compile Arduino code, start a web server, touch LittleFS, operate
GPIO/relays, perform network access, or require ESP32 hardware.
"""

from __future__ import annotations

import json
import re
import unittest
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


REPO_ROOT = Path(__file__).resolve().parents[1]
WEB_SERVER = REPO_ROOT / "web_server.h"
DATA_DIR = REPO_ROOT / "data"
GAMEPAD_CONTROLLER = REPO_ROOT / "gamepad_controller.h"
FIRMWARE = REPO_ROOT / "ota_pc_remote.ino"
PINS = REPO_ROOT / "pins.h"
LED_POWER_DRIVER = REPO_ROOT / "led_power_driver.h"
BUILD_FIRMWARE = REPO_ROOT / "tools" / "build_firmware.py"


@dataclass(frozen=True, order=True)
class EndpointUse:
    source: str
    method: str
    path: str


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def extract_routes() -> set[tuple[str, str]]:
    """Return registered (method, path) pairs from web_server.h."""

    text = read_text(WEB_SERVER)
    routes: set[tuple[str, str]] = set()
    route_re = re.compile(
        r'server\.on\(\s*"(?P<path>[^"]+)"'
        r'(?:\s*,\s*HTTP_(?P<method>GET|POST|PUT|DELETE|PATCH))?',
        re.MULTILINE,
    )
    for match in route_re.finditer(text):
        method = match.group("method") or "GET"
        routes.add((method, match.group("path")))
    return routes


def _method_from_fetch_options(options: str | None) -> str:
    if not options:
        return "GET"
    method_match = re.search(r"method\s*:\s*['\"](?P<method>[A-Za-z]+)['\"]", options)
    return method_match.group("method").upper() if method_match else "GET"


def _extract_fetches_from_text(source_name: str, text: str) -> Iterable[EndpointUse]:
    # Literal fetch('/path') and fetch('/path', { method: 'POST', ... }).
    literal_fetch_re = re.compile(
        r"fetch\(\s*['\"](?P<path>/[^'\"]+)['\"]"
        r"(?:\s*,\s*(?P<options>\{.*?\}))?\s*\)",
        re.DOTALL,
    )
    for match in literal_fetch_re.finditer(text):
        yield EndpointUse(
            source=source_name,
            method=_method_from_fetch_options(match.group("options")),
            path=match.group("path"),
        )

    # XMLHttpRequest.open('POST', '/path', ...).
    xhr_open_re = re.compile(
        r"\.open\(\s*['\"](?P<method>[A-Za-z]+)['\"]\s*,\s*['\"](?P<path>/[^'\"]+)['\"]",
        re.DOTALL,
    )
    for match in xhr_open_re.finditer(text):
        yield EndpointUse(
            source=source_name,
            method=match.group("method").upper(),
            path=match.group("path"),
        )


def _extract_send_command_uses(source_name: str, text: str) -> Iterable[EndpointUse]:
    """Resolve index.html's fetch(`/${endpoint}`) calls from sendCommand users."""

    if "fetch(`/${endpoint}`" not in text:
        return

    function_match = re.search(
        r"async\s+function\s+sendCommand\s*\([^)]*\)\s*\{(?P<body>.*?)\n\}",
        text,
        re.DOTALL,
    )
    if not function_match:
        return

    method = _method_from_fetch_options(function_match.group("body"))
    call_re = re.compile(r"sendCommand\(\s*['\"](?P<endpoint>[^'\"]+)['\"]")
    for match in call_re.finditer(text):
        yield EndpointUse(
            source=source_name,
            method=method,
            path="/" + match.group("endpoint").lstrip("/"),
        )


def extract_frontend_endpoint_uses() -> set[EndpointUse]:
    uses: set[EndpointUse] = set()
    for path in sorted(DATA_DIR.glob("*.html")):
        text = read_text(path)
        source_name = str(path.relative_to(REPO_ROOT))
        uses.update(_extract_fetches_from_text(source_name, text))
        uses.update(_extract_send_command_uses(source_name, text))
    return uses


def route_block(route_path: str, method: str) -> str:
    text = read_text(WEB_SERVER)
    marker = f'server.on("{route_path}", HTTP_{method}'
    if marker not in text and method == "GET":
        marker = f'server.on("{route_path}"'
    start = text.index(marker)
    next_route = text.find("server.on(", start + len(marker))
    if next_route == -1:
        next_route = text.find("server.onNotFound", start)
    return text[start:next_route]


def assert_route_mentions_fields(
    test_case: unittest.TestCase,
    route_path: str,
    method: str,
    fields: Iterable[str],
) -> None:
    block = route_block(route_path, method)
    for field in fields:
        field_patterns = (
            f'["{field}"]',
            f'createNestedObject("{field}")',
            f'createNestedArray("{field}")',
        )
        test_case.assertTrue(
            any(pattern in block for pattern in field_patterns),
            msg=(
                f"{method} {route_path} does not appear to emit JSON field "
                f"{field!r}"
            ),
        )


SUPPORTED_TRIGGER_MODES = {"connect", "button"}
SUPPORTED_TRIGGER_BUTTONS = {
    "system",
    "start",
    "select",
    "capture",
    "a",
    "b",
    "x",
    "y",
}
MAX_AUTHORIZED_CONTROLLERS = 4
CONTROLLER_CONFIG_SCHEMA = 1
LED_CHANNEL_COUNT = 2
LED_MAX_PIXELS_PER_CHANNEL = 50
SUPPORTED_LED_EFFECTS = {"static", "breathing", "colorCycle", "rainbow", "sequential"}


def normalize_controller_mac(value: object) -> str:
    return str(value or "").strip().lower().replace("-", ":")


def is_valid_controller_mac(value: object) -> bool:
    mac = normalize_controller_mac(value)
    return bool(re.fullmatch(r"[0-9a-f]{2}(:[0-9a-f]{2}){5}", mac))


def validate_controller_config_for_web_post(payload: dict) -> tuple[bool, str]:
    """Mirror updateControllerConfigFromJson() in ota_pc_remote.ino."""

    trigger = payload.get("trigger") or {}
    trigger_mode = trigger.get("mode", "connect")
    trigger_button = trigger.get("button", "system")
    rearm_ms = trigger.get("rearmMs", 5000)

    if trigger_mode not in SUPPORTED_TRIGGER_MODES:
        return False, "Unsupported trigger mode"
    if trigger_button not in SUPPORTED_TRIGGER_BUTTONS:
        return False, "Unsupported trigger button"
    if rearm_ms < 1000 or rearm_ms > 60000:
        return False, "rearmMs must be between 1000 and 60000"

    seen: set[str] = set()
    for item in payload.get("controllers") or []:
        if len(seen) >= MAX_AUTHORIZED_CONTROLLERS:
            return False, "Too many authorized controllers"

        mac = normalize_controller_mac(item.get("macAddress"))
        if not is_valid_controller_mac(mac):
            return False, "Invalid controller MAC address"
        if mac in seen:
            return False, "Duplicate controller MAC address"
        seen.add(mac)

    return True, ""


def validate_persisted_controller_config(payload: dict) -> tuple[bool, str]:
    """Mirror loadControllerConfig() schema gate before config validation."""

    if payload.get("schema", 0) != CONTROLLER_CONFIG_SCHEMA:
        return False, "invalid or unsupported config"
    return validate_controller_config_for_web_post(payload)


def validate_led_state_for_web_post(payload: dict) -> tuple[bool, str]:
    """Mirror updateLedStateFromJson() validation checks."""

    if "bri" in payload:
        brightness = payload.get("bri", -1)
        if brightness < 0 or brightness > 255:
            return False, "bri must be between 0 and 255"

    if "rgb" in payload:
        rgb = payload.get("rgb")
        if not isinstance(rgb, list) or len(rgb) != 3:
            return False, "rgb must contain three values"
        if any(value < 0 or value > 255 for value in rgb):
            return False, "rgb values must be between 0 and 255"

    if "channels" in payload:
        channels = payload.get("channels")
        if not isinstance(channels, list):
            return False, "invalid LED channels"
        if len(channels) > LED_CHANNEL_COUNT:
            return False, "too many LED channels"

        for channel in channels:
            channel_id = channel.get("id", 0)
            if channel_id < 1 or channel_id > LED_CHANNEL_COUNT:
                return False, "invalid LED channel id"

            if "pixels" in channel:
                pixels = channel.get("pixels", -1)
                if pixels < 0 or pixels > LED_MAX_PIXELS_PER_CHANNEL:
                    return False, "pixels must be between 0 and 50"

            if "effect" in channel and channel.get("effect") not in SUPPORTED_LED_EFFECTS:
                return False, "unsupported LED effect"

            for field in ("color", "secondaryColor"):
                if field in channel:
                    color = channel.get(field)
                    if not isinstance(color, list) or len(color) != 3:
                        return False, f"{field} must contain three values"
                    if any(value < 0 or value > 255 for value in color):
                        return False, f"{field} values must be between 0 and 255"

            if "speed" in channel:
                speed = channel.get("speed", -1)
                if speed < 1 or speed > 100:
                    return False, "speed must be between 1 and 100"

    return True, ""


class WebContractTests(unittest.TestCase):
    def test_frontend_endpoint_uses_match_registered_routes_and_methods(self) -> None:
        routes = extract_routes()
        frontend_uses = extract_frontend_endpoint_uses()

        missing = sorted(
            use for use in frontend_uses if (use.method, use.path) not in routes
        )

        self.assertEqual(
            [],
            missing,
            "Every frontend fetch/XHR endpoint must be registered in web_server.h "
            "with the same HTTP method.",
        )

    def test_representative_frontend_uses_are_detected(self) -> None:
        uses = extract_frontend_endpoint_uses()
        expected = {
            EndpointUse("data/index.html", "GET", "/api/status"),
            EndpointUse("data/index.html", "POST", "/power/on"),
            EndpointUse("data/index.html", "POST", "/power/off"),
            EndpointUse("data/led.html", "GET", "/api/led/state"),
            EndpointUse("data/led.html", "POST", "/api/led/state"),
            EndpointUse("data/setup.html", "GET", "/api/wifi/config"),
            EndpointUse("data/setup.html", "POST", "/api/wifi/config"),
            EndpointUse("data/setup.html", "GET", "/api/controllers/config"),
            EndpointUse("data/setup.html", "POST", "/api/controllers/config"),
            EndpointUse("data/setup.html", "POST", "/api/controllers/enroll"),
            EndpointUse("data/setup.html", "POST", "/api/controllers/remove"),
            EndpointUse("data/update.html", "POST", "/update"),
        }
        self.assertTrue(
            expected.issubset(uses),
            f"Endpoint extraction missed expected uses: {sorted(expected - uses)}",
        )

    def test_ui_consumed_status_json_fields_are_emitted(self) -> None:
        assert_route_mentions_fields(
            self,
            "/api/status",
            "GET",
            ["pcOn", "shutdownRequested", "forceShutdown", "version"],
        )

    def test_ui_consumed_wifi_config_json_fields_are_emitted_without_password(self) -> None:
        assert_route_mentions_fields(
            self,
            "/api/wifi/config",
            "GET",
            ["ssid", "configured", "apMode"],
        )
        self.assertNotIn(
            '["password"]',
            route_block("/api/wifi/config", "GET"),
            "Wi-Fi config API must not expose stored passwords.",
        )

    def test_wifi_scan_ui_uses_bounded_dom_rendering(self) -> None:
        setup_html = read_text(DATA_DIR / "setup.html")

        self.assertIn("async function scanWiFi()", setup_html)
        self.assertIn("renderNetworkList(networks)", setup_html)
        self.assertIn("document.createElement('li')", setup_html)
        self.assertIn("for (let attempt = 0; attempt < 6; attempt++)", setup_html)
        self.assertNotIn("net.ssid.replace", setup_html)

    def test_ui_consumed_controller_json_fields_are_emitted(self) -> None:
        assert_route_mentions_fields(
            self,
            "/api/controllers/config",
            "GET",
            ["schema", "enabled", "trigger", "mode", "button", "rearmMs", "controllers"],
        )
        assert_route_mentions_fields(
            self,
            "/api/controllers/status",
            "GET",
            ["state", "bluetoothRunning", "connected", "authorized", "bluepad32Firmware"],
        )
        assert_route_mentions_fields(
            self,
            "/api/controllers/discovered",
            "GET",
            ["available", "macAddress", "modelName", "vendorId", "productId"],
        )

    def test_ui_consumed_led_json_fields_are_emitted(self) -> None:
        self.assertIn("writeLedStateJson(doc)", route_block("/api/led/state", "GET"))
        firmware_text = read_text(FIRMWARE)
        for field in (
            "schema",
            "on",
            "bri",
            "channelCount",
            "maxPixelsPerChannel",
            "power",
            "configured",
            "pin",
            "activeHigh",
            "enabled",
            "requested",
            "ready",
            "settleMs",
            "supportedEffects",
            "channels",
            "id",
            "enabled",
            "pixels",
            "effect",
            "color",
            "secondaryColor",
            "speed",
            "reverse",
        ):
            field_patterns = (
                f'["{field}"]',
                f'createNestedArray("{field}")',
                f'createNestedObject("{field}")',
            )
            self.assertTrue(
                any(pattern in firmware_text for pattern in field_patterns),
                msg=f"LED state serializer does not appear to emit {field!r}",
            )

    def test_led_frontend_uses_bounded_native_controls(self) -> None:
        led_html = read_text(DATA_DIR / "led.html")

        self.assertIn('type="range" min="0" max="255"', led_html)
        self.assertIn('type="color"', led_html)
        self.assertIn("clampByte", led_html)
        self.assertIn("channelTemplate", led_html)
        self.assertIn("powerDriverSummary", led_html)
        self.assertIn("JSON.stringify(collectLedState())", led_html)

    def test_led_power_driver_is_optional_and_separate_from_pc_relays(self) -> None:
        pins_text = read_text(PINS)
        driver_text = read_text(LED_POWER_DRIVER)
        firmware_text = read_text(FIRMWARE)
        build_text = read_text(BUILD_FIRMWARE)

        self.assertIn("#define LED_POWER_PIN -1", pins_text)
        self.assertIn("#define LED_POWER_ACTIVE_LEVEL HIGH", pins_text)
        self.assertIn("#define LED_POWER_SETTLE_MS 50UL", pins_text)
        self.assertIn("class LedPowerDriver", driver_text)
        self.assertIn("bool isConfigured() const", driver_text)
        self.assertIn("LED_POWER_PIN >= 0", driver_text)
        self.assertIn("LedPowerDriver ledPowerDriver", firmware_text)
        self.assertIn("ledPowerDriver.setEnabled(requestedPower)", firmware_text)
        self.assertIn('"led_power_driver.h"', build_text)
        self.assertNotIn("OPTO_PIN", driver_text)
        self.assertNotIn("EXTRA_PIN", driver_text)


class ControllerConfigSchemaTests(unittest.TestCase):
    def test_web_post_accepts_representative_valid_config_and_normalized_macs(self) -> None:
        payload = {
            "enabled": True,
            "trigger": {"mode": "button", "button": "a", "rearmMs": 1000},
            "controllers": [
                {
                    "macAddress": "AA-BB-CC-DD-EE-01",
                    "label": "8BitDo Ultimate 2",
                    "enabled": True,
                },
                {
                    "macAddress": "aa:bb:cc:dd:ee:02",
                    "label": "Generic HID",
                    "enabled": False,
                },
            ],
        }

        self.assertEqual((True, ""), validate_controller_config_for_web_post(payload))

    def test_web_post_accepts_defaults_and_empty_controller_list(self) -> None:
        self.assertEqual(
            (True, ""),
            validate_controller_config_for_web_post({"enabled": False}),
        )

    def test_persisted_config_requires_supported_schema(self) -> None:
        valid = {
            "schema": CONTROLLER_CONFIG_SCHEMA,
            "enabled": True,
            "trigger": {"mode": "connect", "button": "system", "rearmMs": 5000},
            "controllers": [],
        }
        invalid_schema = dict(valid, schema=2)

        self.assertEqual((True, ""), validate_persisted_controller_config(valid))
        self.assertEqual(
            (False, "invalid or unsupported config"),
            validate_persisted_controller_config(invalid_schema),
        )

    def test_invalid_trigger_mode_is_rejected(self) -> None:
        ok, error = validate_controller_config_for_web_post(
            {"trigger": {"mode": "hold", "button": "system", "rearmMs": 5000}}
        )
        self.assertFalse(ok)
        self.assertEqual("Unsupported trigger mode", error)

    def test_invalid_trigger_button_is_rejected(self) -> None:
        ok, error = validate_controller_config_for_web_post(
            {"trigger": {"mode": "button", "button": "l3", "rearmMs": 5000}}
        )
        self.assertFalse(ok)
        self.assertEqual("Unsupported trigger button", error)

    def test_invalid_rearm_bounds_are_rejected(self) -> None:
        for value in (999, 60001):
            with self.subTest(rearmMs=value):
                ok, error = validate_controller_config_for_web_post(
                    {"trigger": {"mode": "connect", "button": "system", "rearmMs": value}}
                )
                self.assertFalse(ok)
                self.assertEqual("rearmMs must be between 1000 and 60000", error)

    def test_invalid_controller_mac_is_rejected(self) -> None:
        ok, error = validate_controller_config_for_web_post(
            {
                "controllers": [
                    {"macAddress": "not-a-mac", "label": "Bad", "enabled": True}
                ]
            }
        )
        self.assertFalse(ok)
        self.assertEqual("Invalid controller MAC address", error)

    def test_duplicate_controller_mac_is_rejected_after_normalization(self) -> None:
        ok, error = validate_controller_config_for_web_post(
            {
                "controllers": [
                    {"macAddress": "AA-BB-CC-DD-EE-01", "label": "A", "enabled": True},
                    {"macAddress": "aa:bb:cc:dd:ee:01", "label": "B", "enabled": True},
                ]
            }
        )
        self.assertFalse(ok)
        self.assertEqual("Duplicate controller MAC address", error)

    def test_more_than_four_authorized_controllers_is_rejected(self) -> None:
        ok, error = validate_controller_config_for_web_post(
            {
                "controllers": [
                    {"macAddress": f"aa:bb:cc:dd:ee:{idx:02x}", "enabled": True}
                    for idx in range(5)
                ]
            }
        )
        self.assertFalse(ok)
        self.assertEqual("Too many authorized controllers", error)

    def test_documented_payload_examples_stay_in_sync_with_validator(self) -> None:
        examples_path = REPO_ROOT / "tests" / "fixtures" / "controller_config_cases.json"
        cases = json.loads(read_text(examples_path))
        for case in cases:
            with self.subTest(case=case["name"]):
                ok, error = validate_controller_config_for_web_post(case["payload"])
                self.assertEqual(case["valid"], ok)
                if not ok:
                    self.assertEqual(case["error"], error)


class LedStateSchemaTests(unittest.TestCase):
    def test_web_post_accepts_valid_led_state(self) -> None:
        payload = {
            "on": True,
            "bri": 128,
            "channels": [
                {
                    "id": 1,
                    "enabled": True,
                    "pixels": 50,
                    "effect": "rainbow",
                    "color": [0, 217, 255],
                    "secondaryColor": [255, 77, 109],
                    "speed": 75,
                    "reverse": False,
                },
                {
                    "id": 2,
                    "enabled": True,
                    "pixels": 24,
                    "effect": "sequential",
                    "color": [255, 77, 109],
                    "secondaryColor": [0, 255, 162],
                    "speed": 25,
                    "reverse": True,
                },
            ],
        }

        self.assertEqual((True, ""), validate_led_state_for_web_post(payload))

    def test_web_post_accepts_partial_led_state(self) -> None:
        self.assertEqual((True, ""), validate_led_state_for_web_post({"on": False}))

    def test_web_post_accepts_legacy_rgb_state(self) -> None:
        self.assertEqual((True, ""), validate_led_state_for_web_post({"rgb": [0, 217, 255]}))

    def test_invalid_brightness_bounds_are_rejected(self) -> None:
        for value in (-1, 256):
            with self.subTest(bri=value):
                ok, error = validate_led_state_for_web_post({"bri": value})
                self.assertFalse(ok)
                self.assertEqual("bri must be between 0 and 255", error)

    def test_invalid_rgb_shape_is_rejected(self) -> None:
        ok, error = validate_led_state_for_web_post({"rgb": [0, 217]})

        self.assertFalse(ok)
        self.assertEqual("rgb must contain three values", error)

    def test_invalid_rgb_bounds_are_rejected(self) -> None:
        ok, error = validate_led_state_for_web_post({"rgb": [0, 217, 256]})

        self.assertFalse(ok)
        self.assertEqual("rgb values must be between 0 and 255", error)

    def test_invalid_channel_id_is_rejected(self) -> None:
        ok, error = validate_led_state_for_web_post({"channels": [{"id": 3}]})

        self.assertFalse(ok)
        self.assertEqual("invalid LED channel id", error)

    def test_invalid_channels_shape_is_rejected(self) -> None:
        ok, error = validate_led_state_for_web_post({"channels": {"id": 1}})

        self.assertFalse(ok)
        self.assertEqual("invalid LED channels", error)

    def test_invalid_channel_pixel_count_is_rejected(self) -> None:
        ok, error = validate_led_state_for_web_post({"channels": [{"id": 1, "pixels": 51}]})

        self.assertFalse(ok)
        self.assertEqual("pixels must be between 0 and 50", error)

    def test_invalid_channel_effect_is_rejected(self) -> None:
        ok, error = validate_led_state_for_web_post({"channels": [{"id": 1, "effect": "sparkles"}]})

        self.assertFalse(ok)
        self.assertEqual("unsupported LED effect", error)

    def test_invalid_channel_color_is_rejected(self) -> None:
        ok, error = validate_led_state_for_web_post({"channels": [{"id": 1, "color": [0, 217]}]})

        self.assertFalse(ok)
        self.assertEqual("color must contain three values", error)

    def test_invalid_channel_speed_is_rejected(self) -> None:
        ok, error = validate_led_state_for_web_post({"channels": [{"id": 1, "speed": 0}]})

        self.assertFalse(ok)
        self.assertEqual("speed must be between 1 and 100", error)


if __name__ == "__main__":
    unittest.main()
