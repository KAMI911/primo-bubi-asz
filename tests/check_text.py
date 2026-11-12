#!/usr/bin/env python3
"""Text fit check for both languages: every T("English", "Magyar") pair, the opponents' name pool and the places where text has
to fit a fixed width are read from src/*.c and checked against the screen layout:
  - only characters the font can draw (ASCII 32..95; Hungarian also the accented capitals A E I O O O U U U with acute/umlaut/double acute)
  - every line is at most 32 characters; gfx_text(x, ...) text must end by column 32
  - status line, option labels (12, so a gap remains), game names (15), side menu (14), hand and action names (10), names (10)
  - comment bubbles: word wrap in 22 columns, at most 4 lines (the all-in question: 3)
Usage: check_text.py [src dir]"""
import re
import sys
import glob

SRC = sys.argv[1] if len(sys.argv) > 1 else "src"
ACC = "ÁÉÍÓÖŐÚÜŰ"
LIT = r'"((?:[^"\\]|\\.)*)"'
TPAIR = re.compile(r'\bT\(\s*' + LIT + r'\s*,\s*' + LIT + r'\s*\)', re.S)
bad = 0
n_checked = 0   # module level: the loop below runs at module scope


def unesc(s):
    return s.replace("\\n", "\n").replace('\\"', '"').replace("\\\\", "\\")


def fail(where, msg):
    global bad
    bad += 1
    print("TEXT: %s: %s" % (where, msg))


def wrap(s, width=22):                       # the same greedy wrap as quip_wrap() in play.c
    lines, cur = [], ""
    for w in s.split():
        w = w[:width]
        if cur and len(cur) + 1 + len(w) > width:
            lines.append(cur)
            cur = ""
        cur = cur + (" " if cur else "") + w
    return lines + [cur] if cur else lines


def limit(where, texts, maxlen):
    global n_checked
    for lang, t in zip(("EN", "HU"), texts):
        for ln in t.split("\n"):
            n_checked += 1
            if len(ln) > maxlen:
                fail(where, "%s text %r is %d characters, at most %d fit" % (lang, ln, len(ln), maxlen))


for path in sorted(glob.glob(SRC + "/*.c")):
    text = open(path, encoding="utf-8").read()
    lines = text.split("\n")
    # texts that are word-wrapped into a bubble, not drawn on a line: (start, end, max lines)
    bubbles = []
    for pat, maxl in ((r'quips\[\]\s*=\s*\{.*?\n\};', 4), (r'replies\[\]\s*=\s*\{.*?\n\};', 4), (r'guest_line\[\]\s*=[^\n]*', 4), (r'asks\[\]\s*=\s*\{.*?\n\};', 3)):
        for mb in re.finditer(pat, text, re.S):
            bubbles.append((mb.start(), mb.end(), maxl))
    # --- every T() pair: characters and line length
    for m in TPAIR.finditer(text):
        ln_no = text.count("\n", 0, m.start()) + 1
        bub = [b for b in bubbles if b[0] <= m.start() < b[1]]
        en, hu = unesc(m.group(1)), unesc(m.group(2))
        where = "%s:%d" % (path, ln_no)
        for lang, t, extra in (("EN", en, ""), ("HU", hu, ACC)):
            for ch in t:
                if ch != "\n" and not (32 <= ord(ch) <= 95 or ch in extra):
                    fail(where, "%s text %r has a character the font cannot draw: %r" % (lang, t, ch))
        if bub:
            for lang, t in (("EN", en), ("HU", hu)):
                n_checked += 1
                w = wrap(t)
                if len(w) > bub[0][2] or any(len(x) > 22 for x in w):
                    fail(where, "bubble %s %r needs %d lines of 22 (at most %d fit)" % (lang, t, len(w), bub[0][2]))
            continue
        limit(where, (en, hu), 32)
        row = lines[ln_no - 1]
        # --- context limits, by the line the T() starts on
        mt = re.search(r'gfx_text\(\s*(\d+)\s*,\s*\d+\s*,\s*T\(', row)
        if mt:
            limit(where + " (starts at column %s)" % mt.group(1), (en, hu), 32 - int(mt.group(1)))
        if "status(" in row:
            limit(where + " (status line)", (en, hu), 32)
        if re.search(r'\ba = T\(', row) and "options_row" in text[:m.start()][-1500:] and path.endswith("ui.c"):
            limit(where + " (option label)", (en, hu), 12)
        if re.search(r'\bb = .*T\(', row) and path.endswith("ui.c"):
            limit(where + " (option value)", (en, hu), 19)
        if path.endswith("play.c") and re.search(r'case L_\w+:\s+return T\(', row):
            limit(where + " (last action)", (en, hu), 10)
    # --- tables
    def table(name, maxlen, what):
        m = re.search(name + r'\[[^\]]*\]\s*=\s*\{(.*?)\};', text, re.S)
        if m:
            for p in TPAIR.finditer(m.group(1)):
                limit("%s %s" % (path, what), (unesc(p.group(1)), unesc(p.group(2))), maxlen)
    table(r'game_names', 15, "game name")
    table(r'menu_side', 14, "side menu")
    table(r'hand_names', 10, "hand name")
    m = re.search(r'name_pool\[\]\s*=\s*\{(.*?)#endif', text, re.S)
    if m:
        blk = m.group(1)
        hu_part, _, en_part = blk.partition("#else")
        en_part = en_part.partition("#endif")[0]
        def entries(part, idx):                      # names of one language: T("en","hu") pairs and plain literals
            part = re.sub(r'#\w+[^\n]*', "", part)
            out = []
            for mm in re.finditer(r'T\(\s*' + LIT + r'\s*,\s*' + LIT + r'\s*\)|' + LIT, part):
                out.append(unesc(mm.group(idx) if mm.group(1) is not None else mm.group(3)))
            return out
        lists = (("EN", entries(en_part, 1), ""), ("HU", entries(hu_part, 2), ACC))
        longest = 0
        for lang, names, extra in lists:
            n_checked += len(names)
            if len(names) < 8:
                fail(path, "%s name pool has only %d names" % (lang, len(names)))
            if len(set(names)) != len(names):
                fail(path, "%s name pool has duplicates" % lang)
            for nm in names:
                longest = max(longest, len(nm))
                if len(nm) > 10 or not all(32 <= ord(c) <= 95 or c in extra for c in nm):
                    fail(path, "%s opponent name %r (at most 10 characters, font letters)" % (lang, nm))
        for mm in re.finditer(r'sb_s\(&b, T\(' + LIT + r'\s*,\s*' + LIT + r'\)\)', text):   # "<name><text>" lines in the status bar
            for t in (unesc(mm.group(1)), unesc(mm.group(2))):
                n_checked += 1
                if longest + len(t) + 2 > 32:
                    fail(path, "status piece %r after a %d character name is too long" % (t, longest))

