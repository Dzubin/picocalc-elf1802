"""Put "Author: Thomas Dzubin" in the comment before every function of more than
20 lines. Usage: add_author.py [--apply] files..."""
import re
import sys

AUTHOR = "Author: Thomas Dzubin"
LIMIT = 20          # a function longer than this many lines gets the comment


def split_lines(raw):
    crlf = "\r\n" in raw
    return raw.replace("\r\n", "\n").split("\n"), crlf


def find_functions(lines):
    """Yield (signature_start, open_brace, close_brace) for each function."""
    n = len(lines)
    i = 0
    while i < n:
        if lines[i] == "{":
            # the signature: the lines above, back to a blank line, a ;, a } or a comment end
            j = i - 1
            sig = []
            while j >= 0:
                t = lines[j]
                if not t.strip() or t.rstrip().endswith(";") or t.rstrip().endswith("}") \
                        or t.rstrip().endswith("*/") or t.startswith("#") or t.startswith("/*"):
                    break
                sig.append(j)
                j -= 1
                if len(sig) > 6:
                    break
            if sig and "(" in lines[sig[-1]] + lines[sig[0]] or any("(" in lines[k] for k in sig):
                start = sig[-1] if sig else i
                # the closing brace at column 0
                k = i + 1
                while k < n and lines[k] != "}":
                    k += 1
                if k < n and sig:
                    yield start, i, k
                i = k
        i += 1


def comment_before(lines, start):
    """The block comment that ends on the line before start: (first, last) or None."""
    last = start - 1
    if last < 0 or "*/" not in lines[last]:
        return None
    first = last
    while first >= 0 and "/*" not in lines[first]:
        first -= 1
    if first < 0:
        return None
    block = "\n".join(lines[first:last + 1])
    if "=====" in block or "-----" in block:       # a banner, not the function's own comment
        return None
    return first, last


def process(path, apply):
    raw = open(path, "rb").read().decode("utf-8")
    lines, crlf = split_lines(raw)
    edits = []                                      # (line index, kind, ...) from the bottom up
    count = 0
    for start, open_b, close_b in find_functions(lines):
        length = close_b - start + 1
        if length <= LIMIT:
            continue
        cb = comment_before(lines, start)
        if cb:
            first, last = cb
            if AUTHOR in "\n".join(lines[first:last + 1]):
                continue
        count += 1
        edits.append((start, cb))
    if not apply:
        return count
    for start, cb in sorted(edits, key=lambda e: -e[0]):
        if cb is None:
            lines[start:start] = ["/* " + AUTHOR + " */"]
            continue
        first, last = cb
        end = lines[last]
        if first == last:                           # /* text */ on one line
            text = end.strip()[2:-2].strip()
            lines[first:last + 1] = ["/* " + text, " *", " * " + AUTHOR + " */"]
        elif end.strip() == "*/":                   # a closing line of its own
            lines[last:last + 1] = [" *", " * " + AUTHOR, " */"]
        else:                                       # text and */ on the last line
            text = end.rstrip()
            text = text[:text.rindex("*/")].rstrip()
            lines[last:last + 1] = [text, " *", " * " + AUTHOR + " */"]
    out = "\n".join(lines)
    open(path, "wb").write((out.replace("\n", "\r\n") if crlf else out).encode("utf-8"))
    return count


if __name__ == "__main__":
    apply = "--apply" in sys.argv
    total = 0
    for p in sys.argv[1:]:
        if p.startswith("--"):
            continue
        c = process(p, apply)
        if c:
            print("%-28s %d" % (p, c))
        total += c
    print("functions needing it:" if not apply else "functions changed:", total)
