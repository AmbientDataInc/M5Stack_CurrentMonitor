#include <M5Stack.h>
#include "menu.h"

void Menu::putMenu(char *s, int x) {
    char buf[N_MENU];
    int len;

    if (s) {
        M5.Lcd.setCursor(x, M5.Lcd.height() - H_MENU + 4);
        len = min(strlen(s), N_MENU);
        strncpy(buf, s, len);
        buf[len] = '\0';
        M5.Lcd.print(buf);
    }
}

void Menu::setMenu(char *a, char* b, char *c, uint16_t textcolor, uint16_t backcolor) {
    M5.Lcd.setTextColor(textcolor, backcolor);
    putMenu(a, 38);
    putMenu(b, 130);
    putMenu(c, 225);
}
