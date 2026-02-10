#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// TFT pins
#define TFT_CS 5
#define TFT_DC 16
#define TFT_RST 17

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// Color definitions
#define ST77XX_DARKGREY 0x7BEF
#define ST77XX_LIGHTGREY 0xC618
#define ST77XX_ORANGE 0xFD20
#define NEON_CYAN   ST77XX_CYAN
#define NEON_PINK   ST77XX_MAGENTA
#define NEON_DARK   0x1082  
#define HUD_LABEL   ST77XX_WHITE
#define HUD_VALUE   NEON_CYAN
#define HUD_ACCENT  NEON_PINK

// Button pins
#define BTN_LEFT 32
#define BTN_RIGHT 33
#define BTN_ROTATE 25
#define BTN_DOWN 26
#define BTN_START 27

const int SIDE_GUTTER = 8;

enum PieceType {
  PIECE_NONE = 0,
  PIECE_I,
  PIECE_O,
  PIECE_T,
  PIECE_S,
  PIECE_Z,
  PIECE_J,
  PIECE_L
};

const int BOARD_W = 10;
const int BOARD_H = 20;

byte board[BOARD_H][BOARD_W];

int pieceX = 3;
int pieceY = 0;

byte piece[4][4];
byte prevPiece[4][4];

const byte tetrominoes[7][4][4] = {
  // I
  {
    {0,0,0,0},
    {1,1,1,1},
    {0,0,0,0},
    {0,0,0,0}
  },
  // O
  {
    {0,0,0,0},
    {0,1,1,0},
    {0,1,1,0},
    {0,0,0,0}
  },
  // T
  {
    {0,0,0,0},
    {1,1,1,0},
    {0,1,0,0},
    {0,0,0,0}
  },
  // S
  {
    {0,0,0,0},
    {0,1,1,0},
    {1,1,0,0},
    {0,0,0,0}
  },
  // Z
  {
    {0,0,0,0},
    {1,1,0,0},
    {0,1,1,0},
    {0,0,0,0}
  },
  // J
  {
    {0,0,0,0},
    {1,1,1,0},
    {0,0,1,0},
    {0,0,0,0}
  },
  // L
  {
    {0,0,0,0},
    {1,1,1,0},
    {1,0,0,0},
    {0,0,0,0}
  }
};

const uint16_t pieceColors[8] = {
  ST77XX_BLACK,   // none
  ST77XX_CYAN,    // I
  ST77XX_YELLOW,  // O
  ST77XX_MAGENTA, // T
  ST77XX_GREEN,   // S
  ST77XX_RED,     // Z
  ST77XX_BLUE,    // J
  ST77XX_ORANGE   // L
};

int currentPiece = PIECE_I;
int nextPiece = PIECE_I;

unsigned long lastFall = 0;
const unsigned long fallInterval = 500; // ms
const unsigned long fastFall = 50;

int prevPieceX = pieceX;
int prevPieceY = pieceY;
int prevGhostY = -1;

unsigned long lastMove = 0;
const unsigned long moveDelay = 150;

const int CELL_SIZE = 6;
const int BOARD_OFFSET_X = 30;
const int BOARD_OFFSET_Y = 28;

unsigned long lastRotate = 0;
const unsigned long rotateDelay = 200;

long score = 0;
int linesCleared = 0;
int level = 1;

bool gameOver = false;

const int HOLD_W = 4 * CELL_SIZE + 4;

const int PREVIEW_X = BOARD_OFFSET_X + BOARD_W * CELL_SIZE + SIDE_GUTTER;
const int PREVIEW_Y = 40;

int holdPiece = PIECE_NONE;
bool holdUsed = false;

const int HOLD_X = max(2, BOARD_OFFSET_X - SIDE_GUTTER - HOLD_W);
const int HOLD_Y = 40;

long lastScore = -1;
int lastLevel = -1;

int lastNextPiece = -1;
int lastHoldPiece = -1;

bool btnLeftPressed = false;
bool btnRightPressed = false;
bool btnRotatePressed = false;
bool btnStartPressed = false;

