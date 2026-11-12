# Bubi Asz - poker games for the Primo (z88dk, +primo). Four tape files: build/bubi-asz-{bw,color}-{en,hu}.ptp
#   make            build the tapes (+ .wav), verify structure and memory use
#   make test       host engine tests + host end-to-end (sanitizers) + the engine on an emulated Z80
NAME      ?= bubi-asz
LIMIT_bw    ?= 0xE5F0
LIMIT_color ?= 0xDE90
Z88DK_IMG ?= z88dk/z88dk@sha256:2aab19402c1846858e5fb80b1bb7582af2a94375c41a592512a9bef4bc90719f

ifeq ($(shell command -v zcc 2>/dev/null),)
RUN = docker run --rm --user $(shell id -u):$(shell id -g) -v $(CURDIR):/src -w /src $(Z88DK_IMG)
endif
# no -Cs--sdcccall1: the z88dk runtime helpers expect the default convention
CFLAGS   = -compiler=sdcc -SO3 -Cc"-Wall" -Cs--opt-code-size -Cs"--max-allocs-per-node 100000" -Isrc -Ibuild
LOGIC    = src/cards.c src/poker.c src/ai.c src/hall.c
UI_SRC   = src/font.c src/faces.c src/gfx.c src/input.c src/ui.c src/play.c src/hw_color.c
TAPE_SRC = src/main.c $(LOGIC) $(UI_SRC) src/hw_primo.c
# the Hungarian tapes compile copies of the sources with the accented capitals turned into character codes (tools/accents.py)
HU_SRC   = build/hu/ui.c build/hu/play.c build/hu/input.c
tape_src = $(if $(filter hu,$(word 2,$(subst -, ,$(1)))),$(filter-out src/ui.c src/play.c src/input.c,$(TAPE_SRC)) $(HU_SRC),$(TAPE_SRC))
PTP      = python3 -I tools/ptp.py
MODELS   = bw color
LANGS    = en hu
VARIANTS = $(foreach m,$(MODELS),$(foreach l,$(LANGS),$(m)-$(l)))
TAPES    = $(addprefix build/$(NAME)-,$(addsuffix .ptp,$(VARIANTS)))
lang_flag = $(if $(filter hu,$(word 2,$(subst -, ,$(1)))),-DLANG_HU)

.PHONY: z80-test all test host-test rules oracle ai-stats text gfx-test bench e2e emu check ramtop clean
all: $(TAPES) $(TAPES:.ptp=.wav) check ramtop
build: ; @mkdir -p build
# Gyorgy's and Klarika's comments: src/chat.txt packed into a header per language
CHAT_H = build/chat_en.h build/chat_hu.h
build/chat_%.h: src/chat.txt tools/gen_chat.py | build
	python3 -I tools/gen_chat.py src/chat.txt $* $@

build/hu/%.c: src/%.c tools/accents.py
	@mkdir -p build/hu
	python3 -I tools/accents.py $< $@
build/$(NAME)-%.bin: $(TAPE_SRC) $(HU_SRC) $(wildcard src/*.h) $(CHAT_H) | build
	$(RUN) zcc +primo $(CFLAGS) -DTARGET_$(shell echo $* | cut -d- -f1 | tr a-z A-Z) $(call lang_flag,$*) -m $(call tape_src,$*) -o $@
build/%.img: build/%.bin
	@bss=$$(grep -E "^__BSS_head +=" build/$*.map | sed -E 's/.*\$$([0-9A-Fa-f]+).*/\1/'); \
	test -n "$$bss"; head -c $$(( 0x$$bss - 0x4400 )) $< > $@; echo "$*: tape image $$(wc -c < $@) bytes (BSS starts at $$bss)"
build/%.ptp: build/%.img
	$(PTP) build --name "$*" $< $@
build/%.wav: build/%.ptp ; $(PTP) wav $< $@

check: $(TAPES)
	@for v in $(VARIANTS); do \
	  lim=$$(case $${v%%-*} in bw) echo $(LIMIT_bw);; color) echo $(LIMIT_color);; esac); \
	  $(PTP) verify --max-end $$lim build/$(NAME)-$$v.ptp || exit 1; \
	done
ramtop: $(TAPES)
	@for v in $(VARIANTS); do \
	  top=$$(grep -i -E "^__tail +=|^__BSS_END_tail +=" build/$(NAME)-$$v.map | tail -1 | sed -E 's/.*\$$([0-9A-Fa-f]+).*/\1/'); \
	  lim=$$(printf "%d" $$(case $${v%%-*} in bw) echo $(LIMIT_bw);; color) echo $(LIMIT_color);; esac)); echo "$$v: RAM used up to $$top (limit $$lim)"; \
	  test -n "$$top" && [ $$(printf "%d" 0x$$top) -le $$lim ] || exit 1; \
	done

host-test: | build
	gcc -std=c99 -Wall -Wextra -Wshadow -g -fsanitize=address,undefined -o build/test_poker tests/test_poker.c
	build/test_poker | tee build/host.log
	@grep -qx "ALL PASS" build/host.log
