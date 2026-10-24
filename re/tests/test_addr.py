from __future__ import annotations

import pytest

from isre.addr import AddrError, Address, describe, parse


def test_canonical_to_ghidra_and_back():
    a = parse("seg4:0x2ea2")
    assert a == Address(4, 0x2EA2)
    assert a.ghidra == "1018:2ea2"
    assert parse("1018:2ea2") == a
    assert parse(a.ghidra).canonical == "seg4:0x2ea2"


def test_dgroup_round_trip():
    a = parse("DGROUP:0x1260")
    assert a.canonical == "seg19:0x1260"
    assert a.dgroup(19) == "DGROUP:0x1260"
    assert parse("seg19:0x1260").dgroup(19) == "DGROUP:0x1260"
    assert parse("1090:1260") == a
    assert parse("seg4:0x10").dgroup(19) is None


def test_dgroup_follows_autodata():
    assert parse("DGROUP:0x10", dgroup_seg=11) == Address(11, 0x10)


def test_com_form():
    a = parse("seg4/com:0x2ea2")
    assert a == Address(4, 0x2DA2)
    assert a.com == "seg4/com:0x2ea2"
    assert parse("com:0x2ea2", seg_context=4) == a
    assert Address(4, 0x2EA2).com == "seg4/com:0x2fa2"
    assert Address(4, 0xFFF0).com is None
    with pytest.raises(AddrError):
        parse("com:0x2ea2")
    with pytest.raises(AddrError):
        parse("seg4/com:0x80")


def test_round_trip_all_forms():
    for seg in (1, 4, 19, 32):
        for off in (0, 0x1A, 0x2EA2, 0xFEFF):
            a = Address(seg, off)
            assert parse(a.canonical) == a
            assert parse(a.ghidra) == a
            assert parse(a.com) == a
            assert parse(f"DGROUP:{off:#x}", dgroup_seg=seg) == a


@pytest.mark.parametrize("bad", ["seg0:0x10", "seg4:0x10000", "1001:0010", "0ff8:0010", "bogus", "file:0x10"])
def test_rejects_bad_input(bad):
    with pytest.raises(AddrError):
        parse(bad)


def test_with_module(inspace):
    a = parse("DGROUP:0x1260", dgroup_seg=inspace.autodata)
    info = describe(a, inspace.autodata, inspace)
    assert info["canonical"] == "seg19:0x1260" and info["segment_kind"] == "DATA"
    assert info["packed"] and info["file"] is None
    stub = describe(parse("seg1:0x1a"), inspace.autodata, inspace)
    assert stub["file"] == f"file:{inspace.segments[0].file_off + 0x1a:#x}"
    assert parse(stub["file"], module=inspace) == Address(1, 0x1A)
    with pytest.raises(AddrError, match="packed"):
        parse(f"file:{inspace.segments[3].file_off + 0x10:#x}", module=inspace)
    with pytest.raises(AddrError, match="resource"):
        parse(f"file:{inspace.resources[0].file_off:#x}", module=inspace)


def test_cli_addr(capsys):
    from isre.cli import main

    assert main(["addr", "seg4:0x2ea2"]) == 0
    out = capsys.readouterr().out
    assert "1018:2ea2" in out and "seg4/com:0x2fa2" in out
    assert main(["addr", "1018:2ea2"]) == 0
    assert "seg4:0x2ea2" in capsys.readouterr().out
    assert main(["addr", "DGROUP:0x1260"]) == 0
    assert "seg19:0x1260" in capsys.readouterr().out
    assert main(["addr", "seg19:0x1260"]) == 0
    assert "DGROUP:0x1260" in capsys.readouterr().out
    assert main(["addr", "nonsense"]) == 2