enum GameState {
  STATE_READY,
  STATE_PLAYING,
  STATE_GAMEOVER
};

GameState gameState = STATE_READY;


void setup() {
  Serial.begin(115200);

  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_ROTATE, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_START, INPUT_PULLUP);

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(2);
  tft.fillScreen(ST77XX_BLACK);
  
  drawBorder();
  drawCyberGrid();

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(5, 5);
  tft.print("ESP32 Tetris");

  for (int y = 0; y < BOARD_H; y++) {
    for (int x = 0; x < BOARD_W; x++) {
      board[y][x] = 0;
    }
  }
  
  randomSeed(analogRead(0));
  
  delay(1000);
  tft.fillRect(0, 0, 128, 26, ST77XX_BLACK);
  drawHUD();
  drawNextPiece();
  drawHoldPiece();
  drawReadyScreen();
}

void loop() {
  // ---------------- READY SCREEN ----------------
  if (gameState == STATE_READY) {
    bool startPressed = !digitalRead(BTN_START);

    if (!startPressed) {
      btnStartPressed = false;
    }

    if (startPressed && !btnStartPressed) {
      btnStartPressed = true;

      tft.fillScreen(ST77XX_BLACK);
      drawBorder();
      drawCyberGrid();

      // Reset tracking variables to force redraw
      lastScore = -1;
      lastLevel = -1;
      lastNextPiece = -1;
      lastHoldPiece = -1;

      nextPiece = random(1, 8);
      spawnPiece();

      // Draw all UI elements before starting
      drawHUD();
      drawNextPiece();
      drawHoldPiece();
      redrawBoard();

      lastFall = millis();
      lastMove = millis();
      lastRotate = millis();

      gameState = STATE_PLAYING;
    }
    return; // ⛔ stop everything else
  }

  static bool needsRedraw = true;

  // ---------------- GAME OVER ----------------
  if (gameState == STATE_GAMEOVER) {
    drawGameOverScreen();

    bool startPressed = !digitalRead(BTN_START);
    
    if (!startPressed) {
      btnStartPressed = false;
    }

    if (startPressed && !btnStartPressed) {
      btnStartPressed = true;
      resetGame();
      needsRedraw = true;
    }
    return;
  }

  // Save previous position for selective erase
  prevPieceX = pieceX;
  prevPieceY = pieceY;
  prevGhostY = getGhostY();
  
  // Save previous piece shape
  for (int y = 0; y < 4; y++) {
    for (int x = 0; x < 4; x++) {
      prevPiece[y][x] = piece[y][x];
    }
  }

  // ---------------- ROTATION ----------------
  bool rotatePressed = !digitalRead(BTN_ROTATE);
  
  if (!rotatePressed) {
    btnRotatePressed = false;
  }
  
  if (rotatePressed && !btnRotatePressed && millis() - lastRotate > rotateDelay) {
    btnRotatePressed = true;
    rotatePiece();
    lastRotate = millis();
    needsRedraw = true;
  }

  // ---------------- HORIZONTAL MOVE ----------------
  if (millis() - lastMove > moveDelay) {
    bool leftPressed = !digitalRead(BTN_LEFT);
    bool rightPressed = !digitalRead(BTN_RIGHT);

    if (!leftPressed) btnLeftPressed = false;
    if (!rightPressed) btnRightPressed = false;

    if (leftPressed && !btnLeftPressed && canMove(-1, 0)) {
      btnLeftPressed = true;
      pieceX--;
      lastMove = millis();
      needsRedraw = true;
    }

    if (rightPressed && !btnRightPressed && canMove(1, 0)) {
      btnRightPressed = true;
      pieceX++;
      lastMove = millis();
      needsRedraw = true;
    }
  }

  // ---------------- HOLD ----------------
  bool startPressed = !digitalRead(BTN_START);
  
  if (!startPressed) {
    btnStartPressed = false;
  }

  if (startPressed && !btnStartPressed) {
    btnStartPressed = true;
    holdCurrentPiece();
    needsRedraw = true;
  }

  // ---------------- FALL SPEED ----------------
  unsigned long currentFall = (!digitalRead(BTN_DOWN)) ? fastFall : getNormalFall();

  // ---------------- GRAVITY ----------------
  if (millis() - lastFall > currentFall) {
    if (canMove(0, 1)) {
      pieceY++;
      needsRedraw = true;
    } else {
      lockPiece();
      holdUsed = false;

      int cleared = clearLines();
      if (cleared > 0) {
        linesCleared += cleared;

        if (cleared == 1) score += 40 * level;
        else if (cleared == 2) score += 100 * level;
        else if (cleared == 3) score += 300 * level;
        else if (cleared == 4) score += 1200 * level;

        level = (linesCleared / 10) + 1;
      }

      redrawBoard();
      spawnPiece();
      needsRedraw = true;

      if (!canMove(0, 0)) {
        gameState = STATE_GAMEOVER;
        return;
      }
    }

    lastFall = millis();
  }

  // ---------------- RENDER (ONLY IF NEEDED) ----------------
  if (needsRedraw) {
    drawGame();
    drawHUD();
    drawNextPiece();
    drawHoldPiece();
    needsRedraw = false;
  }
}

