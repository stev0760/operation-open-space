"""Render decoded Win16 resources as resource-compiler (.rc) source text.

The output is a readable dump in rc syntax, not a byte-exact round trip:
controls are written in the generic ``CONTROL`` form so every class and style
is shown as stored.
"""
from __future__ import annotations

from typing import Any


def rc_quote(s: str) -> str:
    """Quote a string the way rc reads it: "" for a quote, \\t \\n \\r escapes."""
    out = s.replace("\\", "\\\\").replace('"', '""')
    return '"' + out.replace("\t", "\\t").replace("\n", "\\n").replace("\r", "\\r") + '"'


def rc_name(rid: int | str) -> str:
    return str(rid) if isinstance(rid, int) else rid


def _text(t: Any) -> str:
    if isinstance(t, dict):
        return str(t["ordinal"])
    return rc_quote(t)


def _ctl_id(cid: int) -> str:
    return "-1" if cid == 0xFFFF else str(cid)


def dialog_rc(rid: int | str, dlg: dict[str, Any]) -> str:
    head = f"{rc_name(rid)} DIALOG {dlg['x']}, {dlg['y']}, {dlg['cx']}, {dlg['cy']}"
    if isinstance(rid, int):
        head += f"  // #{rid:04x}"
    lines = [head, f"STYLE {dlg['style']}  // {' | '.join(dlg['style_names']) or '0'}"]
    if dlg["caption"]:
        lines.append(f"CAPTION {rc_quote(dlg['caption'])}")
    if dlg["menu"] is not None:
        lines.append(f"MENU {_text(dlg['menu'])}")
    if dlg["class"]:
        lines.append(f"CLASS {rc_quote(dlg['class'])}")
    if dlg["font"]:
        lines.append(f"FONT {dlg['font']['point_size']}, {rc_quote(dlg['font']['face'])}")
    lines.append("BEGIN")
    for c in dlg["controls"]:
        line = (f"    CONTROL {_text(c['text'])}, {_ctl_id(c['id'])}, {rc_quote(c['class'])}, "
                f"{c['style']}, {c['x']}, {c['y']}, {c['cx']}, {c['cy']}")
        notes = " | ".join(c["style_names"])
        if c["extra"]:
            notes += f"; extra {c['extra']}"
        lines.append(f"{line}  // {notes}" if notes else line)
    lines.append("END")
    return "\n".join(lines) + "\n"


def _menu_items(items: list[dict[str, Any]], depth: int, lines: list[str]) -> None:
    pad = "    " * depth
    for it in items:
        opts = [n for n in it["flag_names"] if n != "POPUP"]
        suffix = (", " + ", ".join(opts)) if opts else ""
        if "items" in it:
            lines.append(f"{pad}POPUP {rc_quote(it['text'])}{suffix}")
            lines.append(f"{pad}BEGIN")
            _menu_items(it["items"], depth + 1, lines)
            lines.append(f"{pad}END")
        elif it.get("separator"):
            lines.append(f"{pad}MENUITEM SEPARATOR")
        else:
            lines.append(f"{pad}MENUITEM {rc_quote(it['text'])}, {it['id']}{suffix}")


def menu_rc(rid: int | str, menu: dict[str, Any]) -> str:
    lines = [f"{rc_name(rid)} MENU", "BEGIN"]
    _menu_items(menu["items"], 1, lines)
    lines.append("END")
    return "\n".join(lines) + "\n"


def accelerators_rc(rid: int | str, entries: list[dict[str, Any]]) -> str:
    lines = [f"{rc_name(rid)} ACCELERATORS", "BEGIN"]
    for e in entries:
        opts = ", ".join(e["flag_names"])
        lines.append(f"    {e['key_name']}, {e['cmd']}" + (f", {opts}" if opts else ""))
    lines.append("END")
    return "\n".join(lines) + "\n"

