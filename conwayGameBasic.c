/*** includes ***/
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <termio.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <time.h>
#include <stdarg.h>
#include <fcntl.h>
#include <time.h>

/*** Definitions ***/
#define CTRL_KEY(k) ((k) & 0x1f)
#define BOARD_WID 32
#define BOARD_LEN 30

enum keys {
    BACKSPACE = 127,
    ARROW_UP = 1000,
    ARROW_LEFT,
    ARROW_DOWN,
    ARROW_RIGHT,
    CTRL_LEFT,
    CTRL_RIGHT,
    CTRL_UP,
    CTRL_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN,
    F1_FUNCTION_KEY,
    F2_FUNCTION_KEY,
    F3_FUNCTION_KEY,
    F4_FUNCTION_KEY,
    F5_FUNCTION_KEY,
    F6_FUNCTION_KEY,
    F7_FUNCTION_KEY,
    F8_FUNCTION_KEY,
    F9_FUNCTION_KEY,
    F10_FUNCTION_KEY,
    ALT_S,
    ALT_F
};

/*** Conway's game of life Variables ***/
typedef enum gameState {
    PAUSED = 0,
    STEP_BY_STEP,
    CONTINUE
} gameState;


typedef struct gameOfLife {
    char ** curBoard;
    char ** nextBoard;
    char ** savedBoard;

    int boardRows;
    int boardCols;

    gameState curState;

    int cx, cy;

    int numStep;
    int savedStep;
} gameOfLife;

/*** Game Functions ***/
void gameInit(gameOfLife * this);
void updateBoard(gameOfLife * this);
int countAliveNeighbour(gameOfLife * this, int x, int y);
void boardCopy(gameOfLife * this, char ** source, char ** dest);

/*** Input & Output ***/
void processKeypress(gameOfLife * conwayGame);

typedef struct abuf {
    char* b;
    int len;
} abuf;

#define ABUF_INIT {NULL, 0}
void abAppend(struct abuf* ab, const char* s, int len);
void abFree(struct abuf* ab) {free(ab->b);}

void drawScreen(gameOfLife * this);
void setCursor(abuf* ab, int x, int y); // this is 1-indexed
void drawStatusBar(struct abuf * ab, gameOfLife * this);

/*** terminal ***/
void enableRawMode(void);
void disableRawMode(void);
void die(const char* s);
int readKey(void); // add support for new special keys here
int getWindowSize (int* rows, int* cols);
int getCursorPosition(int* rows, int* cols);

/*** Global Varl declaration ***/
struct termios orig_termio;

int main() {
    enableRawMode();

    gameOfLife conwayGame;
    gameInit(&conwayGame);

    while (1) {
        drawScreen(&conwayGame);
        processKeypress(&conwayGame);
        if (conwayGame.curState == CONTINUE || conwayGame.curState == STEP_BY_STEP) {
            updateBoard(&conwayGame);
        }
    }

    return 0;
}

/*** Game Functions ***/
void gameInit(gameOfLife * this) {
    /*
    this->boardCols = BOARD_WID;
    this->boardRows = BOARD_LEN;
    */

    if (getWindowSize(&this->boardRows, &this->boardCols) == -1) {
        die("getWindowSize");
    }
    this->boardCols = this->boardCols / 2 - 1;
    this->boardRows -= 3; // leave space for border, status bar, and instruction

    this->curBoard = (char **) malloc(sizeof(char *) * this->boardRows);
    for (int i = 0; i < this->boardRows; i++) {
        this->curBoard[i] = (char *) malloc(this->boardCols);
        memset(this->curBoard[i], '0', this->boardCols);
    }

    this->nextBoard = (char **) malloc(sizeof(char *) * this->boardRows);
    for (int i = 0; i < this->boardRows; i++) {
        this->nextBoard[i] = (char *) malloc(this->boardCols);
    }

    this->savedBoard = (char **) malloc(sizeof(char *) * this->boardRows);
    for (int i = 0; i < this->boardRows; i++) {
        this->savedBoard[i] = (char *) malloc(this->boardCols);
        memset(this->savedBoard[i], '0', this->boardCols);
    }

    this->cx = this->boardCols/2;
    this->cy = this->boardRows/2;

    this->curState = PAUSED;
    this->numStep = 0;
    this->savedStep = 0;
}