unsigned long getNormalFall() {
  return max(100UL, 500UL - (level - 1) * 40);
}

void drawBlock(int x, int y, uint16_t color) {
  tft.fillRect(
    BOARD_OFFSET_X + x * CELL_SIZE,
    BOARD_OFFSET_Y + y * CELL_SIZE,
    CELL_SIZE - 1,
    CELL_SIZE - 1,
    color
  );
}

void drawBorder() {
  int x = BOARD_OFFSET_X - 3;
  int y = BOARD_OFFSET_Y - 3;
  int w = BOARD_W * CELL_SIZE + 6;
  int h = BOARD_H * CELL_SIZE + 6;

  int gap = 10;

  // Top
  tft.drawLine(x + gap, y, x + w - gap, y, NEON_CYAN);
  // Bottom
  tft.drawLine(x + gap, y + h, x + w - gap, y + h, NEON_CYAN);
  // Left
  tft.drawLine(x, y + gap, x, y + h - gap, NEON_CYAN);
  // Right
  tft.drawLine(x + w, y + gap, x + w, y + h - gap, NEON_CYAN);

  // Corner accents
  tft.drawLine(x + gap - 6, y, x + gap - 2, y, NEON_PINK);
  tft.drawLine(x + w - gap + 2, y, x + w - gap + 6, y, NEON_PINK);
  tft.drawLine(x + gap - 6, y + h, x + gap - 2, y + h, NEON_PINK);
  tft.drawLine(x + w - gap + 2, y + h, x + w - gap + 6, y + h, NEON_PINK);
}

void drawGame() {
  // Erase previous ghost piece
  if (prevGhostY >= 0 && prevGhostY != prevPieceY) {
    for (int py = 0; py < 4; py++) {
      for (int px = 0; px < 4; px++) {
        if (prevPiece[py][px]) {
          int x = prevPieceX + px;
          int y = prevGhostY + py;
          if (y >= 0 && y < BOARD_H && x >= 0 && x < BOARD_W) {
            // Only erase if there's no locked piece here
            if (board[y][x] == 0) {
              drawBlock(x, y, ST77XX_BLACK);
            }
          }
        }
      }
    }
  }

  // Erase previous piece position
  for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
      if (prevPiece[py][px]) {
        int x = prevPieceX + px;
        int y = prevPieceY + py;
        if (y >= 0 && y < BOARD_H && x >= 0 && x < BOARD_W) {
          // Only erase if there's no locked piece here
          if (board[y][x] == 0) {
            drawBlock(x, y, ST77XX_BLACK);
          }
        }
      }
    }
  }

  // Draw locked pieces
  for (int y = 0; y < BOARD_H; y++) {
    for (int x = 0; x < BOARD_W; x++) {
      if (board[y][x]) {
        drawBlock(x, y, pieceColors[board[y][x]]);
      }
    }
  }

  // Draw ghost piece
  drawGhostPiece();

  // Draw current piece
  for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
      if (piece[py][px]) {
        int y = pieceY + py;
        if (y >= 0) {
          drawBlock(pieceX + px, y, pieceColors[currentPiece]);
        }
      }
    }
  }
}

