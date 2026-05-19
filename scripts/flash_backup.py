from pathlib import Path
import os
import subprocess
import sys

Import("env")

FLASH_START = "0x0"
FLASH_SIZE = "0x1000000"  # 16 MB
BACKUP_PATH = Path("build") / "esp32-s3-flash-backup.bin"


def _esptool_command(*args):
    tool_dir = Path(env.PioPlatform().get_package_dir("tool-esptoolpy"))
    esptool_py = tool_dir / "esptool.py"
    contrib_dir = tool_dir / "_contrib"
    python_path = f"{tool_dir}:{contrib_dir}"

    upload_port = env.subst("$UPLOAD_PORT")
    if not upload_port:
        upload_port = env.GetProjectOption("upload_port")

    return (
        [sys.executable, str(esptool_py), "--chip", "esp32s3", "--port", upload_port, "--baud", "921600"]
        + list(args),
        python_path,
    )


def backup_flash(*_args, **_kwargs):
    BACKUP_PATH.parent.mkdir(parents=True, exist_ok=True)
    command, python_path = _esptool_command("read_flash", FLASH_START, FLASH_SIZE, str(BACKUP_PATH))
    print(f"Backing up ESP32-S3 flash to {BACKUP_PATH}")
    process_env = {**os.environ, "PYTHONPATH": python_path}
    subprocess.check_call(command, env=process_env)


env.AddCustomTarget(
    name="backup_flash",
    dependencies=None,
    actions=[backup_flash],
    title="Backup ESP32-S3 Flash",
    description=f"Read full 16 MB flash into {BACKUP_PATH}",
)
