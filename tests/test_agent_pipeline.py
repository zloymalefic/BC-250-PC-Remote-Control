#!/usr/bin/env python3
"""Host-side checks for the Codex agent task pipeline configuration.

These tests validate repository instructions and agent configuration only. They
do not require ESP32 hardware, network access, firmware flashing, LittleFS, or
relay operation.
"""

from __future__ import annotations

import tomllib
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
CODEX_DIR = REPO_ROOT / ".codex"
AGENTS_DIR = CODEX_DIR / "agents"
AGENTS_MD = REPO_ROOT / "AGENTS.md"
ARDUINO_CLI_CONFIG = REPO_ROOT / "arduino-cli.yaml"
PLATFORMIO_CONFIG = REPO_ROOT / "platformio.ini"

PIPELINE_PHASES = (
    "task creation",
    "planning",
    "implementation",
    "validation",
    "final testing",
)

PHASE_RUN_MODES = (
    "only task creation",
    "only planning",
    "only implementation",
    "only validation",
    "only testing",
)

PHASED_AGENT_KEYS = (
    "power_firmware",
    "platform_connectivity",
    "web_contract",
    "test_engineer",
    "coverage_engineer",
    "security_tester",
)


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def load_toml(path: Path) -> dict:
    return tomllib.loads(read_text(path))


class AgentPipelineConfigTests(unittest.TestCase):
    def test_all_codex_toml_files_parse(self) -> None:
        for path in sorted(CODEX_DIR.glob("**/*.toml")):
            with self.subTest(path=path.relative_to(REPO_ROOT)):
                self.assertIsInstance(load_toml(path), dict)

    def test_config_registers_task_orchestrator_and_referenced_agent_files_exist(self) -> None:
        config = load_toml(CODEX_DIR / "config.toml")
        agents = config["agents"]

        self.assertIn("task_orchestrator", agents)
        self.assertEqual(
            "agents/task-orchestrator.toml",
            agents["task_orchestrator"]["config_file"],
        )

        for agent_key, agent_config in agents.items():
            if not isinstance(agent_config, dict) or "config_file" not in agent_config:
                continue
            with self.subTest(agent=agent_key):
                agent_path = CODEX_DIR / agent_config["config_file"]
                self.assertTrue(
                    agent_path.is_file(),
                    f"{agent_key} references missing config {agent_path}",
                )

    def test_agents_md_documents_phase_order_and_independent_phase_runs(self) -> None:
        text = read_text(AGENTS_MD)

        expected_order = (
            "task creation, planning, implementation with tests for new code, "
            "validation, and final testing"
        )
        self.assertIn(expected_order, text)

        self.assertIn("Use `task_orchestrator`", text)
        self.assertIn("Delegate task creation and phase coordination", text)
        for mode in PHASE_RUN_MODES:
            with self.subTest(mode=mode):
                self.assertIn(mode, text)

    def test_task_orchestrator_requires_full_pipeline_and_single_phase_execution(self) -> None:
        text = read_text(AGENTS_DIR / "task-orchestrator.toml")

        for phrase in (
            "create an explicit task record before planning or editing",
            "Run the default workflow in this order",
            "Support independent phase execution",
            "implementation with tests for new code",
            "Hardware checks must remain explicit and unclaimed",
            "passed, failed, skipped, and untestable",
        ):
            with self.subTest(phrase=phrase):
                self.assertIn(phrase, text)

        for phase in PIPELINE_PHASES:
            with self.subTest(phase=phase):
                self.assertIn(phase, text)

    def test_domain_agents_participate_in_phased_pipeline(self) -> None:
        config = load_toml(CODEX_DIR / "config.toml")

        for agent_key in PHASED_AGENT_KEYS:
            with self.subTest(agent=agent_key):
                agent_path = CODEX_DIR / config["agents"][agent_key]["config_file"]
                text = read_text(agent_path)
                self.assertIn("phased task pipeline", text)
                self.assertIn("task creation and planning", text)
                self.assertIn("validation and final testing", text)

    def test_review_and_test_agents_enforce_new_code_tests(self) -> None:
        test_engineer = read_text(AGENTS_DIR / "test-engineer.toml")
        coverage_engineer = read_text(AGENTS_DIR / "coverage-engineer.toml")
        code_reviewer = read_text(AGENTS_DIR / "code-reviewer.toml")

        self.assertIn("add or update deterministic tests for new code", test_engineer)
        self.assertIn("tests for new behavior before validation starts", coverage_engineer)
        self.assertIn("implementation includes tests for new code", code_reviewer)

    def test_build_tool_configs_document_bluepad32_cli_path(self) -> None:
        arduino_cli = read_text(ARDUINO_CLI_CONFIG)
        platformio = read_text(PLATFORMIO_CONFIG)
        readme = read_text(REPO_ROOT / "README.md")

        self.assertIn("package_esp32_index.json", arduino_cli)
        self.assertIn("package_esp32_bluepad32_index.json", arduino_cli)
        self.assertIn("host-tests", platformio)
        self.assertIn("firmware-arduino-cli", platformio)
        self.assertIn("tools/setup_arduino_cli.sh", readme)
        self.assertIn("tools/build_firmware.py compile", readme)


if __name__ == "__main__":
    unittest.main()