bool canMove(int dx, int dy) {
  for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
      if (!piece[py][px]) continue;

      int newX = pieceX + px + dx;
      int newY = pieceY + py + dy;

      if (newY >= BOARD_H) return false;
      if (newX < 0 || newX >= BOARD_W) return false;
      if (newY >= 0 && board[newY][newX]) return false;
    }
  }
  return true;
}

void lockPiece() {
  for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
      if (piece[py][px]) {
        int bx = pieceX + px;
        int by = pieceY + py;
        if (by >= 0 && by < BOARD_H && bx >= 0 && bx < BOARD_W) {
          board[by][bx] = currentPiece;
        }
      }
    }
  }
}

int clearLines() {
  int cleared = 0;

  for (int y = BOARD_H - 1; y >= 0; y--) {
    bool full = true;

    for (int x = 0; x < BOARD_W; x++) {
      if (board[y][x] == 0) {
        full = false;
        break;
      }
    }

    if (full) {
      cleared++;

      flashRow(y, ST77XX_WHITE);
      delay(60);
      flashRow(y, ST77XX_BLACK);
      delay(60);

      for (int row = y; row > 0; row--) {
        for (int col = 0; col < BOARD_W; col++) {
          board[row][col] = board[row - 1][col];
        }
      }

      for (int col = 0; col < BOARD_W; col++) {
        board[0][col] = 0;
      }

      y++;
    }
  }

  return cleared;
}

void spawnPiece() {
  currentPiece = nextPiece;
  nextPiece = random(1, 8);

  pieceX = 3;
  pieceY = -1;

  for (int y = 0; y < 4; y++) {
    for (int x = 0; x < 4; x++) {
      piece[y][x] = tetrominoes[currentPiece - 1][y][x];
    }
  }
}

void rotatePiece() {
  byte temp[4][4];

  for (int y = 0; y < 4; y++) {
    for (int x = 0; x < 4; x++) {
      temp[x][3 - y] = piece[y][x];
    }
  }

  for (int y = 0; y < 4; y++) {
    for (int x = 0; x < 4; x++) {
      if (!temp[y][x]) continue;

      int boardX = pieceX + x;
      int boardY = pieceY + y;

      if (boardX < 0 || boardX >= BOARD_W) return;
      if (boardY >= BOARD_H) return;
      if (boardY >= 0 && board[boardY][boardX]) return;
    }
  }

  for (int y = 0; y < 4; y++) {
    for (int x = 0; x < 4; x++) {
      piece[y][x] = temp[y][x];
    }
  }
}

void redrawBoard() {
  for (int y = 0; y < BOARD_H; y++) {
    for (int x = 0; x < BOARD_W; x++) {
      if (board[y][x]) {
        drawBlock(x, y, pieceColors[board[y][x]]);
      } else {
        drawBlock(x, y, ST77XX_BLACK);
      }
    }
  }
}

void drawHUD() {
  if (score == lastScore && level == lastLevel) return;

  lastScore = score;
  lastLevel = level;

  tft.fillRect(0, 0, 128, 26, 0x0010);
  tft.drawLine(0, 25, 128, 25, HUD_ACCENT);

  tft.setTextSize(1);
  tft.setTextColor(HUD_LABEL);
  tft.setCursor(4, 2);
  tft.print("SCORE");

  tft.setTextSize(2);
  tft.setTextColor(HUD_VALUE);
  tft.setCursor(4, 10);
  tft.print(score);

  tft.setTextSize(1);
  tft.setCursor(90, 2);
  tft.print("LVL");

  tft.setTextSize(2);
  tft.setCursor(90, 10);
  if (level < 10) tft.print("0");
  tft.print(level);
}

