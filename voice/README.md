# Voice module materials

| File | Meaning |
|---|---|
| `command_template_original.xlsx` | Original generator-format workbook, preserved byte for byte |
| `commands_english.csv` | Readable English export of all 17 command/wake phrases, responses, semantic tags and protocol frames |

The workbook contains vendor-format Chinese headers and an instruction sheet. It is kept unchanged rather than translating a machine-consumed template. The English CSV is a reading companion, not a replacement generator input. Recognition phrases themselves are already English.

## Interface and mapping boundary

The supplied Mega source reads one result byte from **I²C address `0x34`, register 100**. The supplied binary and spreadsheet describe **UART frames**. Their compatibility with that I²C interface is not established by the source package; a matching module configuration or bridge must be verified on the actual hardware.

The original `handleVoiceCommand()` maps indices 1–6 to auto-on **except index 3**, which is intercepted by an earlier no-operation branch; 7–9 disable auto; 10–11 enable UV; 12 disables UV; 14 requests material change. Indices 13 and 15 have no action branch. The spreadsheet has UART entries through `0F` and includes duplicate semantic labels with differing frames. UART frame bytes, semantic labels and I²C result indices must not be assumed interchangeable.

No binary was executed, reverse-engineered or flashed. No voice accuracy or end-to-end interface validation is claimed. Original module identifiers and recognition data are preserved for future integration.

## Binary retained locally

The supplied `CI1302_Eng_V00627_UART1_115200_2M.bin` (1,534,491 bytes) is kept in the owner’s local archive pending confirmation of redistribution permission. Its implementation source and redistribution terms were not supplied. It is not included in this public repository or the release.

SHA-256: `4d4914686e90fca27c3cb96bcc120e4e496ad37f51b5076af3ef0b656eaca18f`. The English command table and original generator workbook remain available for reviewing the intended interface.
