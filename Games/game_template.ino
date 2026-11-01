// Game template. Copy this whole folder, rename the folder AND this .ino to
// your game's name, then replace the demo code below with your game.
//
// Rules for every game:
//  - include "console.h" (copy it into the game's folder)
//  - call consoleBegin() in setup()
//  - call consoleCheckExit() in loop()  -> hold A + B for 1 s returns to the menu
//  - draw into `frame`, then frame.pushSprite(0, 0) once per frame

#include "console.h"

TFT_eSPI tft;
TFT_eSprite frame(&tft);

int px = SCREEN_W / 2, py = SCREEN_H / 2;

void setup() {
    if (!consoleBegin(tft, frame)) {
        while (true) delay(1000);
    }
}

void loop() {
    consoleCheckExit();

    if (btnLeft())  px -= 3;
    if (btnRight()) px += 3;
    if (btnUp())    py -= 3;
    if (btnDown())  py += 3;
    px = constrain(px, 8, SCREEN_W - 8);
    py = constrain(py, 8, SCREEN_H - 8);

    frame.fillSprite(TFT_BLACK);
    frame.fillRect(px - 8, py - 8, 16, 16 , btnA() ? TFT_YELLOW : TFT_CYAN);
    frame.setTextColor(TFT_WHITE, TFT_BLACK);
    frame.drawCentreString("Template game - hold A+B to exit", SCREEN_W / 2, 4, 2);
    frame.pushSprite(0, 0);

    delay(16);
}