void resetGame() {
  lastFall = millis();
  lastMove = millis();
  lastRotate = millis();
  score = 0;
  linesCleared = 0;
  level = 1;
  holdPiece = PIECE_NONE;
  holdUsed = false;
  lastScore = -1;
  lastLevel = -1;
  lastNextPiece = -1;
  lastHoldPiece = -1;

  for (int y = 0; y < BOARD_H; y++) {
    for (int x = 0; x < BOARD_W; x++) {
      board[y][x] = 0;
    }
  }

  tft.fillScreen(ST77XX_BLACK);
  drawBorder();
  drawCyberGrid();
  redrawBoard();
  nextPiece = random(1, 8);
  spawnPiece();
  drawHUD();
  drawNextPiece();
  drawHoldPiece();
  gameState = STATE_PLAYING;
}

void drawGameOverScreen() {
  static bool screenDrawn = false;
  
  if (!screenDrawn) {
    tft.fillScreen(ST77XX_BLACK);

    // Animated border flash effect
    for (int i = 0; i < 3; i++) {
      tft.drawRect(5, 5, 118, 150, NEON_PINK);
      delay(100);
      tft.drawRect(5, 5, 118, 150, ST77XX_BLACK);
      delay(100);
    }
    tft.drawRect(5, 5, 118, 150, NEON_CYAN);

    // Title with shadow effect
    tft.setTextColor(ST77XX_DARKGREY);
    tft.setTextSize(2);
    tft.setCursor(21, 21);
    tft.print("GAME");
    tft.setCursor(21, 41);
    tft.print("OVER");

    tft.setTextColor(NEON_PINK);
    tft.setCursor(20, 20);
    tft.print("GAME");
    tft.setCursor(20, 40);
    tft.print("OVER");

    // Decorative line
    tft.drawLine(15, 62, 113, 62, NEON_CYAN);
    tft.drawLine(15, 63, 113, 63, NEON_DARK);

    // Stats section
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(15, 72);
    tft.print("FINAL STATS");

    // Score with label
    tft.setTextColor(NEON_CYAN);
    tft.setCursor(15, 88);
    tft.print("Score:");
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(2);
    tft.setCursor(20, 98);
    tft.print(score);

    // Level
    tft.setTextSize(1);
    tft.setTextColor(NEON_CYAN);
    tft.setCursor(80, 88);
    tft.print("Level:");
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(2);
    tft.setCursor(85, 98);
    tft.print(level);

    // Lines cleared
    tft.setTextSize(1);
    tft.setTextColor(NEON_CYAN);
    tft.setCursor(15, 118);
    tft.print("Lines:");
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(50, 118);
    tft.print(linesCleared);

    // Decorative line
    tft.drawLine(15, 130, 113, 130, NEON_PINK);

    // Instructions with blinking effect
    tft.setTextColor(ST77XX_YELLOW);
    tft.setCursor(18, 140);
    tft.print("Press START");
    
    screenDrawn = true;
  } else {
    // Blinking prompt effect
    static unsigned long lastBlink = 0;
    static bool blinkState = false;
    
    if (millis() - lastBlink > 500) {
      lastBlink = millis();
      blinkState = !blinkState;
      
      if (blinkState) {
        tft.setTextSize(1);
        tft.setTextColor(ST77XX_YELLOW);
        tft.setCursor(18, 140);
        tft.print("Press START");
      } else {
        tft.fillRect(18, 140, 100, 8, ST77XX_BLACK);
      }
    }
  }
}

void drawNextPiece() {
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(PREVIEW_X - 2, PREVIEW_Y - 12);
  tft.print("NEXT");

  if (nextPiece == lastNextPiece) return;
  lastNextPiece = nextPiece;

  tft.fillRect(PREVIEW_X - 2, PREVIEW_Y - 2,
               4 * CELL_SIZE + 4,
               4 * CELL_SIZE + 4,
               ST77XX_BLACK);

  tft.drawRect(PREVIEW_X - 2, PREVIEW_Y - 2,
               4 * CELL_SIZE + 4,
               4 * CELL_SIZE + 4,
               ST77XX_WHITE);

  for (int y = 0; y < 4; y++) {
    for (int x = 0; x < 4; x++) {
      if (tetrominoes[nextPiece - 1][y][x]) {
        tft.fillRect(
          PREVIEW_X + x * CELL_SIZE,
          PREVIEW_Y + y * CELL_SIZE,
          CELL_SIZE - 1,
          CELL_SIZE - 1,
          pieceColors[nextPiece]
        );
      }
    }
  }
}

