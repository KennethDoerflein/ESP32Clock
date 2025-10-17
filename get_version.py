# extra_script.py
import subprocess
from datetime import datetime
import os

# Import the build environment from PlatformIO globals
Import("env")

# Check if the 'CI' environment variable is set to 'true'
# This is a common practice in CI/CD systems like GitHub Actions, GitLab CI, etc.
is_ci = os.getenv("CI", "false").lower() == "true"

version = ""

if is_ci:
    # We are in a CI environment, so we expect a git tag for the version.
    print("CI environment detected. Getting version from git tag.")
    try:
        # Use 'git describe' to get the most recent tag.
        # '--tags' ensures we are looking at tags.
        # '--exact-match' will cause the command to fail if the current commit HEAD
        # is not a tagged commit. This prevents building from intermediate commits.
        version = subprocess.check_output(["git", "describe", "--tags", "--exact-match"]).strip().decode("utf-8")
    except subprocess.CalledProcessError as e:
        # This error occurs if 'git describe' returns a non-zero exit code,
        # which happens when '--exact-match' fails.
        print(f"Error: Not on a git tag. 'git describe' failed: {e}")
        print("CI builds must be performed on a commit with a version tag.")
        # Exit with a non-zero status code to fail the PlatformIO build.
        env.Exit(1)
    except FileNotFoundError:
        # This error occurs if the 'git' command itself is not found.
        print("Error: git command not found. Is git installed and in the PATH?")
        env.Exit(1)
else:
    # We are in a local development environment.
    # Create a unique version string with a 'dev-' prefix and a timestamp.
    print("Local build detected. Creating dev version string.")
    now = datetime.now()
    version = now.strftime("dev-%Y%m%d-%H%M%S") # e.g., "dev-20251017-023223"

# Define the content for the version.h file
header_content = f"""#pragma once

#define FIRMWARE_VERSION "{version}"
"""

# Get the project's include directory from the PlatformIO environment
include_dir = env.get("PROJECT_INCLUDE_DIR", "include")
version_file_path = os.path.join(include_dir, "version.h")

# Ensure the include directory exists
if not os.path.isdir(include_dir):
    os.makedirs(include_dir)

# Write the content to the version.h file
with open(version_file_path, "w") as f:
    f.write(header_content)

print(f"Generated {version_file_path} with FIRMWARE_VERSION: {version}")

