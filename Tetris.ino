#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// TFT pins
#define TFT_CS 5
#define TFT_DC 16
#define TFT_RST 17

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
#define ST77XX_DARKGREY 0x7BEF
#define ST77XX_LIGHTGREY 0xC618
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

const unsigned long fastFall   = 50;

int prevPieceX = pieceX;
int prevPieceY = pieceY;

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

const int PREVIEW_X =
  BOARD_OFFSET_X
  + BOARD_W * CELL_SIZE
  + SIDE_GUTTER;
const int PREVIEW_Y = 40;

int holdPiece = PIECE_NONE;
bool holdUsed = false;

const int HOLD_X = max(2, BOARD_OFFSET_X - SIDE_GUTTER - HOLD_W);

const int HOLD_Y = 40;

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
  tft.print("ESP32 Tetris Test");

  for (int y = 0; y < BOARD_H; y++) {
    for (int x = 0; x < BOARD_W; x++) {
      board[y][x] = 0;
    }
  }
  randomSeed(analogRead(0));
  nextPiece = random(1, 8);
  spawnPiece();

}

void loop() {

  if (gameOver) {
    drawGameOverScreen();

    static bool startReleased = true;

    if (digitalRead(BTN_START)) startReleased = true;

    if (!digitalRead(BTN_START) && startReleased) {
      resetGame();
      startReleased = false;
    }

    return;
  }

  prevPieceX = pieceX;
  prevPieceY = pieceY;

  // ---------- ROTATION ----------
  if (!digitalRead(BTN_ROTATE) && millis() - lastRotate > rotateDelay) {
    rotatePiece();
    lastRotate = millis();
  }

  // ---------- HORIZONTAL MOVEMENT ----------
  if (millis() - lastMove > moveDelay) {

    if (!digitalRead(BTN_LEFT) && canMove(-1, 0)) {
      pieceX--;
      lastMove = millis();
    }

    if (!digitalRead(BTN_RIGHT) && canMove(1, 0)) {
      pieceX++;
      lastMove = millis();
    }
  }

  // ---------- SOFT DROP ----------
  unsigned long currentFall =
    (!digitalRead(BTN_DOWN)) ? fastFall : getNormalFall();

    static bool holdReleased = true;

  if (digitalRead(BTN_START)) holdReleased = true;

  if (!digitalRead(BTN_START) && holdReleased) {
    holdCurrentPiece();
    holdReleased = false;
  }


  // ---------- GRAVITY ----------
  if (millis() - lastFall > currentFall) {

    if (canMove(0, 1)) {
      pieceY++;
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

      // ---------- SPAWN & GAME OVER CHECK ----------
      spawnPiece();
      if (!canMove(0, 0)) {
        gameOver = true;
      }
    }

    lastFall = millis();
  }

  // ---------- RENDER ----------
  drawGame();
  drawHUD();
  drawNextPiece();
  drawHoldPiece();

  delay(5);
}

unsigned long getNormalFall() {
  return max(100UL, 500UL - (level - 1) * 40);
}

void drawGrid() {
  int cell = 6;       
  int offsetX = 30;    
  int offsetY = 30;    

  for (int x = 0; x <= 10; x++) {
    tft.drawLine(
      offsetX + x * cell,
      offsetY,
      offsetX + x * cell,
      offsetY + 20 * cell,
      ST77XX_DARKGREY
    );
  }

  for (int y = 0; y <= 20; y++) {
    tft.drawLine(
      offsetX,
      offsetY + y * cell,
      offsetX + 10 * cell,
      offsetY + y * cell,
      ST77XX_DARKGREY
    );
  }
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

  tft.drawLine(x + gap - 6, y, x + gap - 2, y, NEON_PINK);
  tft.drawLine(x + w - gap + 2, y, x + w - gap + 6, y, NEON_PINK);

  tft.drawLine(x + gap - 6, y + h, x + gap - 2, y + h, NEON_PINK);
  tft.drawLine(x + w - gap + 2, y + h, x + w - gap + 6, y + h, NEON_PINK);
}



void drawGame() {

  for (int y = 0; y < BOARD_H; y++) {
    for (int x = 0; x < BOARD_W; x++) {
      drawBlock(x, y, ST77XX_BLACK);
    }
  }

  for (int y = 0; y < BOARD_H; y++) {
    for (int x = 0; x < BOARD_W; x++) {
      if (board[y][x]) {
        drawBlock(x, y, pieceColors[board[y][x]]);
      }
    }
  }

  drawGhostPiece();

  for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
      if (piece[py][px]) {
        drawBlock(pieceX + px, pieceY + py,
                  pieceColors[currentPiece]);
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
        if (by >= 0 && by < BOARD_H) {
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

  // Clear HUD area
  tft.fillRect(0, 0, 128, 26, 0x0010);

  // Accent line 
  tft.drawLine(0, 25, 128, 25, HUD_ACCENT);

  // ----- SCORE -----
  tft.setTextSize(1);
  tft.setTextColor(HUD_LABEL);
  tft.setCursor(4, 2);
  tft.print("SCORE");

  tft.setTextSize(2);
  tft.setTextColor(HUD_VALUE);
  tft.setCursor(4, 10);
  tft.print(score);

  // ----- LEVEL -----
  tft.setTextSize(1);
  tft.setTextColor(HUD_LABEL);
  tft.setCursor(90, 2);
  tft.print("LVL");

  tft.setTextSize(2);
  tft.setTextColor(HUD_VALUE);
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
  gameOver = false;
  holdPiece = PIECE_NONE;
  holdUsed = false;

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
}


void drawGameOverScreen() {
  tft.fillRect(0, 0, 128, 160, ST77XX_BLACK);

  tft.setTextColor(ST77XX_RED);
  tft.setTextSize(2);
  tft.setCursor(10, 40);
  tft.print("GAME");

  tft.setCursor(10, 60);
  tft.print("OVER");

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);

  tft.setCursor(10, 90);
  tft.print("Score: ");
  tft.print(score);

  tft.setCursor(10, 105);
  tft.print("Level: ");
  tft.print(level);

  tft.setCursor(10, 130);
  tft.print("START = retry");
}

void drawNextPiece() {

  // Clear preview area
  tft.fillRect(PREVIEW_X - 2, PREVIEW_Y - 2,
               4 * CELL_SIZE + 4,
               4 * CELL_SIZE + 4,
               ST77XX_BLACK);

  // Border
  tft.drawRect(PREVIEW_X - 2, PREVIEW_Y - 2,
               4 * CELL_SIZE + 4,
               4 * CELL_SIZE + 4,
               ST77XX_WHITE);

  // Draw blocks
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

  // Label
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(PREVIEW_X - 2, PREVIEW_Y - 12);
  tft.print("NEXT");
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

  // clear area
  tft.fillRect(HOLD_X - 2, HOLD_Y - 2,
               4 * CELL_SIZE + 4,
               4 * CELL_SIZE + 4,
               ST77XX_BLACK);

  // border
  tft.drawRect(HOLD_X - 2, HOLD_Y - 2,
               4 * CELL_SIZE + 4,
               4 * CELL_SIZE + 4,
               ST77XX_WHITE);

  // label
  tft.setCursor(HOLD_X, HOLD_Y - 12);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.print("HOLD");

  if (holdPiece == PIECE_NONE) return;

  // draw held piece
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

  // Don’t draw if piece is above board
  if (ghostY < 0) return;

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
