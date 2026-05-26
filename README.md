# G3 SHU Bluetooth Fix Tool

> Private/test-use utility for full 128 KB Segway/Ninebot G3 VCU dumps.

This tool searches a full `MEMORY_G3.bin` VCU dump for the dynamic SHU Bluetooth security key area and creates a patched copy where the detected key occurrences are replaced with `FF`.

It does **not** flash anything by itself.

## Credits

Method credit goes to **sharkboy**.

This tool is intended to be used together with the ST-Link dump/flash workflow from sharkboy's repository:

https://github.com/Sharkboy-j/ninebot-g3-max-vcu-speed-hack/tree/main

## Disclaimer

This project is provided for testing, research, and private-use purposes only.

Use it only on hardware you own and understand. Modifying VCU firmware can cause device malfunction, unstable behavior, or a bricked controller. You are responsible for checking and following local laws, safety rules, and road-use requirements. Do not use modified firmware on public roads where it is not legal.

The author of this tool is not responsible for damage, data loss, warranty loss, or unsafe device behavior.

## What this tool does

The tool:

1. Opens a file picker so the user can select a full 128 KB `MEMORY_G3.bin` dump.
2. Verifies the selected file is exactly `131072` bytes.
3. Searches the dump for the marker:

   ```text
   SCOOTER_VCU_xxG3
   ```

4. Scans shortly after that marker for a dynamic alphanumeric key-like token.
5. Verifies the same key appears exactly twice in the dump.
6. Replaces both key occurrences with `FF`.
7. Verifies that only those two ranges changed.
8. Writes a new patched output file.
9. Writes a report into the local `logs` folder.
10. Creates a backup of the original dump in the local `backups` folder.

The tool does **not** modify the selected input file directly.

## Requirements

### Hardware

- ST-Link V2 compatible programmer
- Dupont/jumper wires
- Access to the G3 VCU board
- Stable 3.3 V, SWDIO, SWCLK, and GND connection

### Software

- Windows
- Sharkboy's G3 VCU ST-Link tool/repo for dumping/flashing:
  - https://github.com/Sharkboy-j/ninebot-g3-max-vcu-speed-hack/tree/main

Recommended optional tools:

- HxD or another hex editor
- A file comparison tool

## Important safety notes

Before flashing anything:

- Create a dump.
- Create a second dump.
- Compare both dumps byte-for-byte.
- Keep the original dump safe.
- Only flash full 128 KB images.
- Confirm the patched output is still exactly `131072` bytes.
- Confirm only the two expected key ranges changed.

## Step-by-step usage

### 1. Create a full VCU dump

Use sharkboy's ST-Link workflow to create a full dump:

```text
MEMORY_G3.bin
```

The file must be exactly:

```text
131072 bytes
```

### 2. Verify your dump

Create a second dump and compare it with the first one.

If the two dumps are not identical, do not continue. Fix your ST-Link/contact setup and dump again.

### 3. Start this tool

Run:

```text
ryze_shu_bluetooth_fix.exe
```

### 4. Select menu option 1

Choose:

```text
[1] Patch MEMORY_G3.bin for SHU Bluetooth fix
```

### 5. Select your dump

A Windows file picker opens.

Select your full 128 KB:

```text
MEMORY_G3.bin
```

### 6. Let the tool detect the key

The tool searches for:

```text
SCOOTER_VCU_xxG3
```

Then it scans shortly after that marker for the dynamic key and verifies that the exact same key appears twice in the full dump.

If the key is not found exactly twice, the tool stops.

### 7. Confirm the patch

If the detection looks correct, type:

```text
PATCH
```

The tool creates:

- a backup in `backups/`
- a patched output file next to your dump
- a report in `logs/`

### 8. Verify the output manually

Open the output file in HxD or another hex editor.

Check:

- the file is still exactly `131072` bytes (128 KB)
- the two key locations are filled with `FF`
- the original dump backup still exists

### 9. Optional: copy to flashing filename

The tool can optionally copy the output to:

```text
MEMORY_G3.bin.patched.bin
```

next to the selected dump.

Only do this if you are ready to use it with your ST-Link flashing script.

### 10. Flash using the ST-Link workflow

Use the normal ST-Link flashing workflow from sharkboy's repo.

This tool does not flash anything.

## Output files

The tool creates folders next to the executable:

```text
backups/
logs/
```

Example files:

```text
backups/MEMORY_G3_before_shu_bluetooth_fix_YYYYMMDD_HHMMSS.bin
logs/shu_bluetooth_fix_report_YYYYMMDD_HHMMSS.txt
```

The patched output is saved next to the selected dump by default:

```text
MEMORY_G3_shu_bluetooth_fix.bin
```

## Failure behavior

It stops if:

- no input file is selected
- the file is not exactly 128 KB
- the marker is missing
- no valid key candidate is found
- the key does not appear exactly twice
- the output changes more than the expected two ranges

## License / publishing note

Credit the method to sharkboy and link to the original ST-Link workflow repository.
