# Bubi Ász – póker a Primóra / poker for the Primo

Két pókerjáték három gépi ellenfél ellen, Z80-ra (z88dk, C). Négy szalagfájl: `build/bubi-asz-{bw,color}-{en,hu}.ptp` (fekete-fehér A/B és színes C/Pro Primo, angol és magyar felület).
Two poker games against three computer players (random names each session). Texas Hold'em is the default (the first menu entry, the cursor starts on it).

| menü | játék | szabály |
|---|---|---|
| TEXAS HOLD'EM | Texas Hold'em (alap) | 2 zárt + 5 közös lap; vak 5/10, tét 10 majd 20 |
| OMAHA | Omaha (high) | 4 zárt lap, pontosan 2 sajátot + 3 asztalit kell használni |

Egy tét + legfeljebb három emelés körönként, az emelés mértéke választható (FEL/LE), a legfelső szint az all in, amire Korda György (vagy Klárika) visszakérdez, mellékpotokkal. Mindenki 500 zsetonnal indul; a játék akkor ér véget, ha elfogysz vagy kinyered mindenkitől. A dicsőséglista játékonként az 5 legtöbb zsetont tartja.

## Vezérlés / controls
`F` eldob/fold · `C` megad/passz (call/check) · `R` emel/tét (raise/bet) · bal/jobb + SPACE is működik · `T` esélyed · `M` vagy `BRK`: kilépés az asztaltól a menübe, előtte rákérdez (a munkamenet a menü FOLYTATÁS pontjával folytatható, egy játék kiválasztásával újrakezdődik); `I`/`U` fel, `K` le, `J` balra, `L` jobbra is mozgat · `Z` szünet · `S` hang.

## Felépítés / layout
- `src/poker.c` – kézértékelés (5–7 lap, Omaha 2+3), tétkörök, mellékpotok, a két játékmenet; tisztán logika, hardvermentes.
- `src/ai.c` – a gépi játékosok: táblázatos kézerősség (ellenfelenként), tét-esély/pot odds, négy stílus (szoros, laza, agresszív, állomás = sokat megad, ritkán emel), blöff. Az emelési küszöb a „méltányos részhez” (100 / játékosok száma) viszonyított, ezért kevesebb játékosnál szigorúbb; az újraemeléshez többet kell tudni, mint a tétnyitáshoz. Értékre tétet nyit (egy pár is, főleg kevés játékos ellen), két pár vagy jobb szinte mindig emel. Monte Carlo helyett táblázat: a Z80-on egy kiértékelés ~16 ezer órajel, a szimuláció másodpercekig tartott volna.
- `src/play.c` – az asztal képernyője. Csak azt rajzolja újra, ami megváltozott (lapok és szövegmezők gyorsítótárral).
- `src/play.c` (Korda György és Klárika) – a Beállításokban kapcsolható kommentátorok: a jobb alsó sarokban képregényes arc és beszédbuborék jelenik meg az állás szerint (all in, azonos színű bubi-ász, eldobás, vesztésre álló játékos ...), majd a mentett képrészlet visszakerül, az asztalt nem kell újrarajzolni. Az arcok a `tools/gen_faces.py`-val készülnek (`src/faces.c`).
- `src/ui.c`, `src/hall.c`, `src/gfx.c`, `src/input.c`, `src/hw_*.c` – menük, dicsőséglista, rajzolás, billentyűzet, hardver.

## Fordítás és tesztek
```
make host-test   # kézsorrend, 7 lapos kiértékelés nyers erővel, mellékpotok, mindkét játék sok kézzel, zseton-megmaradás, az AI agresszivitása (gcc, sanitizerek)
make e2e         # a valódi felület scriptelt billentyűkkel, képernyőképek: build/shots/*.pbm (tools/pbm2png.py -> png)
make z80-test    # a motor emulált Z80-on (z88dk-ticks, Docker)
make emu         # a VALÓDI program (induló kód, felület, mindkét játék, emelések, all in, kommentátorok) emulált Z80-on szkriptelt billentyűkkel;
                 # méri a legmélyebb veremhasználatot, és ellenőrzi, hogy mind a négy szalagon elfér-e a változók és a verem között (build/emu/*.png: képernyőképek)
make rules      # a játékszabályok: véletlen leosztások végigjátszása, minden esemény ellenőrizve (vakok, ki kezd előtte/utána, fejek fölött, potok, mellékpotok, kifizetés)
make ai-stats    # a gépi játékosok viselkedése: kézerősség szerint mennyit tesznek, emelnek, adnak meg, dobnak el (játékosszám szerint; `build/ai_stats [0 hold'em|1 omaha] [kéz] [2|3|4 játékos]`)
make oracle      # a kézértékelő egy független Python kiértékelővel összevetve (tests/oracle.py, 36 000 kéz torzított paklikkal is)
make text        # minden szöveg a képernyő-elrendezéssel szemben, mindkét nyelven (tests/check_text.py)
make gfx-test    # a rajzolók (törlés, kitöltés, szöveg az assembly ciklussal és az ékezetes karakterekkel) pixelre egy naiv modellel szemben, hoszton és Z80-emulátoron
make bench       # a rajzolók ciklusszáma (T-state) az emulált Z80-on
make test        # mind a fenti teszt
make             # a négy szalag + wav, memóriahatárokkal
```
A Beállításokban színes Primón az asztal színe is állítható (zöld, bordó, fekete).
