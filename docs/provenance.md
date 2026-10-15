# Provenance and preparation changes

## Source coverage

All materials come from the supplied `MEC202 Demo.zip`, except the separately supplied certificate, which is not published. The archive contains one Mega firmware project, one ESP8266 project with frontend assets, a serial mock script, documentation, one presentation/video, one PCB project, 46 mechanical files and voice-module materials.

There are no competing source-code versions requiring a guessed “latest” selection. Mechanical variants with similar names/sizes have different hashes and are all preserved. The original assembly directory and native filenames are retained.

## Repository organization

| Original area | Repository location |
|---|---|
| `ArduinoMegaCode` | `firmware/mega2560` |
| `ESP8266Code/esp8266` source/config | `firmware/esp8266` |
| ESP8266 `data/` | Single shared `web/` directory via `data_dir` |
| `ESP8266Code/mock.py` | `tools/mock_serial.py` |
| `3DprintModels` | `hardware/mechanical` |
| `JLCpcbDesign.epro2` | `hardware/pcb/dehumidifier.epro2` |
| `VoiceRecognitionModuleFirmware` | `voice/` plus an English CSV companion |
| Demonstration slides/video | Anonymized presentation and separately packaged original video |

The raw source ZIP remains unchanged locally. An owner-only file disposition list records every original entry and its destination or reason for exclusion; it is not added to the public project.

## Changes during preparation

- Reorganized paths and added root/module reading guides in English.
- Kept the frontend only once, with a PlatformIO filesystem path pointing to it.
- Replaced original access-point values with explicit demo defaults and an ignored local configuration template.
- Converted the ESP upload-speed inline comment into valid INI syntax while retaining the original baud rate.
- Translated the remaining mock/config comments and one mixed-language diagnostic.
- Added `--port` / `--baud` options to the original serial mock and activated its previously unused 60-second IAQ warm-up transition. The generated numeric value is synthetic.
- Added a localhost-only synthetic dashboard preview and four host checks for supplied command/reference behavior.
- Preserved the voice XLSX and hardware files unchanged; retained the voice binary locally pending redistribution permission. Exported the voice table to readable English CSV without modifying the generator template.
- Removed names/student identifiers from editable presentation text and replaced the contribution-name column with work areas. Corrected text overlap, adsorption terminology and unsupported efficacy wording; clarified that voice integration remains unverified. No individual contribution claim was added.

No PID tuning, pin reassignment, relay control-law change or binary patching was performed. Existing voice-index, thermal-failure, target-persistence and optimistic-state limitations are documented rather than silently redesigned without hardware evidence.

## Exclusions

Original PlatformIO scaffold `include/lib/test` README files are generic boilerplate and replaced by relevant module guides. The old top-level/module README files are consolidated into the new English documentation; the PDF rendition of the old root README is redundant. The original deck containing names/student identifiers remains local and is replaced by the anonymized technical copy.

No certificate image, participant roster, original local AP credentials, dependency/build cache or machine-specific source root containing student identifiers is published. The voice binary remains in the local source archive pending redistribution permission. Native CAD references and generator-format fields are retained for compatibility.