void updateBoard(gameOfLife * this) {
    for (int i = 0; i < this->boardRows; i++) {
        for (int j = 0; j < this->boardCols; j++) {
            int numAliveNeighbour = countAliveNeighbour(this, j, i);
            bool curCellALive = (this->curBoard[i][j] == '1');
            
            if (numAliveNeighbour == 3 || (numAliveNeighbour == 2 && curCellALive)) {
                this->nextBoard[i][j] = '1';
            }
            else {
                this->nextBoard[i][j] = '0';
            }
        }
    }

    // swap cur and next
    char ** temp;
    temp = this->curBoard;
    this->curBoard = this->nextBoard;
    this->nextBoard = temp;

    this->numStep++;
    if (this->curState == STEP_BY_STEP) this->curState = PAUSED;
}

int countAliveNeighbour(gameOfLife * this, int x, int y){
    int i, j, count = 0;
    for (i = y - 1; i <= y + 1; i++) {
        for (j = x - 1; j <= x + 1; j++) {
            if ((i == y && j == x) || i <0 || j < 0 || i >= this->boardRows || j >= this->boardCols) {
                continue;
            }
            if (this->curBoard[i][j] == '1') count++;
        }
    }
    return count;
}

void boardCopy(gameOfLife * this, char ** source, char ** dest){
    for (int i = 0; i < this->boardRows; i++) {
        memcpy(dest[i], source[i], this->boardCols);
    }
}

/*** Input & Output ***/
// input
void processKeypress(gameOfLife * conwayGame) {
    int c = readKey();

    switch (c) {
        case CTRL_KEY('q'): // quit
            write(STDOUT_FILENO, "\x1b[2J", 4); 
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
        case CTRL_KEY('r'): // reset
            for (int i = 0; i < conwayGame->boardRows; i++) {
                for (int j = 0; j < conwayGame->boardCols; j++) {
                    conwayGame->curBoard[i][j] = '0';
                }
            }
            conwayGame->curState = PAUSED;
            conwayGame->numStep = 0;
            break;
        case CTRL_KEY('s'): // save board
            boardCopy(conwayGame, conwayGame->curBoard, conwayGame->savedBoard);
            conwayGame->savedStep = conwayGame->numStep;
            break;
        case CTRL_KEY('l'): // load board
            boardCopy(conwayGame, conwayGame->savedBoard, conwayGame->curBoard);
            conwayGame->numStep = conwayGame->savedStep;
            conwayGame->curState = PAUSED;
            break;
        case ' ':
            if (conwayGame->cx >= 0 && conwayGame->cx < conwayGame->boardCols && 
                conwayGame->cy >= 0 && conwayGame->cy < conwayGame -> boardRows) {
                    conwayGame->curBoard[conwayGame->cy][conwayGame->cx] = (conwayGame->curBoard[conwayGame->cy][conwayGame->cx] == '0' ? '1' : '0');
                }
            break;

        case ARROW_UP:
            if (conwayGame->cy > 0) conwayGame->cy--;
            break;
        case ARROW_LEFT:
            if (conwayGame->cx > 0) conwayGame->cx--;
            break;
        case ARROW_DOWN:
            if (conwayGame->cy < conwayGame->boardRows - 1) conwayGame->cy++;
            break;
        case ARROW_RIGHT:
            if (conwayGame->cx < conwayGame->boardCols - 1) conwayGame->cx++;
            break;
        case '\0':
            break;

        case F5_FUNCTION_KEY:
            conwayGame->curState = conwayGame->curState == PAUSED ? CONTINUE : PAUSED;
            break;
        case F6_FUNCTION_KEY:
            conwayGame->curState = STEP_BY_STEP;
            break;
    }
}

