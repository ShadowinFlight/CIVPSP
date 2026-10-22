TARGET = civpsp
OBJS = src/main.o src/app.o src/input.o src/analog.o src/render.o src/text.o src/audio.o src/log.o src/rng.o \
       src/map.o src/unit.o src/data.o src/city.o src/tech.o src/improvement.o \
       src/world.o src/combat.o src/ai.o src/diplomacy.o src/turn_report.o src/save.o src/mapview.o \
       src/cityview.o src/diplomacyview.o src/gen/strings_gen.o
HOST_CC ?= cc

INCDIR =
CFLAGS = -O2 -G0 -Wall -Wextra -Wno-unused-parameter -std=c99 -Isrc
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =
LDFLAGS =

# Do not link -lpspsdk or -lm.  On some CFW those pull in modules that are not
# loaded and cause 8002013C at startup.
LIBS = -lpspgu -lpspgum -lpsprtc -lpsppower -lpspaudio

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = CIVITAS
PSP_EBOOT_ICON = ICON0.PNG
PSP_EBOOT_PIC1 = PIC1.PNG

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

# Headers include the generated string table, whose enum values shift when
# strings are added.  Rebuild objects when any header changes.
HEADERS = $(wildcard src/*.h) $(wildcard src/gen/*.h)
$(OBJS): $(HEADERS)

.PHONY: assets test
assets:
	python3 tools/gen_strings.py
	python3 tools/gen_font.py
	python3 tools/gen_art.py
	python3 tools/gen_audio.py
	python3 tools/pack_atlas.py

test:
	$(HOST_CC) -std=c99 -O2 -Wall -Wextra -Isrc tests/test_analog.c src/analog.c -o tests/test_analog
	./tests/test_analog
	$(HOST_CC) -std=c99 -O2 -Wall -Wextra -Isrc tests/test_map.c src/map.c -o tests/test_map
	./tests/test_map
	$(HOST_CC) -std=c99 -O2 -Wall -Wextra -Isrc tests/test_units.c src/map.c src/unit.c src/data.c src/city.c src/tech.c src/world.c src/combat.c src/ai.c src/diplomacy.c src/turn_report.c src/gen/strings_gen.c tests/log_stub.c -o tests/test_units
	./tests/test_units
	$(HOST_CC) -std=c99 -O2 -Wall -Wextra -Isrc tests/test_city.c src/map.c src/unit.c src/data.c src/city.c src/tech.c src/world.c src/combat.c src/ai.c src/diplomacy.c src/turn_report.c src/gen/strings_gen.c tests/log_stub.c -o tests/test_city
	./tests/test_city
	$(HOST_CC) -std=c99 -O2 -Wall -Wextra -Isrc tests/test_tech.c src/map.c src/unit.c src/data.c src/city.c src/tech.c src/world.c src/combat.c src/ai.c src/diplomacy.c src/turn_report.c src/gen/strings_gen.c tests/log_stub.c -o tests/test_tech
	./tests/test_tech
	$(HOST_CC) -std=c99 -O2 -Wall -Wextra -Isrc tests/test_improvements.c src/map.c src/unit.c src/data.c src/city.c src/tech.c src/improvement.c -o tests/test_improvements
	./tests/test_improvements
	$(HOST_CC) -std=c99 -O2 -Wall -Wextra -Isrc tests/test_combat.c src/map.c src/unit.c src/data.c src/city.c src/tech.c src/world.c src/combat.c src/ai.c src/diplomacy.c src/turn_report.c src/gen/strings_gen.c tests/log_stub.c -o tests/test_combat
	./tests/test_combat
	$(HOST_CC) -std=c99 -O2 -Wall -Wextra -Isrc tests/test_turn_report.c src/turn_report.c tests/log_stub.c -o tests/test_turn_report
	./tests/test_turn_report
	$(HOST_CC) -std=c99 -O2 -Wall -Wextra -Isrc tests/test_ai.c src/map.c src/unit.c src/data.c src/city.c src/tech.c src/world.c src/combat.c src/ai.c src/diplomacy.c src/turn_report.c src/gen/strings_gen.c tests/log_stub.c -o tests/test_ai
	./tests/test_ai
	$(HOST_CC) -std=c99 -O2 -Wall -Wextra -Isrc tests/test_diplomacy.c src/map.c src/unit.c src/data.c src/city.c src/tech.c src/world.c src/combat.c src/ai.c src/diplomacy.c src/turn_report.c src/gen/strings_gen.c tests/log_stub.c -o tests/test_diplomacy
	./tests/test_diplomacy
	$(HOST_CC) -std=c99 -O2 -Wall -Wextra -Isrc tests/test_save.c src/save.c src/map.c src/unit.c src/data.c src/city.c src/tech.c -o tests/test_save
	./tests/test_save
	$(HOST_CC) -std=c99 -O2 -Wall -Wextra -Isrc tests/test_m7.c src/map.c src/unit.c src/data.c src/city.c src/tech.c src/world.c src/combat.c src/ai.c src/diplomacy.c src/turn_report.c src/gen/strings_gen.c tests/log_stub.c -o tests/test_m7
	./tests/test_m7
	$(HOST_CC) -std=c99 -O2 -Wall -Wextra -Isrc tests/test_m8.c src/map.c src/unit.c src/data.c src/city.c src/tech.c src/world.c src/combat.c src/ai.c src/diplomacy.c src/turn_report.c src/gen/strings_gen.c tests/log_stub.c -o tests/test_m8
	./tests/test_m8
	$(HOST_CC) -std=c99 -O2 -Wall -Wextra -Isrc tests/test_i18n.c src/gen/strings_gen.c -o tests/test_i18n
	./tests/test_i18n
	python3 tools/check_i18n.py
	python3 tools/check_audio.py
