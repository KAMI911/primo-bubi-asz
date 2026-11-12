#!/usr/bin/env python3
"""Independent check of the hand evaluator: re-ranks every deal of build/deals.txt (written by tests/dump_deals) with a plain
brute-force evaluator and compares it with the engine: the hand category of each seat, and the order of every pair of seats
(better / equal / worse). Also prints how often each category occurred. Usage: oracle.py deals.txt"""
import sys
from itertools import combinations
from collections import Counter

RANKS = "A23456789TJQK"
CATS = ["HIGH", "PAIR", "TWOPAIR", "TRIPS", "STRAIGHT", "FLUSH", "FULL", "QUADS", "SFLUSH"]


def val(c):                       # ace high
    return 14 if c[0] == "A" else RANKS.index(c[0]) + 1


def rank5(cs):
    """(category, tiebreak list) of exactly five cards; larger tuple = better."""
    vs = sorted((val(c) for c in cs), reverse=True)
    flush = len({c[1] for c in cs}) == 1
    uniq = sorted(set(vs), reverse=True)
    straight = 0
    if len(uniq) == 5:
        if uniq[0] - uniq[4] == 4:
            straight = uniq[0]
        elif uniq == [14, 5, 4, 3, 2]:
            straight = 5                                   # the wheel plays as a five-high straight
    groups = sorted(((vs.count(v), v) for v in uniq), reverse=True)
    shape = [g[0] for g in groups]
    tb = [g[1] for g in groups]
    if straight and flush:
        return (8, [straight])
    if shape == [4, 1]:
        return (7, tb)
    if shape == [3, 2]:
        return (6, tb)
    if flush:
        return (5, vs)
    if straight:
        return (4, [straight])
    if shape == [3, 1, 1]:
        return (3, tb)
    if shape == [2, 2, 1]:
        return (2, tb)
    if shape == [2, 1, 1, 1]:
        return (1, tb)
    return (0, vs)


def best(game, hole, board):
    if game == 1:                                          # Omaha: exactly two of the hole cards and three of the board
        hands = (list(h) + list(b) for h in combinations(hole, 2) for b in combinations(board, 3))
    else:                                                  # Hold'em: any five of seven
        hands = combinations(list(hole) + list(board), 5)
    return max(rank5(h) for h in hands)


def main():
    bad = n = 0
    cats = Counter()
    for line in open(sys.argv[1]):
        parts = [p.strip() for p in line.split("|")]
        game = int(parts[0])
        hole = [p.split() for p in parts[1:5]]
        board = parts[5].split()
        eng = [int(x, 16) for x in parts[6].split()]
        mine = [best(game, h, board) for h in hole]
        n += 1
        for s in range(4):
            cats[(game, CATS[mine[s][0]])] += 1
            if eng[s] >> 20 != mine[s][0]:
                bad += 1
                if bad < 15:
                    print("CATEGORY MISMATCH game %d seat %d: engine %s oracle %s  %s | %s" % (game, s, CATS[eng[s] >> 20], CATS[mine[s][0]], " ".join(hole[s]), " ".join(board)))
        for a in range(4):
            for b in range(a + 1, 4):
                e = (eng[a] > eng[b]) - (eng[a] < eng[b])
                o = (mine[a] > mine[b]) - (mine[a] < mine[b])
                if e != o:
                    bad += 1
                    if bad < 15:
                        print("ORDER MISMATCH game %d seats %d/%d: engine %d oracle %d  %s vs %s | %s" % (game, a, b, e, o, " ".join(hole[a]), " ".join(hole[b]), " ".join(board)))
    names = ", ".join("%s %d" % (k, v) for k, v in sorted(Counter(k[1] for k in cats.elements()).items(), key=lambda kv: CATS.index(kv[0])))
    print("%d deals, %d hands checked; categories seen: %s" % (n, 4 * n, names))
    rare = [c for c in CATS if sum(v for k, v in cats.items() if k[1] == c) < 50]
    if rare:
        print("WARNING: too few examples of", rare)
        bad += 1
    print("ORACLE ALL PASS" if not bad else "ORACLE FAILED: %d mismatches" % bad)
    return 1 if bad else 0


sys.exit(main())
