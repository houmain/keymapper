#!/usr/bin/env python3

import copy
import json
import re

README_FILES = [
    "README.md",
]

VARIABLE_JSON_FILES = [
    "src/libs/Karabiner-DriverKit-VirtualHIDDevice/version.json",
    "src/libs/Karabiner-DriverKit-VirtualHIDDevice/elements-version.json",
]

REPLACEMENTS = {
    "package_version": r"\d+\.\d+\.\d+",
    "elements_version": r"\d+\.\d+\.\d+",
}

TAG_REGEX_FORMAT = r"(<!--\s*replace:\s*{name}\s*-->)(`[^`]+`|\[[^]]+\]\([^)]+\))"

variables = {}
for var_file in VARIABLE_JSON_FILES:
    with open(var_file) as f:
        variables.update(json.load(f))

for readme_file in README_FILES:
    with open(readme_file) as f:
        content = f.read()
    original_content = copy.copy(content)

    for var_name, var_pattern in REPLACEMENTS.items():
        pattern = TAG_REGEX_FORMAT.format(name=re.escape(var_name))
        def replacer(m: re.Match) -> str:
            tag = m[1]
            tag_content = m[2]
            try:
                 tag_content = re.sub(var_pattern, variables[var_name], tag_content)
            except KeyError as e:
                raise ValueError(f"REPLACEMENTS variable {var_name} does not appear in any of the JSON files: {VARIABLE_JSON_FILES!r}") from e
            return tag + tag_content

        content = re.sub(pattern, replacer, content)

    if content != original_content:
        with open(readme_file, "w") as f:
            f.write(content)
