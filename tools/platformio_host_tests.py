Import("env")

env.AddCustomTarget(
    "hosttests",
    None,
    "python3 tools/run_host_tests.py",
    title="Run host-side tests",
    description="Run deterministic tests that do not require ESP32 hardware.",
    always_build=True,
)
