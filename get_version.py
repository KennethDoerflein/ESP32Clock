# extra_script.py
import subprocess
from datetime import datetime
import os

def generate_dev_version(now=None):
    """Generate a development version string based on current timestamp."""
    if now is None:
        now = datetime.now()
    return now.strftime("dev-%Y%m%d-%H%M%S")

def format_version_header(version_str):
    """Generate the C header content defining FIRMWARE_VERSION."""
    return f"""#pragma once

#define FIRMWARE_VERSION "{version_str}"
"""

def get_git_tag_version():
    """Retrieve the exact git tag for the current commit, or None if not on a tag."""
    return subprocess.check_output(["git", "describe", "--tags", "--exact-match"]).strip().decode("utf-8")

def run_script(pio_env=None):
    # Check if the 'CI' environment variable is set to 'true'
    is_ci = os.getenv("CI", "false").lower() == "true"
    version = ""

    if is_ci:
        print("CI environment detected. Getting version from git tag.")
        try:
            version = get_git_tag_version()
        except subprocess.CalledProcessError as e:
            print(f"Error: Not on a git tag. 'git describe' failed: {e}")
            print("CI builds must be performed on a commit with a version tag.")
            if pio_env:
                pio_env.Exit(1)
            return
        except FileNotFoundError:
            print("Error: git command not found. Is git installed and in the PATH?")
            if pio_env:
                pio_env.Exit(1)
            return
    else:
        print("Local build detected. Creating dev version string.")
        version = generate_dev_version()

    header_content = format_version_header(version)

    include_dir = "include"
    if pio_env:
        include_dir = pio_env.get("PROJECT_INCLUDE_DIR", "include")
    version_file_path = os.path.join(include_dir, "version.h")

    if not os.path.isdir(include_dir):
        os.makedirs(include_dir)

    with open(version_file_path, "w") as f:
        f.write(header_content)

    print(f"Generated {version_file_path} with FIRMWARE_VERSION: {version}")

# PlatformIO extra_script execution hook
try:
    Import("env")
    run_script(env)
except NameError:
    pass


