# Contributing to InertialSaber OS

Thank you for your interest in contributing. This document describes the minimum you need to build the firmware and submit a change.

## Building

InertialSaber OS targets the ESP32-S3 exclusively and uses ESP-IDF v6.1.

```bash
idf.py set-target esp32s3
idf.py build
```

## Branches and pull requests

- Create a branch with the `feature/` prefix, lowercase with underscores (for example `feature/profile_switch`).
- Open pull requests against `main`. Do not commit directly to `main`.
- Keep commits focused and describe the change in English.
- Fill in the pull request template.

## Formatting

C++ code is formatted with clang-format 21.1.3 using the `.clang-format` file in the repository root. This is the `clang-format` of the `esp-clang` tool of ESP-IDF v6.1 (`esp-21.1.3_20260408`); install it with `python $IDF_PATH/tools/idf_tools.py install esp-clang`. Older releases reject the configuration and other releases may format differently. CI checks every C++ file under `main/` with `clang-format --dry-run --Werror`, so run it on every file you change before opening a pull request.

## License headers

Every C++ source and header file under `main/` and every CMake file of the project starts with an SPDX license identifier, followed by an empty line:

- C++: `// SPDX-License-Identifier: GPL-3.0-or-later`
- CMake: `# SPDX-License-Identifier: GPL-3.0-or-later`

Add it as the first line of any new file. It does not apply to `components/` (see below).

## The `components/` directory

`components/` is vendored from an upstream repository. Do not modify it in this repository. If a change is needed there, make it upstream; it will reach this repository through a component sync.

## Reporting problems

- Bugs and feature requests: open a GitHub issue using the matching template.
- Security issues: do not open a public issue; see [SECURITY.md](SECURITY.md).

By participating you agree to follow the [Code of Conduct](CODE_OF_CONDUCT.md).
