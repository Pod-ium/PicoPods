#include "hw/keyboard.h"
#include "hw/lcd.h"
#include "ui/app.h"
int main(){ lcd::init(); static Keyboard k; k.init(); static App app(k); app.run(); }
