"""Import required board support files from the archive linked in README.md."""

import argparse
from pathlib import Path, PurePosixPath
from zipfile import BadZipFile, ZipFile

ROOT = Path(__file__).resolve().parents[1]
DIRECTORIES = {"Library", "bootloader", "svd"}
REQUIRED = {
    "Library/SPL/MDR32VF0xI_config.h",
    "Library/SPL/inc/MDR32VF0xI_adcui.h",
    "Library/CMSIS/DeviceSupport/src/gcc/MDR1206/startup_MDR1206.S",
    "bootloader/MDR1206_Bootloader.hex",
    "svd/MDR1206FI.svd",
}


def import_sdk(archive_path):
    with ZipFile(archive_path) as archive:
        files = []
        for member in archive.infolist():
            path = PurePosixPath(member.filename)
            if not path.parts or path.parts[0] not in DIRECTORIES or member.is_dir():
                continue
            if ".." in path.parts or "\\" in member.filename or ":" in member.filename:
                raise ValueError(f"Unsafe archive member: {member.filename}")
            files.append(member)

        missing = REQUIRED.difference(member.filename for member in files)
        if missing:
            raise ValueError("Files missing from the Milandr archive: " + ", ".join(sorted(missing)))

        # Read all archive entries before writing so a corrupt input leaves the tree intact.
        payload = [(ROOT / member.filename, archive.read(member)) for member in files]
        for destination, contents in payload:
            if destination.exists() and destination.read_bytes() != contents:
                raise ValueError(f"Existing file differs: {destination.relative_to(ROOT)}")

        for destination, contents in payload:
            if not destination.exists():
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes(contents)

    return len(files)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path, help="Original example ZIP from the Milandr support portal")
    args = parser.parse_args()
    try:
        count = import_sdk(args.archive)
    except (BadZipFile, OSError, ValueError) as error:
        parser.exit(1, f"{error}\n")
    print(f"Ready: {count} files in Library/, bootloader/ and svd/.")
