// Pong for the handheld: You (left, cyan) vs the computer (right, orange).
// UP/DOWN move your paddle, A serves / restarts, hold A+B to exit to the menu.

#include "console.h"

TFT_eSPI tft;
TFT_eSprite frame(&tft);

const int   PAD_W = 8, PAD_H = 48, BALL = 8, MARGIN 12;
const int   WIN_SCORE = 7;
const float PLAYER_SPEED = 240.0f;
const float AI_SPEED     = 150.0f;
const float BALL_START   = 190.0f;
const float BALL_MAX     = 400.0f;
const float STEP         = 1.0f / 120.0f;

enum State { READY, PLAYING, OVER };
State state = READY;

float playerY, aiY;
float bx, by, vx, vy, speed;
int playerScore, aiScore;
int serveDir = 1;
bool prevA = false;
uint32_t lastMicros;
float acc = 0;

void resetBall() {
    bx = SCREEN_W / 2 - BALL / 2;
    by = SCREEN_H / 2 - BALL / 2;
    vx = vy = 0;
    speed = BALL_START;
    state = READY;
}

void newGame() {
    playerScore = aiScore = 0;
    playerY = aiY = (SCREEN_H - PAD_H) / 2;
    serveDir = 1;
    resetBall();
}

void serve() {
    float angle = random(-25, 26) * DEG_TO_RAD;
    vx = serveDir * speed * cosf(angle);
    vy = speed * sinf(angle);
    state = PLAYING;
}

void bounce(float padY, int dir, float newBx) {
    float rel = ((by + BALL / 2) - (padY + PAD_H / 2)) / (PAD_H / 2 + BALL / 2);
    rel = constrain(rel, -1.0f, 1.0f);
    speed = min(speed * 1.06f, BALL_MAX);
    float angle = rel * 55.0f DEG_TO_RAD;
    vx = dor * speed * cosf(angle);
    vy = speed * sinf(angle);
    bx = newBx
}

void point(int nextServeDir) {
    serveDir = nextServeDir;
    if (playerScore >= WIN_SCORE || aiScore >= WIN_SCORE) state = OVER;
    else resetBall();
}

void update(float dt) {
    if (btnUp())    playerY -= PLAYER_SPEED * dt;
    if (btnDown())  playerY += PLAYER_SPEED * dt;
    playerY = constrain(playerY, 0.0f, (float)(SCREEN_H - PAD_H));

    float target = (state == playing && vx > 0) ? (by + BALL / 2 - PAD_H / 2)
                                                : (SCREEN_H - PAD_H) / 2.0f;
    float diff = target - aiY;
    if (fabsf(diff) > 4.0f) aiY += constrain(diff, -AI_SPEED * dt, AI_SPEED * dt);
    aiY = constrain(aiY, 0.0f, (float)(SCREEN_H - PAD_H));

    if (state != PLAYING) return;

    bx += vx * dt;
    by += vy * dt;

    if (by < 0)                 { by = 0;                  vy = -vy }
    if (by > SCREEN_H - BALL)   { by = SCREEN_H - BALL;    vy = -vy }     

    float leftFront = MARGIN + PAD_W;
    if (vx < 0 && bx <= leftFront && bx + BALL >= MARGIN &&
        by + BALL >= playerY && by <= playerY + PAD_H) {
       bounce(playerY, +1, leftFront)
    }

    float rightFront = SCREEN_W - MARGIN - PAD_W;
    if (vx > 0 && bx + BALL >= rightFront && bx <= SCREEN_W - MARGIN &&
        by + BALL >= aiY && by <= aiY + PAD_H) {
       bounce(aiY, -1, rightFront - BALL);
    }

    if (bx + ball < 0)      { aiScore++;     point(-1); }
    else if (bx > SCREEN_W) { playerScore++; point(+1); }
}

void draw() {
    frame.fillSprite(TFT_BLACK);

    for (int y = 0; y < SCREEN_H; y += 16)
      frame.fillRect(SCREEN_W / 2 - 1, y, 2, 8, TFT_DARKGREY);

      frame.setTextColor(TFT_WHITE);
      frame.drawRightString(String(playerScore), SCREEN_W / 2 - 16, 8, 4);
      frame.drawString(String(aiScore), SCREEN_W / 2 + 16, 8, 4);

      frame.fillRect(MARGIN, (int)playerY, PAD_W, PAD_H, TFT_CYAN);
      frame.fillRect(SCREEN_W - MARGIN - PAD_W, (int)aiY, PAD_W, PAD_H, TFT_ORANGE);
      frame.fillRect((int)bx,  (int)by, BALL, BALL, BALL, TFT_WHITE);

      if (state == READY) {
        frame.setTextColor(TFT_YELLOW);
        frame.drawCentreString("Press A to serve", SCREEN_W / 2, 150, 4);
        frame.setTextColor(TFT_LIGHTGREY);
        frame.drawCentreString("UP/DOWN move     hold A+B for menu", SCREEN_W / 2, 218, 2);
      } else if (state == OVER) {
        frame.fillRoundRect(50, 85, 220, 80, 10, TFT_BLACK);
        frame.fillRoundRect(50, 85, 220, 80, 10, TFT_WHITE);
        frame.setTextColor(playerScore > aiScore ? TFT_GREEN : TFT_RED);
        frame.drawCentreString(playerScore > aiScore ? "YOU WIN!" : "YOU LOSE", SCREEN_W / 2, 98, 4);
        frame.setTextColor(TFT_WHITE);
        frame.drawCentreString("Press A to play again", SCREEN_W / 2, 135, 2);
      }

      frame.pushSprite(0, 0)    
}

void setup() {
    if (!consoleBegin(tft, frame)) {
        while (true) delay(1000);
    }
    randomSeed(esp_random());
    newGame();
    lastMicros = micros();
}

void loop() {
  consoleCheckExit();

  bool a = btnA();
  if (a && !prevA) {
    if (state == READY) serve();
    else if (state == OVER) newGame();
  }
  prevA = a;
  
  uint32_t now = micros();
  acc += (now - lastMicros) / 1000000.0f;
  lastMicros = now;
  if (acc > 0.1f) acc = 0.1f;
  while (acc >= STEP) { update(STEP); acc -= STEP; }

  draw();
}