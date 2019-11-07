#ifndef MENU_H
#define MENU_H

#define N_MENU 7  // 1つのメニューの最大文字数
#define H_MENU 20  // メニューの高さ

class Menu {
public:
    Menu(void) {};
    void setMenu(char *a = NULL, char* b = NULL, char *c = NULL, uint16_t textcolor = WHITE, uint16_t backcolor = BLACK);
    void putMenu(char *s, int x);
    int height() { return H_MENU; }
};

#endif // MENU_H
