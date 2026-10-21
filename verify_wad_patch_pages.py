#!/usr/bin/env python3

import argparse
import struct
import sys

PAGE_SIZE = 0x80000


def u16_be(data, offset):
    return struct.unpack_from(">H", data, offset)[0]


def u32_be(data, offset):
    return struct.unpack_from(">I", data, offset)[0]


def page_set(abs_offset, size, base_offset):
    if size <= 0:
        return {(base_offset + abs_offset) // PAGE_SIZE}
    start_page = (base_offset + abs_offset) // PAGE_SIZE
    end_page = (base_offset + abs_offset + size - 1) // PAGE_SIZE
    return set(range(start_page, end_page + 1))


def parse_wad(path):
    with open(path, "rb") as fh:
        data = fh.read()

    if len(data) < 12:
        raise ValueError(f"{path!r} is too short to be a valid WAD.")

    magic = data[:4]
    if magic not in (b"IWAD", b"PWAD"):
        raise ValueError(f"{path!r} does not look like a WAD file (magic={magic!r}).")

    num_files = u32_be(data, 4)
    dir_offset = u32_be(data, 8)
    if dir_offset + num_files * 16 > len(data):
        raise ValueError("The WAD directory is out of range.")

    entries = []
    for i in range(num_files):
        pos = dir_offset + i * 16
        offset = u32_be(data, pos)
        size = u32_be(data, pos + 4)
        name = data[pos + 8:pos + 16].rstrip(b"\0").decode("latin1")
        entries.append({"offset": offset, "size": size, "name": name})
    return data, entries


def parse_poffsets(blob):
    if len(blob) % 6 != 0:
        raise ValueError(f"POFFSETS length is not a multiple of 6 bytes ({len(blob)}).")

    count = len(blob) // 6
    offsets_end = count * 4
    values = []
    for i in range(count):
        pixel_offset = u32_be(blob, i * 4)
        pixel_len = u16_be(blob, offsets_end + i * 2)
        values.append((pixel_offset, pixel_len))
    return values


def iter_sprite_patches(entries):
    in_sprite = False
    for entry in entries:
        name = entry["name"]
        if name == "S_START":
            in_sprite = True
            continue
        if name == "S_END":
            in_sprite = False
            continue
        if in_sprite and name not in ("", "."):
            yield entry


def allowed_pages_for(patch_page):
    if patch_page <= 5:
        return set(range(0, 6))
    return set(range(0, 6)) | {patch_page}


def validate_pages(label, abs_offset, size, patch_page, base_offset, errors):
    if size <= 0:
        return
    pages = page_set(abs_offset, size, base_offset)
    if patch_page <= 5:
        bad = pages - set(range(0, 6))
        if bad:
            errors.append(f"{label} uses pages {sorted(pages)} but patch page {patch_page} is <= 5.")
    else:
        bad = pages - {patch_page}
        if bad:
            errors.append(f"{label} uses pages {sorted(pages)} but patch page {patch_page} is > 5; it must stay on page {patch_page}.")


def validate_pixel_pages(label, abs_offset, size, patch_page, base_offset, errors):
    if size <= 0:
        return
    pages = page_set(abs_offset, size, base_offset)
    allowed = set(range(0, 6))
    if patch_page > 5:
        allowed.add(patch_page)
    bad = pages - allowed
    if bad:
        errors.append(f"{label} uses pages {sorted(pages)}; allowed pages are {sorted(allowed)}.")


def parse_patch_layout(patch_blob):
    if len(patch_blob) < 6:
        return None

    width = u16_be(patch_blob, 0)
    layouts = []
    for header_size, optimized in ((6, True), (8, False)):
        table_end = header_size + width * 2
        if table_end > len(patch_blob):
            continue
        column_words = [u16_be(patch_blob, header_size + i * 2) for i in range(width)]
        offsets = [
            (word & 0x7FFF) if optimized and word & 0x8000 else word
            for word in column_words
        ]
        if all(offset >= table_end and offset < len(patch_blob) for offset in offsets):
            starts_at_table = not offsets or offsets[0] == table_end
            layouts.append((starts_at_table, header_size, optimized, width, column_words))

    if not layouts:
        return None
    layouts.sort(key=lambda layout: layout[0], reverse=True)
    _, header_size, optimized, width, column_words = layouts[0]
    return header_size, optimized, width, column_words


def validate_patch(wad_data, patch_entry, pixel_offset, pixel_len, patch_page, base_offset, errors):
    patch_offset = patch_entry["offset"]
    patch_size = patch_entry["size"]
    patch_name = patch_entry["name"]

    validate_pages(f"Patch data for {patch_name!r}", patch_offset, patch_size, patch_page, base_offset, errors)
    validate_pixel_pages(f"Pixel data for {patch_name!r}", pixel_offset, pixel_len, patch_page, base_offset, errors)

    patch_blob = wad_data[patch_offset:patch_offset + patch_size]
    layout = parse_patch_layout(patch_blob)
    if layout is None:
        errors.append(f"Patch {patch_name!r} has an invalid or truncated patch header/column table.")
        return

    _, optimized, width, column_words = layout
    for col_index, column_word in enumerate(column_words):
        inline = optimized and bool(column_word & 0x8000)
        col_offset = column_word & 0x7FFF if inline else column_word
        if col_offset >= len(patch_blob):
            errors.append(f"Patch {patch_name!r} column {col_index} starts outside its patch data at offset {col_offset:#x}.")
            continue

        if inline:
            td = patch_blob[col_offset]
            if td == 0xFF:
                continue
            if col_offset + 2 > len(patch_blob):
                errors.append(f"Patch {patch_name!r} inline column {col_index} is truncated before its post header.")
                continue
            length_byte = patch_blob[col_offset + 1]
            if not (length_byte & 0x80):
                errors.append(f"Patch {patch_name!r} inline column {col_index} has an invalid inline-post header.")
                continue
            pixel_len_inline = (length_byte & 0x7F) + 1
            inline_pixel_start = col_offset + 2
            if inline_pixel_start + pixel_len_inline > len(patch_blob):
                errors.append(
                    f"Patch {patch_name!r} inline column {col_index} pixels extend past the end of the patch data."
                )
                continue
            validate_pages(
                f"Inline pixel data for {patch_name!r} column {col_index}",
                patch_offset + inline_pixel_start,
                pixel_len_inline,
                patch_page,
                base_offset,
                errors,
            )
            continue

        pos = col_offset
        while pos < len(patch_blob):
            td = patch_blob[pos]
            if td == 0xFF:
                break
            if pos + 4 > len(patch_blob):
                errors.append(f"Patch {patch_name!r} column {col_index} has a truncated post header at offset {pos:#x}.")
                break
            length_byte = patch_blob[pos + 1]
            if optimized:
                ln = (length_byte & 0x7F) + 1
                last_post = bool(length_byte & 0x80)
            else:
                ln = length_byte
                last_post = False
            pix_ofs = u16_be(patch_blob, pos + 2)
            post_start = pixel_offset + pix_ofs
            post_end = post_start + ln
            if post_end > pixel_offset + pixel_len:
                errors.append(
                    f"Patch {patch_name!r} column {col_index} post at {pos} reaches {post_end:#x}, past the end of the pixel blob at {pixel_offset + pixel_len:#x}."
                )
            else:
                validate_pixel_pages(
                    f"Patch {patch_name!r} column {col_index} post",
                    post_start,
                    ln,
                    patch_page,
                    base_offset,
                    errors,
                )
            pos += 4
            if last_post:
                break


def main(argv=None):
    parser = argparse.ArgumentParser(description="Validate sprite page placement for wadcode --big-endian --poffsets --ssf WAD files.")
    parser.add_argument("wad", help="WAD produced by wadcode --big-endian --poffsets --ssf --baseoffset 225280")
    parser.add_argument("--baseoffset", type=int, default=225280, help="Base offset used when generating the WAD (default: 225280)")
    args = parser.parse_args(argv)

    try:
        wad_data, entries = parse_wad(args.wad)
    except (OSError, ValueError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2

    poffset_entry = None
    for entry in entries:
        if entry["name"] == "POFFSETS":
            poffset_entry = entry
            break
    if poffset_entry is None:
        print("ERROR: WAD does not contain a POFFSETS lump.")
        return 2

    try:
        poffset_blob = wad_data[poffset_entry["offset"]:poffset_entry["offset"] + poffset_entry["size"]]
        poffsets = parse_poffsets(poffset_blob)
    except ValueError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2

    patches = list(iter_sprite_patches(entries))
    if not patches:
        print("ERROR: No sprite patches were found between S_START and S_END.")
        return 2

    if len(patches) != len(poffsets):
        print(f"WARNING: {len(patches)} sprite patches but {len(poffsets)} POFFSETS entries; validating the common prefix.")

    errors = []
    for idx, patch_entry in enumerate(patches[:min(len(patches), len(poffsets))]):
        patch_offset = patch_entry["offset"]
        patch_page = (args.baseoffset + patch_offset) // PAGE_SIZE
        pixel_offset, pixel_len = poffsets[idx]
        patch_pages = page_set(patch_offset, patch_entry["size"], args.baseoffset)
        pixel_pages = page_set(pixel_offset, pixel_len, args.baseoffset)
        print(
            f"Patch #{idx} {patch_entry['name']!r}: "
            f"patch pages={sorted(patch_pages)}, pixel pages={sorted(pixel_pages)}"
        )

        if patch_page <= 5:
            if patch_pages - set(range(0, 6)):
                errors.append(f"Patch {patch_entry['name']!r} is on page {patch_page}, but its patch data spans pages {sorted(patch_pages)}; all patch data must stay on pages <= 5.")

            if pixel_pages - set(range(0, 6)):
                errors.append(f"Patch {patch_entry['name']!r} is on page {patch_page}, but its pixel data spans pages {sorted(pixel_pages)}; all pixel data must stay on pages <= 5.")
        else:
            if patch_pages - {patch_page}:
                errors.append(f"Patch {patch_entry['name']!r} is on page {patch_page}, but its patch data spans pages {sorted(patch_pages)}; all patch data must stay on the same patch page.")

            allowed = {patch_page} | set(range(0, 6))
            bad = pixel_pages - allowed
            if bad:
                errors.append(f"Patch {patch_entry['name']!r} is on page {patch_page}, but its pixel data spans pages {sorted(pixel_pages)}; only pages {sorted(allowed)} are allowed.")

        validate_patch(wad_data, patch_entry, pixel_offset, pixel_len, patch_page, args.baseoffset, errors)

    if errors:
        print(f"FAIL: found {len(errors)} validation error(s).")
        for err in errors:
            print(f"  - {err}")
        return 1

    print(f"PASS: validated {min(len(patches), len(poffsets))} sprite patches and all column posts without page-placement violations.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
