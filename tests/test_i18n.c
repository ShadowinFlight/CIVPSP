#include <stdio.h>
#include <string.h>

#include "gen/strings_gen.h"

static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } \
} while (0)

int main(void) {
    CHECK(Str_GetLanguage() == LANG_EN);
    CHECK(strcmp(Str(S_MENU_NEW_GAME), "NEW GAME") == 0);

    Str_SetLanguage(LANG_ZH);
    CHECK(Str_GetLanguage() == LANG_ZH);
    CHECK(strcmp(Str(S_MENU_NEW_GAME), "新游戏") == 0);
    CHECK(strcmp(Str(S_TITLE), "CIVITAS") == 0);

    Str_SetLanguage(LANG_EN);
    CHECK(Str_GetLanguage() == LANG_EN);
    CHECK(strcmp(Str(S_TITLE), "CIVITAS") == 0);

    Str_SetLanguage(99);
    CHECK(Str_GetLanguage() == LANG_EN);
    CHECK(strcmp(Str((StrId)S_COUNT), "?") == 0);

    if (failures) {
        fprintf(stderr, "%d i18n check(s) failed\n", failures);
        return 1;
    }
    printf("PASS: English/Chinese language toggle and string lookup\n");
    return 0;
}
