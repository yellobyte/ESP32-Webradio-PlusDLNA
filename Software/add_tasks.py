from os.path import expanduser
Import("env")
env.AddCustomTarget(
    name="envdump",
    dependencies=None,
    actions=[
        "pio run -t envdump",
    ],
    title="Show Build Options [envdump]",
    description="Show Build Options "
)
env.AddCustomTarget(
    name="prune",
    dependencies=None,
    actions=[
        "pio system prune -f",
    ],
    title="Prune System",
    description="Prune System"
)
#cmd1 = "python.exe " + expanduser("~") + "/.platformio/packages/tool-esptoolpy/espefuse.py --port $UPLOAD_PORT summary"
cmd1 = "python.exe " + "$PROJECT_PACKAGES_DIR/tool-esptoolpy/espefuse.py --port $UPLOAD_PORT summary"
env.AddCustomTarget(
    name="fuses-summary",
    dependencies=None,
    actions=[
        cmd1
    ],
    title="Fuses Summary",
    description="Fuses Summary"
)
cmd2 = "$PROJECT_PACKAGES_DIR/toolchain-xtensa-esp-elf/bin/xtensa-esp-elf-objdump.exe -S -C -d"
env.AddCustomTarget(
    name="objdump",
    dependencies=None,
    actions=[
      cmd2 + " \"$BUILD_DIR\"/src/main.cpp.o > \"$BUILD_DIR\"/src/main.obj.log"
    ],
    title="dbg: objdump",
    description="dbg: objdump"
)
cmd3 = "$PROJECT_PACKAGES_DIR/toolchain-xtensa-esp-elf/bin/xtensa-esp-elf-addr2line.exe -pfiaC -e"
env.AddCustomTarget(
    name="addr2line",
    dependencies=None,
    actions=[
        cmd3 + " \"$BUILD_DIR\"/firmware.elf 0x400d17A0"
    ],
    title="dbg: addr2line",
    description="dbg: addr2line"
)
