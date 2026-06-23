Import("env")

env.AddCustomTarget(
    "firmware",
    None,
    "python3 tools/build_firmware.py compile",
    title="Build firmware with Arduino CLI",
    description="Compile the BC-250 firmware with the Bluepad32 Arduino board package.",
    always_build=True,
)
