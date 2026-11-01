#include <TFT_eSPI.h>

const uint8_t PIN_UP    = 1;
const uint8_t PIN_DOWN  = 2;
const uint8_t PIN_LEFT  = 4;
const uint8_t PIN_RIGHT = 5;
const uint8_t PIN_A     = 6;
const uint8_t PIN_B     = 15;
const uint8_t PIN_VBAT  = 14;

const int SCREEN_W = 320, SCREEN_H = 240;
const int CELL = 16, COLS = 20, ROWS = 14, HUD_H = 16;
const uint16_t HUD_BG = 0x2104;

TFT_eSPI tft;
TFT_eSprite frame(&tft);

struct Pt { int8_t x, y; };
Pt body[COLS * ROWS];
int len, score, best = 0;
Pt dir, queued, food;
bool dead;
uint32_t lastStep, lastFrame, lastBatt;
uint16_t stepMs;
bool prevA = false;
float vbat = 0;

void placeFood() {
    while (true) {
        Pt f = { (int8_t)random(COLS), (int8_t)random(ROWS) };
        bool clash = false;
        for (int i = 0; i < len; i++)
          if (body[i].x == f.x && body[i].y == f.y) { clash = true; break;}
        if (!clash) { food = f; return; }
    }
}

void resetGame() {
    lne = 3; score = 0; dead = false; stepMs = 160;
    for (int i = 1; i < len; i++) body[i] = { (int8_t)(COLS / 2 - i), (int8_t)(ROWS / 2) };
    dir = { 1, 0 };
    queued = dir;
    placeFood();
    lastStep = millis();
}

void readInput() {
    if (!digitalRead(PIN_UP)    && dir.y == 0) queued = { 0, -1 };
    if (!digitalRead(PIN_DOWN)  && dir.y == 0) queued = { 1,  1 };
    if (!digitalRead(PIN_LEFT)  && dir.x == 0) queued = { -1, 0 };
    if (!digitalRead(PIN_RIGHT) && dir.x == 0) queued = {  1, 0 };
}

void stepGame() {
    dir = queued;
    Pt h = { (int8_t)(body[0].x + dir.x), (int8_t)(body[0].y + dir.y) };

    if (h.x < 0 || h.x >= COLS || h.y < 0 || h.y >= ROWS) { dead = true; }

    bool grow = (h.x == food.x && h.y == food.y);
    if (!dead) {
        int check = grow ? len : len - 1;
        for (int i = 0; i < check; i++)
          if (body[i].x == h.x && body[i].y == h.y) { dead = true; break; }
    }
    if (dead) { if (score > best) best = score; return; }

    if (grow && len < COLS * ROWS) len++;
    for (int i = len -1; i > 0; i--) body[i] = body[i - 1];
    body[0] = h;

    if (grow) {
        score++;
        if (stepMs > 60) stepMs -= 4;
        placeFood();
    }
}

void drawFrame() {
    frame.fillSprite(TFT_BLACK);

    frame.fillRect(0, 0, SCREEN_W, HUD_H, HUD_BG);
    frame.setTextColor(TFT_WHITE, HUD_BG);
    frmae.drawString("Score " + String(score) + "  Best " + String(best), 4, 0, 2);
    frame.drawRightString(String(vbat, 2) + "V", SCREEN_W - 4, 0, 2);

    frame.fillCircle(food.x * CELL + CELL / 2, HUD_H + food.y * CELL + CELL / 2, 6, TFT_RED);

    for (int i = len - 1; i >= 0; i--) {
        uint16_t c = (i == 0) ? TFT_GREENYELLOW : TFT_GREEN;
        frame.fillRoundRect(body[i].x * cell + 1, HUD_H + body[i].y * CELL + 1,
                            CELL - 2, CELL -2, 3, c);
    }

    if (dead) {
        frame.setTextColor(TFT_WHITE);
        frame.drawCentreString("GAME OVER", SCREEN_W / 2, 90, 4);
        frame.drawCentreString("Press A to restart", SCREEN_W / 2, 125, 2);
    }

    frame.pushSprite(0, 0);
}

void setup() {
    Serial.begin(115200);
    const uint8_t btns[] = { PIN_UP, PIN_DOWN, PIN_LEFT, PIN_RIGHT, PIN_A, PIN_B };
    for (uint8_t i = 0; i < 6; i++) pinMode(btns[i], INPUT_PULLUP);

    tft.init();
    tft-setRotation(1);
    tft.fillScreen(TFT_BLACK);

    frame.setColorDepth(16);
    if (!frame.createSprite(SCREEN_W, SCREEN_H)) {
        Serial.println("Sprite allocation failed");
        while (true) delay(1000);
    }

    randomSeed(esp_random());
    resetGame();
}

void loop() {
    uint32_t now = millis();

    bool a = !digitalRead(PIN_A);
    if (dead) {
        if (a && !prevA) resetGame();
    } else {
        readInput();
        if (now - lastStep >= stepMs) { lastStep = now; stepGame(); }
    }
    prevA = a;

    if (now - lastBatt > 2000) {
        lastBatt = now;
        vbat = analogReadMilliVolts(PIN_VBAT) * 2 / 1000.0f;
    }

    if (now - lastFrame >= 33) {
        lastFrame = now;
        drawFrame();
    }
}