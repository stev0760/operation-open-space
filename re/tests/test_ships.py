from __future__ import annotations

import json
import struct

import pytest
from conftest import require

from isre import ships, szdd
from isre.cli import main as cli_main


def body_bytes() -> bytes:
    b = bytearray(ships.BODY_SIZE)
    b[0:11] = b"TRANSFORMER"
    struct.pack_into("<HHbbbbbBBBH", b, 0x15, 1, 0x0302, -8, 10, -127, 24, -1, 200, 6, 0x42, 60)
    b[0x23:0x2A] = b"IDRAGON"
    return bytes(b)


def ship_bytes() -> bytes:
    """Distinct non-zero bytes in every number field, so a field read at another field's
    offset gets a different value: TRANSFORMER's, with team 5, to_friends 1, byte_07 12 and
    both words given a high byte. tests/test_ships.cpp builds the same record."""
    s = bytearray(ships.SHIP_SIZE)
    s[0:10] = bytes([0x21, 0x29, 0x22, 5, 1, 2, 8, 12, 0x0A, 0x15])
    s[0x0A:0x15] = b"TRANSFORMER"
    struct.pack_into("<HH", s, 0x1F, 0x0307, 0x0423)
    s[0x23:0x26] = b"SDi"
    s[0x38:0x3B] = b"ODD"
    return bytes(s)


# ---------------------------------------------------------------- synthetic

def test_field_tables_cover_every_byte():
    for kind, (size, fields) in ships.KINDS.items():
        covered = sorted(b for _n, off, _t, n in fields for b in range(off, off + n))
        assert covered == list(range(size)), kind


def test_body_record_fields_and_signedness():
    r = ships.parse_record("bod", body_bytes())
    assert r.values == {
        "name": b"TRANSFORMER", "word_15": 1, "word_17": 0x0302, "byte_19": -8, "byte_1a": 10,
        "strength_base": -127, "byte_1c": 24, "byte_1d": -1, "byte_1e": 200, "byte_1f": 6,
        "flags": 0x42, "bitmap_id": 60, "bitmap_file": b"IDRAGON"}
    assert not any(r.stale)
    assert ships.write_record("bod", r) == body_bytes()
    with pytest.raises(ValueError):
        ships.parse_record("bod", body_bytes()[:-1])


def test_ship_record_fields():
    r = ships.parse_record("shp", ship_bytes())
    assert r.values == {
        "fuel_tank": 33, "thruster": 41, "armor": 34, "team": 5, "to_friends": 1,
        "to_enemies": 2, "weapon": 8, "byte_07": 12, "byte_08": 10, "flags": 0x15,
        "name": b"TRANSFORMER", "hit_sound": 0x0307, "body": 0x0423, "author": b"SDi",
        "sound_file": b"ODD"}
    assert ships.write_record("shp", r) == ship_bytes()


def test_stale_bytes_and_string_limits():
    raw = bytearray(ship_bytes())
    raw[0x0A + 15] = ord("Q")
    raw[0x40] = 0xEE
    r = ships.parse_record("shp", bytes(raw))
    assert r.values["name"] == b"TRANSFORMER"
    assert [i for i, c in enumerate(r.stale) if c] == [0x0A + 15, 0x40]
    assert ships.write_record("shp", r) == bytes(raw)
    r.stale = b""  # the Factory's zero-filled record
    assert ships.write_record("shp", r) == ship_bytes()
    r.values["author"] = b"A" * 21  # fills the field, no NUL
    full = ships.write_record("shp", r)
    assert full[0x23:0x38] == b"A" * 21 and full[0x38:0x3B] == b"ODD"
    assert ships.parse_record("shp", full).values["author"] == b"A" * 21
    for bad in (b"A" * 22, b"a\0b"):
        r.values["author"] = bad
        with pytest.raises(ValueError):
            ships.write_record("shp", r)


def test_file_count_and_tail():
    data = body_bytes() + b"\0" + body_bytes()[1:] + b"\0\0\xab\0\0"
    f = ships.parse("bod", data)
    assert len(f.records) == 2 and f.tail == b"\0\0\xab\0\0"
    assert f.records[1].values["name"] == b"" and f.records[1].stale[1] == ord("R")
    assert ships.write(f) == data
    short = ships.parse("shp", ship_bytes()[:64])
    assert short.records == [] and short.tail == ship_bytes()[:64]
    assert ships.write(short) == ship_bytes()[:64]
    with pytest.raises(ValueError):
        ships.parse("xyz", b"")


def test_kind_from_name():
    assert ships.kind_for("MASTER.BOD") == "bod"
    assert ships.kind_for("x/master.shp") == "shp"
    assert ships.kind_for("MASTER.BO1") == "bod"
    assert ships.kind_for("MASTER.SH1") == "shp"
    assert ships.kind_for("README.TXT") is None