e2e: $(CHAT_H) | build
	mkdir -p build/shots
	gcc -std=c99 -DHOST -DTARGET_BW -Isrc -Ibuild -Wall -Wextra -g -fsanitize=address,undefined -o build/e2e tests/e2e.c tests/hw_host.c $(LOGIC) $(UI_SRC)
	build/e2e build/shots | tee build/e2e.log
	@grep -q "E2E ALL PASS" build/e2e.log
z80-test: | build
	$(RUN) zcc +test $(CFLAGS) -DQUICK tests/test_poker.c -o build/test_poker.bin -create-app
	$(RUN) z88dk-ticks -mz80 -counter 4000000000 build/test_poker.bin | tee build/z80.log
	@grep -qx "ALL PASS" build/z80.log || { echo "Z80 tests FAILED"; exit 1; }
# The real +primo build (own C start-up, all UI and game code) in the z88dk-ticks Z80 emulator with a scripted hardware layer
# (tests/hw_ticks.c): every game is played with all kinds of keys; the run reports the deepest stack use, which must fit in each
# tape's free RAM (a stack running into the variables made the colour tape freeze and draw garbage).
emu: $(TAPES) $(CHAT_H) | build
	python3 tools/gen_emu_script.py build/emu_script.h
	$(RUN) zcc +primo $(CFLAGS) -DTARGET_BW -m $(filter-out src/hw_primo.c,$(TAPE_SRC)) tests/hw_ticks.c -o build/emu.bin
	python3 tools/emu_pack.py build/emu.bin build/emu_script.bin build/emu_run.bin
	$(RUN) z88dk-ticks -mz80 -x build/emu.map -pc 0x4400 -start 0x4400 -end _test_end -l 0x4400 -iochar 1 -counter 4000000000 build/emu_run.bin > build/emu_out.txt
	python3 tools/check_stack.py build/emu_out.txt $(addprefix build/$(NAME)-,$(addsuffix .map,$(VARIANTS)))

# game rules: whole random hands with every event checked (blinds, who acts first, pots, side pots), the hand evaluator
# against an independent Python one, and every text against the screen layout (both languages)
rules: | build
	gcc -std=c99 -Wall -Wextra -g -fsanitize=address,undefined -o build/test_rules tests/test_rules.c
	build/test_rules | tee build/rules.log
	@grep -qx "RULES ALL PASS" build/rules.log
# how the computer players bet: by street and made hand, for 4, 3 and 2 players (a report, not a test; the assertions are in host-test)
ai-stats: | build
	gcc -std=c99 -O2 -Wall -o build/ai_stats tests/ai_stats.c
	@for l in 4 3 2; do build/ai_stats 0 30000 $$l; echo; done; build/ai_stats 1 30000 4
oracle: | build
	gcc -std=c99 -Wall -Wextra -g -o build/dump_deals tests/dump_deals.c
	build/dump_deals build/deals.txt 600
	python3 -I tests/oracle.py build/deals.txt | tee build/oracle.log
	@grep -qx "ORACLE ALL PASS" build/oracle.log
text:
	python3 -I tests/check_text.py src
# the drawing routines (clear, fill, text incl. the assembly loop and the accent codes) against a pixel-by-pixel model: on the host and
# in the Z80 emulator, monochrome and colour, English and Hungarian
gfx-test: | build
	@for m in BW COLOR; do for l in "" -DLANG_HU; do \
	  gcc -std=gnu99 -Wall -Wextra -g -fsanitize=address,undefined -DHOST -DTARGET_$$m $$l -Isrc -o build/test_gfx_host tests/test_gfx.c src/font.c src/faces.c && \
	  build/test_gfx_host | grep -qx "GFX ALL PASS" && echo "gfx host $$m $$l: pass" || { echo "gfx host $$m $$l: FAILED"; exit 1; }; \
	  $(RUN) zcc +test $(CFLAGS) -DTARGET_$$m $$l tests/test_gfx.c src/font.c src/faces.c -o build/test_gfx.bin -create-app >/dev/null && \
	  $(RUN) z88dk-ticks -mz80 -counter 4000000000 build/test_gfx.bin | grep -qx "GFX ALL PASS" && echo "gfx z80  $$m $$l: pass" || { echo "gfx z80 $$m $$l: FAILED"; exit 1; }; \
	done; done
# cycle counts of the drawing routines (T-states in the Z80 emulator, one workload per run minus the empty run), colour and monochrome:
# WORK 1 = 20 screen clears, 2 = 200 text lines of 32 characters, 3 = 100 bubble-sized fills, 4 = 200 cards
bench: | build
	@for m in BW COLOR; do c=0; [ $$m = COLOR ] && c=1; base=0; for w in 0 1 2 3 4; do \
	  $(RUN) zcc +test $(CFLAGS) -DTARGET_$$m -DCOLOR=$$c -DWORK=$$w tests/bench_gfx.c src/font.c src/faces.c -o build/bench.bin -create-app >/dev/null && \
	  t=$$($(RUN) z88dk-ticks -mz80 -counter 4000000000 build/bench.bin | grep -o "Ticks: *[0-9]*" | grep -o "[0-9]*$$" | tail -1); \
	  [ $$w = 0 ] && base=$$t || echo "$$m workload $$w: $$((t - base)) T-states"; \
	done; done
test: host-test rules oracle text gfx-test e2e z80-test emu
clean: ; rm -rf build