# --- Gyorgy's and Klarika's comments (chat.txt): font, word wrap in 22 columns, at most 4 lines (the all-in question 3)
for no, ln in enumerate(open(SRC + "/chat.txt", encoding="utf-8").read().split("\n"), 1):
    if not ln.strip() or ln.startswith("#"):
        continue
    f = [x.strip() for x in ln.split("|")]
    kind = f[0].split()[0]
    if kind == "L":
        if len(f) != 3 or f[0].split()[1] not in ("0", "1", "2", "3", "4", "C"):
            fail("chat.txt:%d" % no, "bad line %r" % ln)
        else:
            for lang, t, extra in (("EN", f[1], ""), ("HU", f[2], ACC)):
                n_checked += 1
                if len(t) > 32 or not all(32 <= ord(c) <= 95 or c in extra for c in t):
                    fail("chat.txt:%d" % no, "%s rules line %r: at most 32 columns, font characters" % (lang, t))
        continue
    if kind not in "GKDRA" or (kind in "GKD" and (f[0].split()[1] not in ("ALLIN", "BUBIASZ", "FOLD", "AIWIN", "MEWIN", "BROKE", "IDLE", "SHOW", "RIVER", "ALLINC", "TURN", "BLUFF", "NICE") or len(f) not in (3, 5))) or (kind in "RA" and len(f) != 3):
        fail("chat.txt:%d" % no, "bad line %r" % ln)
        continue
    for i, t in enumerate(f[1:]):
        lang = "EN" if i % 2 == 0 else "HU"
        n_checked += 1
        if lang == "EN" and not all(32 <= ord(c) <= 95 for c in t) or lang == "HU" and not all(32 <= ord(c) <= 95 or c in ACC for c in t):
            fail("chat.txt:%d" % no, "%s text %r has a character the font cannot draw" % (lang, t))
        w = wrap(t)
        if len(w) > (3 if kind == "A" else 4) or any(len(x) > 22 for x in w):
            fail("chat.txt:%d" % no, "%s text %r needs %d lines of 22 (at most %d fit)" % (lang, t, len(w), 3 if kind == "A" else 4))

# --- sdcc passes a 32-bit argument of a UI function wrongly (hand_text showed TWO PAIR for a high card): no score_t parameters in the UI code
for path in glob.glob(SRC + "/play.c") + glob.glob(SRC + "/ui.c"):
    for m in re.finditer(r'^\s*(?:static\s+)?\w[\w \*]*\b\w+\s*\(([^)]*)\)\s*\{', open(path, encoding="utf-8").read(), re.M):
        n_checked += 1
        if re.search(r'\b(score_t|uint32_t|int32_t|long)\s+\w+', m.group(1)):
            fail(path, "a function takes a 32-bit parameter (%s): the Z80 build passes it wrongly, pass 8/16-bit parts instead" % m.group(1).strip())

print("%d text fits checked" % n_checked)
print("TEXT ALL PASS" if not bad else "TEXT FAILED: %d problems" % bad)
sys.exit(1 if bad else 0)