def test_cli_tsv_text_json(tmp_path, capsys):
    p = tmp_path / "MASTER.SHP"
    p.write_bytes(ship_bytes() + ship_bytes() + bytes(10))
    assert cli_main(["ships", str(p), "--tsv"]) == 0
    lines = capsys.readouterr().out.splitlines()
    assert lines[0].split("\t") == ships.columns("shp")
    assert lines[2].split("\t") == ["1", "33", "41", "34", "5", "1", "2", "8", "12", "10", "0x15",
                                    "TRANSFORMER", "775", "1059", "SDi", "ODD"]
    assert len(lines) == 3
    assert cli_main(["ships", str(p)]) == 0
    assert "2 records, 10 tail bytes (all zero)" in capsys.readouterr().out
    assert cli_main(["ships", str(p), "--json"]) == 0
    j = json.loads(capsys.readouterr().out)
    assert j["kind"] == "shp" and j["tail"] == "00" * 10 and j["records"][1]["body"] == 0x0423

    q = tmp_path / "ships.dat"
    q.write_bytes(body_bytes())
    assert cli_main(["ships", str(q)]) == 2
    assert "--kind" in capsys.readouterr().err
    assert cli_main(["ships", str(q), "--kind", "bod", "--tsv"]) == 0
    assert capsys.readouterr().out.splitlines()[1].split("\t")[-2:] == ["60", "IDRAGON"]
    assert cli_main(["ships", str(tmp_path / "missing.bod")]) == 2


def test_tsv_escapes_text():
    r = ships.parse_record("bod", body_bytes())
    r.values["name"] = b"A\tB\\\xe9"
    f = ships.ShipFile("bod", [r])
    assert ships.tsv(f).splitlines()[1].split("\t")[1] == "A\\tB\\\\é"


# ---------------------------------------------------------------- original files

@pytest.fixture(scope="module")
def shipped(game):
    return (ships.load(require(game / "MASTER.BO1")),
            ships.load(require(game / "MASTER.SH1")))


def test_shipped_counts_and_round_trip(shipped, game):
    bod, shp = shipped
    assert (len(bod.records), bod.tail) == (52, bytes(16))
    assert (len(shp.records), shp.tail) == (118, bytes(10))
    assert ships.write(bod) == szdd.expand((game / "MASTER.BO1").read_bytes())
    assert ships.write(shp) == szdd.expand((game / "MASTER.SH1").read_bytes())
    assert not any(any(r.stale) for r in bod.records + shp.records)


def test_shipped_documented_values(shipped):
    """formats.md MASTER.BOD / MASTER.SHP facts, read through the field tables."""
    bod, shp = shipped
    b = [r.values for r in bod.records]
    s = [r.values for r in shp.records]
    assert {r["word_15"] for r in b} == {1}
    assert [i for i, r in enumerate(b) if r["name"] == b""] == [51]
    assert (b[51]["bitmap_id"], b[51]["bitmap_file"]) == (0xFFFF, b"IDRAGON")
    thrusters = [r["thruster"] for r in s]
    assert (min(thrusters), max(thrusters)) == (23, 60)
    assert all(r["body"] < len(b) for r in s)
    t = s[1]
    assert (t["name"], t["body"], t["author"], t["sound_file"]) == (b"TRANSFORMER", 33, b"SDi", b"ODD")
    assert (b[33]["name"], b[33]["bitmap_id"]) == (b"TRANSFORMER", 60)


def test_shipped_thunderbird(shipped):
    """SHP record 3 holds distinct non-zero bytes in every number field."""
    bod, shp = shipped
    assert ships.write_record("shp", shp.records[3])[:10] == bytes.fromhex("1b322005020103070c1c")
    assert shp.records[3].values == {
        "fuel_tank": 27, "thruster": 50, "armor": 32, "team": 5, "to_friends": 2,
        "to_enemies": 1, "weapon": 3, "byte_07": 7, "byte_08": 12, "flags": 0x1C,
        "name": b"THUNDERBIRD", "hit_sound": 9, "body": 3, "author": b"SDi",
        "sound_file": b"FUNK2"}
    assert bod.records[3].values["name"] == b"THUNDERBIRD"


# Each number field summed over every record; tests/test_ships.cpp checks the same sums
# on the C++ reader, so the two readers agree field by field on the shipped files.
FIELD_SUMS = {
    "bod": {"word_15": 52, "word_17": 52, "byte_19": 1283, "byte_1a": 495,
            "strength_base": 1557, "byte_1c": 1563, "byte_1d": 302, "byte_1e": 212,
            "byte_1f": 707, "flags": 492, "bitmap_id": 97170},
    "shp": {"fuel_tank": 3462, "thruster": 5135, "armor": 3705, "team": 390,
            "to_friends": 186, "to_enemies": 166, "weapon": 1096, "byte_07": 935,
            "byte_08": 925, "flags": 2633, "hit_sound": 1161, "body": 2767},
}


def test_shipped_field_sums(shipped):
    for f in shipped:
        numbers = [name for name, _off, typ, _n in ships.KINDS[f.kind][1] if typ != "str"]
        assert sorted(FIELD_SUMS[f.kind]) == sorted(numbers), f.kind
        sums = {name: sum(r.values[name] for r in f.records) for name in numbers}
        assert sums == FIELD_SUMS[f.kind], f.kind


def test_shipped_bitmap_ids_are_igraphic_sprites(shipped, game):
    from isre.ne import NEModule

    ig = NEModule.load(require(game / "IGRAPHIC.DL1"))
    sprites = {r.id for r in ig.resources if r.type_id == 4000}
    ids = {r.values["bitmap_id"] for r in shipped[0].records} - {0xFFFF}
    assert len(ids) == 51 and ids <= sprites