// output
void abAppend(struct abuf * ab, const char * s, int len) {
    char * new = (char *) realloc(ab->b, ab->len + len);
    if (new == NULL) return;

    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void drawScreen(gameOfLife * this){
    struct abuf ab = ABUF_INIT;
    abAppend(&ab, "\x1b[?25l", 6); //hide cursor

    // draw board
    abAppend(&ab, "\x1b[H", 3);
    for (int i = 0; i < this->boardRows; i++) {
        for (int j = 0; j < this->boardCols; j++) {
            char charToPrint = this->curBoard[i][j];
            if (charToPrint == '1') {
                abAppend(&ab, "\x1b[7;33m  \x1b[m", 12); // yellow
            }
            else {
                abAppend(&ab, "  ", 2);
            }
        }
        abAppend(&ab, "|\x1b[K", 4);  // clear line right of cursor
        abAppend(&ab, "\r\n", 2);
    }

    for (int j = 0; j < this->boardCols; j++) {
        abAppend(&ab, "==", 2);
    }
    abAppend(&ab, "|\x1b[K\r\n", 6);

    drawStatusBar(&ab, this);


    setCursor(&ab, (this->cx + 1) * 2, this->cy + 1);
    abAppend(&ab, "\x1b[?25h", 6);
    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

void setCursor(struct abuf * ab, int x, int y) {
    char buf[32];
    snprintf(buf, sizeof(buf),"\x1b[%d;%dH", y, x);
    abAppend(ab, buf, strlen(buf));
}

void drawStatusBar(struct abuf * ab, gameOfLife * this) {
    abAppend(ab, "\x1b[7m", 4); // invert color
    char status[80];
    int len = snprintf(status, sizeof(status), "Step Count: %d | Game Mode: %s", this->numStep, this->curState == PAUSED ? "Paused" : "Continue");
    abAppend(ab, status, len);
    abAppend(ab, "\x1b[K\r\n", 5);

    char inst[80];
    len = snprintf(inst, sizeof(inst), "Space: Draw/Erase | Ctrl-Q: Quit | Ctrl-R: Reset | Ctrl-S/L : Save/Load State");
    abAppend(ab, inst, len);

    abAppend(ab, "\x1b[K\x1b[m", 6);
    
}

/*** terminal ***/
void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &orig_termio) == -1) {
        die("tcgetattr");
    }
    atexit(disableRawMode); // it's cool that this can be placed anywhere

    struct termios raw = orig_termio;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;    // adds a timeout to read (in 0.1s)

    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) { // TCSAFLUCH defines how the change is applied
        die("tcsetattr"); 
    }
}

void disableRawMode(void) {
    write(STDOUT_FILENO, "\x1b[?25h", 6);
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termio) == -1) {
        die("tcsetattr");
    }
}

void die(const char* s) { //error handling
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    perror(s);
    write(STDOUT_FILENO, "\r", 1);
    exit(1);
}

int readKey(void) {
    int nread;
    char c;

    nread = read(STDIN_FILENO, &c, 1);
    if(nread == -1 && errno != EAGAIN) die("read");
    else if (nread != 1) return '\0';

    if (c == '\x1b') {
        char seq[5];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        
        switch(seq[0]) {
            case 's': return ALT_S;
            case 'f': return ALT_F;
        }

        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
                else if (seq[2] == ';') {
                    if (read(STDIN_FILENO, &seq[3], 2) != 2) return '\x1b'; //this means something went wrong
                    if (seq[3] == '5') { //ctrl key pressed, seq is esc[1;5C, C is from arrow key, 1 is unchanged default keycode (used by page up/down and delete), 5 is modifier meaning ctrl
                                        //https://en.wikipedia.org/wiki/ANSI_escape_code#:~:text=0%3B%0A%7D-,Terminal%20input%20sequences,-%5Bedit%5D
                        switch (seq[4]) {
                            case 'A': return CTRL_UP;
                            case 'B': return CTRL_DOWN;
                            case 'C': return CTRL_RIGHT;
                            case 'D': return CTRL_LEFT;
                        }
                    }
                }
                else if (read(STDIN_FILENO, &seq[3], 1) == 1) {
                    if (seq[3] == '~' && seq[1] == '1') {
                        switch (seq[2]) {
                            case '5': return F5_FUNCTION_KEY;
                            case '7': return F6_FUNCTION_KEY;
                            case '8': return F7_FUNCTION_KEY;
                            case '9': return F8_FUNCTION_KEY;
                        }
                    }
                    else if (seq[3] == '~' && seq[1] == '2' && seq[2] == '0') {
                        return F9_FUNCTION_KEY;
                    }
                }
            }
            else {
                switch (seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                }
            }
        }
        else if (seq[0] == '0') {
            switch (seq[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }
        else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'P': return F1_FUNCTION_KEY;
                case 'Q': return F2_FUNCTION_KEY;
                case 'R': return F3_FUNCTION_KEY;
                case 'S': return F4_FUNCTION_KEY;
                case 't': return F5_FUNCTION_KEY;
                case 'u': return F6_FUNCTION_KEY;
                case 'v': return F7_FUNCTION_KEY;
                case 'l': return F8_FUNCTION_KEY;
                case 'w': return F9_FUNCTION_KEY;
                case 'x': return F10_FUNCTION_KEY;
            }
        }

        return '\x1b';
    }
    else {
        return c;
    }
}

int getWindowSize (int* rows, int* cols) {
    struct winsize ws;
    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;  //try move cursor a ton
        return getCursorPosition(rows, cols);  
    }
    else {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
        return 0;
    }
}

int getCursorPosition(int* rows, int* cols) {
    char buf[32];
    unsigned int i = 0;
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1; //ask terminal for cursor loc

    for (i = 0; i < sizeof(buf) - 1; i++) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
    }
    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) !=2) return -1;
}