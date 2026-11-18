# Repository Guidelines

## Project Structure

This repository contains the Linux 5.4 Android kernel sources for Samsung SM7325 devices.
## Build and Development

Build through the workspace GitHub Actions workflow `build-sm7325`(/home/zhang/Workspaces/Kosiou-kernel-workspace/Kosiou-kernel-ci). Select the feature branch, tag, or commit under test as `kernel_ref`.

Before submitting changes, run `scripts/checkpatch.pl --strict <patch-file>` and `git diff --check` from the kernel repository root. Run relevant kernel selftests from `tools/testing/selftests/` when applicable, then validate the resulting image on the target device and record the runtime logs.

## Code and Configuration Style

Follow the existing Linux kernel C style: tabs for indentation, kernel naming conventions, and small, focused changes. Keep device-specific configuration in the relevant vendor defconfig. Preserve Samsung and Android vendor hooks when changing upstream code. Add comments only where behavior or compatibility constraints are not evident from the code.

## Testing Guidelines

There is no single repository-wide test command. Use the cloud build result, run targeted selftests when applicable, then validate boot and affected hardware or features on the device. Record the kernel configuration, workflow result, and relevant runtime logs with test feedback.

## Commits and Pull Requests

Use a short scope prefix such as `fix:`, `feat:`, `mm:`, or `config:` followed by a clear summary. In chinese
