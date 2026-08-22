# Stamps every build with a version string, generated fresh each time.
#
# The timestamp - not the git hash - is what makes the stamp unique here. This
# project is flashed straight from a working tree that is essentially always
# dirty, so `git describe --dirty` returns an identical string for every build
# between commits and cannot answer "did that OTA actually take?". The hash is
# kept alongside for provenance.
#
# The header is written into the build directory rather than src/, so the source
# tree stays clean and only Version.cpp recompiles when the stamp changes.

Import("env")

import datetime
import os
import subprocess


def git(*args):
    try:
        return subprocess.check_output(
            ["git", *args], stderr=subprocess.DEVNULL
        ).decode().strip()
    except Exception:
        # No git, no checkout, or a tarball export. A build stamp is worth
        # having anyway, so fall back rather than failing the build.
        return ""


revision = git("describe", "--tags", "--always", "--dirty") or "nogit"
built_at = datetime.datetime.now().astimezone().strftime("%Y-%m-%dT%H:%M:%S%z")
version = "{} {}".format(revision, built_at)

out_dir = os.path.join(env.subst("$BUILD_DIR"), "generated")
os.makedirs(out_dir, exist_ok=True)

# Deliberately rewritten every build: a changing timestamp is the whole point,
# and it is what makes SCons recompile Version.cpp.
with open(os.path.join(out_dir, "build_version.h"), "w") as handle:
    handle.write('#pragma once\n#define FIRMWARE_VERSION "{}"\n'.format(version))

env.Append(CPPPATH=[out_dir])
print("firmware version: {}".format(version))