void flashRow(int row, uint16_t color) {
  for (int x = 0; x < BOARD_W; x++) {
    drawBlock(x, row, color);
  }
}

void holdCurrentPiece() {
  if (holdUsed) return;

  if (holdPiece == PIECE_NONE) {
    holdPiece = currentPiece;
    spawnPiece();
  } else {
    int temp = currentPiece;
    currentPiece = holdPiece;
    holdPiece = temp;

    pieceX = 3;
    pieceY = -1;

    for (int y = 0; y < 4; y++) {
      for (int x = 0; x < 4; x++) {
        piece[y][x] = tetrominoes[currentPiece - 1][y][x];
      }
    }
  }

  holdUsed = true;
}

void drawHoldPiece() {
  tft.setCursor(HOLD_X, HOLD_Y - 12);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.print("HOLD");

  if (holdPiece == lastHoldPiece) return;
  lastHoldPiece = holdPiece;

  tft.fillRect(HOLD_X - 2, HOLD_Y - 2,
               4 * CELL_SIZE + 4,
               4 * CELL_SIZE + 4,
               ST77XX_BLACK);

  tft.drawRect(HOLD_X - 2, HOLD_Y - 2,
               4 * CELL_SIZE + 4,
               4 * CELL_SIZE + 4,
               ST77XX_WHITE);

  if (holdPiece == PIECE_NONE) return;

  for (int y = 0; y < 4; y++) {
    for (int x = 0; x < 4; x++) {
      if (tetrominoes[holdPiece - 1][y][x]) {
        tft.fillRect(
          HOLD_X + x * CELL_SIZE,
          HOLD_Y + y * CELL_SIZE,
          CELL_SIZE - 1,
          CELL_SIZE - 1,
          pieceColors[holdPiece]
        );
      }
    }
  }
}

int getGhostY() {
  int ghostY = pieceY;

  while (true) {
    bool canFall = true;

    for (int py = 0; py < 4; py++) {
      for (int px = 0; px < 4; px++) {
        if (!piece[py][px]) continue;

        int x = pieceX + px;
        int y = ghostY + py + 1;

        if (y >= BOARD_H || (y >= 0 && board[y][x])) {
          canFall = false;
          break;
        }
      }
      if (!canFall) break;
    }

    if (!canFall) break;
    ghostY++;
  }

  return ghostY;
}

void drawGhostPiece() {
  int ghostY = getGhostY();

  if (ghostY < 0 || ghostY == pieceY) return;

  for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
      if (!piece[py][px]) continue;

      int x = pieceX + px;
      int y = ghostY + py;

      if (y >= 0 && y < BOARD_H) {
        drawBlockOutline(x, y, ST77XX_LIGHTGREY);
      }
    }
  }
}

void drawBlockOutline(int x, int y, uint16_t color) {
  int px = BOARD_OFFSET_X + x * CELL_SIZE;
  int py = BOARD_OFFSET_Y + y * CELL_SIZE;

  tft.drawRect(
    px,
    py,
    CELL_SIZE - 1,
    CELL_SIZE - 1,
    color
  );
}

void drawCyberGrid() {
  for (int x = 1; x < BOARD_W; x++) {
    int px = BOARD_OFFSET_X + x * CELL_SIZE;
    tft.drawLine(
      px,
      BOARD_OFFSET_Y,
      px,
      BOARD_OFFSET_Y + BOARD_H * CELL_SIZE,
      NEON_DARK
    );
  }
}

void drawReadyScreen() {
  tft.fillRect(0, 0, 128, 160, ST77XX_BLACK);

  drawBorder();
  drawCyberGrid();

  tft.setTextColor(NEON_CYAN);
  tft.setTextSize(2);
  tft.setCursor(18, 40);
  tft.print("TETRIS");

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(20, 80);
  tft.print("Press START");

  tft.setCursor(20, 95);
  tft.print("to play");
}
