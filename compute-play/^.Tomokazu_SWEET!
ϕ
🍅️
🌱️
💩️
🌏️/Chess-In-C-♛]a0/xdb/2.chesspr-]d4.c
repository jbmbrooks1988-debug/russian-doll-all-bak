#include<stdio.h>
#include<stdlib.h>
#include<termios.h>
#include<unistd.h>
#include<fcntl.h>
#include<locale.h>

// Unicode chess piece definitions
#define white_king   0x2654 // ♔
#define white_queen  0x2655 // ♕
#define white_rook   0x2656 // ♖
#define white_bishop 0x2657 // ♗
#define white_knight 0x2658 // ♘
#define white_pawn   0x2659 // ♙
#define black_king   0x265A // ♚
#define black_queen  0x265B // ♛
#define black_rook   0x265C // ♜
#define black_bishop 0x265D // ♝
#define black_knight 0x265E // ♞
#define black_pawn   0x265F // ♟

int pwstatus[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
int pbstatus[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

int board[8][8] = {
    { white_rook, white_knight, white_bishop, white_king, white_queen, white_bishop, white_knight, white_rook },
    { white_pawn, white_pawn, white_pawn, white_pawn, white_pawn, white_pawn, white_pawn, white_pawn },
    { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' },
    { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' },
    { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' },
    { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' },
    { black_pawn, black_pawn, black_pawn, black_pawn, black_pawn, black_pawn, black_pawn, black_pawn },
    { black_rook, black_knight, black_bishop, black_king, black_queen, black_bishop, black_knight, black_rook }
};

void display(int cursor_row, int cursor_col, int selected_piece, int orig_row, int orig_col);
void change(int, int, int, int);
void pawn(int, int);
void rook(int, int);
void horse(int, int);
void camel(int, int);
void king(int, int);
void queen(int, int);
void pawnb(int, int);
void player1();
void player2();
int check(int, int);
int check2(int, int);
void setup_termios(struct termios *old_tio);
void restore_termios(struct termios *old_tio);
char get_key();
void select_position(int *row, int *col, int *cancelled);
void select_destination(int *row, int *col, int orig_row, int orig_col, int piece, int *cancelled);
void print_unicode(int code);

int main()
{
    int x = 0;
    char ch;
    struct termios old_tio;

    setlocale(LC_ALL, ""); // Enable UTF-8 output

    printf("\n\tWELCOME TO CHESS GAME");
    printf("\n\n\t By Shreeji, Neel, Kirtan\n");
    printf("Use arrow keys to move cursor, Enter to select, Esc to cancel.\n");

    getchar();
    system("cls || clear");

    setup_termios(&old_tio);

    do
    {
        x++;
        system("cls || clear");
        display(-1, -1, 0, -1, -1); // Initial display without cursor

        if ((x % 2) == 0)
        {
            player2();
        }
        else
        {
            player1();
        }

        printf("\n\nPress Enter to continue, or any other key to exit.\n");
        ch = get_key();
    } while (ch == '\n');

    restore_termios(&old_tio);
    return 0;
}

void print_unicode(int code)
{
    if (code == ' ')
    {
        printf(" ");
        return;
    }
    // Convert Unicode code point to UTF-8
    if (code <= 0x7F)
    {
        printf("%c", code);
    }
    else if (code <= 0x7FF)
    {
        printf("%c%c", 0xC0 | (code >> 6), 0x80 | (code & 0x3F));
    }
    else if (code <= 0xFFFF)
    {
        printf("%c%c%c", 0xE0 | (code >> 12), 0x80 | ((code >> 6) & 0x3F), 0x80 | (code & 0x3F));
    }
    else
    {
        printf("%c%c%c%c", 0xF0 | (code >> 18), 0x80 | ((code >> 12) & 0x3F),
               0x80 | ((code >> 6) & 0x3F), 0x80 | (code & 0x3F));
    }
}

void display(int cursor_row, int cursor_col, int selected_piece, int orig_row, int orig_col)
{
    int i, j, k;

    printf(" ");
    for (i = 0; i < 8; i++) printf("    %d", i);
    printf("\n");

    for (k = 0; k < 8; k++)
    {
        printf("  ");
        for (i = 0; i < 42; i++) printf("-");
        printf("\n%d ", k);

        for (j = 0; j < 8; j++)
        {
            if (k == cursor_row && j == cursor_col)
                printf("||>");
            else
                printf("|| ");
            
            if (k == cursor_row && j == cursor_col && selected_piece != 0)
                print_unicode(selected_piece); // Show moving piece at cursor
            else if (k == orig_row && j == orig_col && selected_piece != 0)
                printf(" "); // Original position is empty while piece is moving
            else
                print_unicode(board[k][j]);
            
            if (k == cursor_row && j == cursor_col)
                printf("< ");
            else
                printf(" ");
        }
        printf("||\n");
    }

    printf("  ");
    for (i = 0; i < 42; i++) printf("-");
    printf("\n");
}

void change(int r1, int c1, int r2, int c2)
{
    int temp;
    temp = board[r1][c1];
    board[r1][c1] = board[r2][c2];
    board[r2][c2] = temp;
}

void pawn(int r1, int c1)
{
    pwstatus[c1]++;
    printf("Available moves: \n");

    if (pwstatus[c1] == 1)
    {
        if (board[r1 + 1][c1] == ' ')
            printf("%d%d, ", r1 + 1, c1);
        if (board[r1 + 2][c1] == ' ')
            printf("%d%d, ", r1 + 2, c1);
    }
    else
    {
        if (board[r1 + 1][c1] == ' ')
            printf("%d%d, ", r1 + 1, c1);
        if (check(r1 + 1, c1 + 1) == 1)
            printf("%d%d*, ", r1 + 1, c1 + 1);
        if (check(r1 + 1, c1 - 1) == 1)
            printf("%d%d*, ", r1 + 1, c1 - 1);
    }
}

void rook(int r1, int c1)
{
    int n;
    printf("Available moves:\nHorizontally:\n");

    n = c1;
    while (n > 0 && board[r1][n - 1] == ' ')
    {
        printf("%d%d, ", r1, n - 1);
        n--;
    }

    n = c1;
    while (n < 7 && board[r1][n + 1] == ' ')
    {
        printf("%d%d, ", r1, n + 1);
        n++;
    }

    printf("\nVertically:\n");
    n = r1;
    while (n > 0 && board[n - 1][c1] == ' ')
    {
        printf("%d%d, ", n - 1, c1);
        n--;
    }

    n = r1;
    while (n < 7 && board[n + 1][c1] == ' ')
    {
        printf("%d%d, ", n + 1, c1);
        n++;
    }
}

void horse(int r1, int c1)
{
    printf("Available moves: ");
    if (r1 + 2 <= 7 && c1 + 1 <= 7 && board[r1 + 2][c1 + 1] == ' ')
        printf("%d%d, ", r1 + 2, c1 + 1);
    if (r1 + 2 <= 7 && c1 - 1 >= 0 && board[r1 + 2][c1 - 1] == ' ')
        printf("%d%d, ", r1 + 2, c1 - 1);
    if (r1 + 1 <= 7 && c1 + 2 <= 7 && board[r1 + 1][c1 + 2] == ' ')
        printf("%d%d, ", r1 + 1, c1 + 2);
    if (r1 - 1 >= 0 && c1 + 2 <= 7 && board[r1 - 1][c1 + 2] == ' ')
        printf("%d%d, ", r1 - 1, c1 + 2);
    if (r1 - 2 >= 0 && c1 - 1 >= 0 && board[r1 - 2][c1 - 1] == ' ')
        printf("%d%d, ", r1 - 2, c1 - 1);
    if (r1 - 2 >= 0 && c1 + 1 <= 7 && board[r1 - 2][c1 + 1] == ' ')
        printf("%d%d, ", r1 - 2, c1 + 1);
    if (r1 + 1 <= 7 && c1 - 2 >= 0 && board[r1 + 1][c1 - 2] == ' ')
        printf("%d%d, ", r1 + 1, c1 - 2);
    if (r1 - 1 >= 0 && c1 - 2 >= 0 && board[r1 - 1][c1 - 2] == ' ')
        printf("%d%d, ", r1 - 1, c1 - 2);
    printf("\n");
}

void camel(int r1, int c1)
{
    int a, b;
    printf("Available moves:\n");

    a = 1; b = 1;
    while (r1 - a >= 0 && c1 + b <= 7 && board[r1 - a][c1 + b] == ' ')
    {
        printf("%d%d, ", r1 - a, c1 + b);
        a++; b++;
    }

    a = 1; b = 1;
    while (r1 + a <= 7 && c1 - b >= 0 && board[r1 + a][c1 - b] == ' ')
    {
        printf("%d%d, ", r1 + a, c1 - b);
        a++; b++;
    }

    a = 1; b = 1;
    while (r1 + a <= 7 && c1 + b <= 7 && board[r1 + a][c1 + b] == ' ')
    {
        printf("%d%d, ", r1 + a, c1 + b);
        a++; b++;
    }

    a = 1; b = 1;
    while (r1 - a >= 0 && c1 - b >= 0 && board[r1 - a][c1 - b] == ' ')
    {
        printf("%d%d, ", r1 - a, c1 - b);
        a++; b++;
    }
}

void king(int r1, int c1)
{
    printf("Available moves: ");
    if (c1 + 1 <= 7 && board[r1][c1 + 1] == ' ')
        printf("%d%d, ", r1, c1 + 1);
    if (c1 - 1 >= 0 && board[r1][c1 - 1] == ' ')
        printf("%d%d, ", r1, c1 - 1);
    if (r1 + 1 <= 7 && board[r1 + 1][c1] == ' ')
        printf("%d%d, ", r1 + 1, c1);
    if (r1 - 1 >= 0 && board[r1 - 1][c1] == ' ')
        printf("%d%d, ", r1 - 1, c1);
    if (r1 + 1 <= 7 && c1 + 1 <= 7 && board[r1 + 1][c1 + 1] == ' ')
        printf("%d%d, ", r1 + 1, c1 + 1);
    if (r1 - 1 >= 0 && c1 - 1 >= 0 && board[r1 - 1][c1 - 1] == ' ')
        printf("%d%d, ", r1 - 1, c1 - 1);
    if (r1 - 1 >= 0 && c1 + 1 <= 7 && board[r1 - 1][c1 + 1] == ' ')
        printf("%d%d, ", r1 - 1, c1 + 1);
    if (r1 + 1 <= 7 && c1 - 1 >= 0 && board[r1 + 1][c1 - 1] == ' ')
        printf("%d%d, ", r1 + 1, c1 - 1);
    printf("\n");
}

void queen(int r1, int c1)
{
    int x, y, a, b;
    printf("Available moves:\nHorizontal: ");

    y = 1;
    while (c1 - y >= 0 && board[r1][c1 - y] == ' ')
    {
        printf("%d%d, ", r1, c1 - y);
        y++;
    }

    y = 1;
    while (c1 + y <= 7 && board[r1][c1 + y] == ' ')
    {
        printf("%d%d, ", r1, c1 + y);
        y++;
    }

    printf("Vertical: ");
    x = 1;
    while (r1 - x >= 0 && board[r1 - x][c1] == ' ')
    {
        printf("%d%d, ", r1 - x, c1);
        x++;
    }

    x = 1;
    while (r1 + x <= 7 && board[r1 + x][c1] == ' ')
    {
        printf("%d%d, ", r1 + x, c1);
        x++;
    }

    printf("Diagonally: ");
    a = 1; b = 1;
    while (r1 - a >= 0 && c1 + b <= 7 && board[r1 - a][c1 + b] == ' ')
    {
        printf("%d%d, ", r1 - a, c1 + b);
        a++; b++;
    }

    a = 1; b = 1;
    while (r1 + a <= 7 && c1 - b >= 0 && board[r1 + a][c1 - b] == ' ')
    {
        printf("%d%d, ", r1 + a, c1 - b);
        a++; b++;
    }

    a = 1; b = 1;
    while (r1 + a <= 7 && c1 + b <= 7 && board[r1 + a][c1 + b] == ' ')
    {
        printf("%d%d, ", r1 + a, c1 + b);
        a++; b++;
    }

    a = 1; b = 1;
    while (r1 - a >= 0 && c1 - b >= 0 && board[r1 - a][c1 - b] == ' ')
    {
        printf("%d%d, ", r1 - a, c1 - b);
        a++; b++;
    }
}

void pawnb(int r1, int c1)
{
    pbstatus[c1]++;
    printf("Available moves: \n");

    if (pbstatus[c1] == 1)
    {
        if (board[r1 - 1][c1] == ' ')
            printf("%d%d, ", r1 - 1, c1);
        if (board[r1 - 2][c1] == ' ')
            printf("%d%d, ", r1 - 2, c1);
    }
    else
    {
        if (board[r1 - 1][c1] == ' ')
            printf("%d%d, ", r1 - 1, c1);
        if (check2(r1 - 1, c1 - 1) == 1)
            printf("%d%d*, ", r1 - 1, c1 - 1);
        if (check2(r1 - 1, c1 + 1) == 1)
            printf("%d%d*, ", r1 - 1, c1 + 1);
    }
}

void setup_termios(struct termios *old_tio)
{
    struct termios new_tio;
    tcgetattr(STDIN_FILENO, old_tio);
    new_tio = *old_tio;
    new_tio.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
}

void restore_termios(struct termios *old_tio)
{
    tcsetattr(STDIN_FILENO, TCSANOW, old_tio);
}

char get_key()
{
    char c = getchar();
    if (c == '\x1B') // Escape sequence
    {
        if (getchar() == '[')
        {
            c = getchar();
            switch (c)
            {
                case 'A': return 'U'; // Up
                case 'B': return 'D'; // Down
                case 'C': return 'R'; // Right
                case 'D': return 'L'; // Left
            }
        }
        return '\x1B'; // Esc key
    }
    return c;
}

void select_position(int *row, int *col, int *cancelled)
{
    int r = 0, c = 0;
    char key;
    *cancelled = 0;

    while (1)
    {
        system("cls || clear");
        display(r, c, 0, -1, -1);
        printf("Selecting position (Arrow keys to move, Enter to select, Esc to cancel): \n");
        key = get_key();

        if (key == 'U' && r > 0) r--;
        else if (key == 'D' && r < 7) r++;
        else if (key == 'R' && c < 7) c++;
        else if (key == 'L' && c > 0) c--;
        else if (key == '\n')
        {
            if (board[r][c] != ' ') // Allow any non-empty square for debugging
            {
                printf("Selected: ");
                print_unicode(board[r][c]);
                printf(" at (%d,%d) [Debug: Code=0x%X]\n", r, c, board[r][c]);
                *row = r;
                *col = c;
                return;
            }
            else
            {
                printf("Invalid: Empty square at (%d,%d). Select a piece.\n", r, c);
                getchar(); // Pause
            }
        }
        else if (key == '\x1B')
        {
            *cancelled = 1;
            return;
        }
    }
}

void select_destination(int *row, int *col, int orig_row, int orig_col, int piece, int *cancelled)
{
    int r = orig_row, c = orig_col;
    char key;
    *cancelled = 0;

    while (1)
    {
        system("cls || clear");
        display(r, c, piece, orig_row, orig_col);
        printf("Selecting destination (Arrow keys to move, Enter to confirm, Esc to cancel): \n");
        key = get_key();

        if (key == 'U' && r > 0) r--;
        else if (key == 'D' && r < 7) r++;
        else if (key == 'R' && c < 7) c++;
        else if (key == 'L' && c > 0) c--;
        else if (key == '\n')
        {
            *row = r;
            *col = c;
            return;
        }
        else if (key == '\x1B')
        {
            *cancelled = 1;
            return;
        }
    }
}

void player1()
{
    int r1, c1, r2, c2, cancelled, piece;

    printf("\nPLAYER 1 - White Pieces (Debug: Any piece)\n");
again1:
    select_position(&r1, &c1, &cancelled);
    if (cancelled)
    {
        printf("Selection cancelled.\n");
        getchar();
        goto again1;
    }

    piece = board[r1][c1];
    board[r1][c1] = ' '; // Temporarily remove piece for display

    system("cls || clear");
    display(r1, c1, piece, r1, c1);
    switch (piece)
    {
        case white_pawn: pawn(r1, c1); break;
        case white_rook: rook(r1, c1); break;
        case white_knight: horse(r1, c1); break;
        case white_bishop: camel(r1, c1); break;
        case white_king: king(r1, c1); break;
        case white_queen: queen(r1, c1); break;
        case black_pawn: pawnb(r1, c1); break;
        case black_rook: rook(r1, c1); break;
        case black_knight: horse(r1, c1); break;
        case black_bishop: camel(r1, c1); break;
        case black_king: king(r1, c1); break;
        case black_queen: queen(r1, c1); break;
    }

    printf("\nSelect destination (Arrow keys to move, Enter to confirm, Esc to cancel):\n");
    select_destination(&r2, &c2, r1, c1, piece, &cancelled);
    
    if (cancelled)
    {
        board[r1][c1] = piece; // Place piece back
        printf("Move cancelled, piece placed back at (%d,%d).\n", r1, c1);
        getchar();
        goto again1;
    }

    // Basic validation (allow move to empty or opponent's piece)
    if (board[r2][c2] == ' ' || check(r2, c2) || check2(r2, c2))
    {
        printf("Moved ");
        print_unicode(piece);
        printf(" to (%d,%d)\n", r2, c2);
        change(r1, c1, r2, c2);
    }
    else
    {
        board[r1][c1] = piece; // Place piece back
        printf("Invalid move! Try again.\n");
        getchar();
        goto again1;
    }
}

void player2()
{
    int r1, c1, r2, c2, cancelled, piece;

    printf("\nPLAYER 2 - Black Pieces (Debug: Any piece)\n");
again2:
    select_position(&r1, &c1, &cancelled);
    if (cancelled)
    {
        printf("Selection cancelled.\n");
        getchar();
        goto again2;
    }

    piece = board[r1][c1];
    board[r1][c1] = ' '; // Temporarily remove piece for display

    system("cls || clear");
    display(r1, c1, piece, r1, c1);
    switch (piece)
    {
        case white_pawn: pawn(r1, c1); break;
        case white_rook: rook(r1, c1); break;
        case white_knight: horse(r1, c1); break;
        case white_bishop: camel(r1, c1); break;
        case white_king: king(r1, c1); break;
        case white_queen: queen(r1, c1); break;
        case black_pawn: pawnb(r1, c1); break;
        case black_rook: rook(r1, c1); break;
        case black_knight: horse(r1, c1); break;
        case black_bishop: camel(r1, c1); break;
        case black_king: king(r1, c1); break;
        case black_queen: queen(r1, c1); break;
    }

    printf("\nSelect destination (Arrow keys to move, Enter to confirm, Esc to cancel):\n");
    select_destination(&r2, &c2, r1, c1, piece, &cancelled);
    
    if (cancelled)
    {
        board[r1][c1] = piece; // Place piece back
        printf("Move cancelled, piece placed back at (%d,%d).\n", r1, c1);
        getchar();
        goto again2;
    }

    // Basic validation (allow move to empty or opponent's piece)
    if (board[r2][c2] == ' ' || check(r2, c2) || check2(r2, c2))
    {
        printf("Moved ");
        print_unicode(piece);
        printf(" to (%d,%d)\n", r2, c2);
        change(r1, c1, r2, c2);
    }
    else
    {
        board[r1][c1] = piece; // Place piece back
        printf("Invalid move! Try again.\n");
        getchar();
        goto again2;
    }
}

int check(int x, int y)
{
    if (x < 0 || x > 7 || y < 0 || y > 7) return 0;
    switch (board[x][y])
    {
        case black_pawn:
        case black_rook:
        case black_knight:
        case black_bishop:
        case black_king:
        case black_queen: return 1;
        default: return 0;
    }
}

int check2(int x, int y)
{
    if (x < 0 || x > 7 || y < 0 || y > 7) return 0;
    switch (board[x][y])
    {
        case white_pawn:
        case white_rook:
        case white_knight:
        case white_bishop:
        case white_king:
        case white_queen: return 1;
        default: return 0;
    }
}
