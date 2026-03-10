#include <graphics.h>
#include <windows.h>
#include <algorithm>
#include <random>
#include <vector>
#include <cstdio>
#include <cstring>

using namespace std;

struct card {
    int rank;   // 1..13
    int suit;   // 1..4
};

enum bet_state {
    NO_BET,
    BET_INCREMENT,
    BET_MAX,
    BET_INITIATED,
    BET_FINISH
};

static bool hold[5] = {false, false, false, false, false};

static int on_bet = NO_BET;
static int drawState = 0;        // 0 = antes de revelar, 1 = selecionar holds, 2 = mão final
static int bet_select = 0;
static int game = 1;
static int result = 0;
static int credits = 500;
static int pos = 1;

static vector<card> deck;
static vector<card> hand;
static int deckPos = 0;

static const int CARD_W = 106;
static const int CARD_H = 166;
static const int CARD_Y1 = 217;
static const int CARD_Y2 = 383;
static const int CARD_X0 = 20;
static const int CARD_STEP = 123;

static bool plusLatch = true;
static bool mLatch = true;
static bool spaceLatch = true;
static bool leftLatch = true;
static bool rightLatch = true;
static bool eLatch = true;

bool KEYDOWN(int vk) {
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

bool keyPressedOnce(int vk, bool& latch) {
    if (KEYDOWN(vk)) {
        if (latch) {
            latch = false;
            return true;
        }
    } else {
        latch = true;
    }
    return false;
}

bool isRedSuit(int suit) {
    return suit == 1 || suit == 2;
}

bool PointInRect(int mx, int my, int left, int top, int right, int bottom){
    return mx >= left && mx <= right && my >= top && my <= bottom;
}

/* ---------- Naipes ---------- */

void DrawHearts(int x, int y, int s) {
    fillellipse(x - s/2, y - s/3, s/2, s/2);
    fillellipse(x + s/2, y - s/3, s/2, s/2);
    int p[6] = { x - s, y - s/4, x + s, y - s/4, x, y + s };
    fillpoly(3, p);
}

void DrawDiamonds(int x, int y, int s) {
    int p[8] = { x, y - s, x + s, y, x, y + s, x - s, y };
    fillpoly(4, p);
}

void DrawClubs(int x, int y, int s) {
    fillellipse(x, y - s/2, s/2, s/2);
    fillellipse(x - s/2, y + s/6, s/2, s/2);
    fillellipse(x + s/2, y + s/6, s/2, s/2);
    int p[6] = { x - 4, y, x + 4, y, x, y + s };
    fillpoly(3, p);
}

void DrawSpades(int x, int y, int s) {
    int p[6] = { x, y - s, x + s, y, x - s, y };
    fillpoly(3, p);
    fillellipse(x - s/2, y, s/2, s/2);
    fillellipse(x + s/2, y, s/2, s/2);
    int t[6] = { x - 4, y, x + 4, y, x, y + s };
    fillpoly(3, t);
}

void DrawSuitSymbol(int suit, int x, int y, int size) {
    if (suit == 1) DrawHearts(x, y, size);
    if (suit == 2) DrawDiamonds(x, y, size);
    if (suit == 3) DrawClubs(x, y, size);
    if (suit == 4) DrawSpades(x, y, size);
}

/* ---------- Rank ---------- */

const char* rankText(int rank) {
    switch(rank) {
        case 1:  return "A";
        case 11: return "J";
        case 12: return "Q";
        case 13: return "K";
        default: return "";
    }
}

void DrawA(int x, int y, int s) {
    int left[8] = {
        x, y + s,
        x + s/6, y + s,
        x + s/2, y,
        x + s/3, y
    };
    fillpoly(4, left);

    int right[8] = {
        x + s/2, y,
        x + 5*s/6, y + s,
        x + s, y + s,
        x + 2*s/3, y
    };
    fillpoly(4, right);

    int bar[8] = {
        x + s/4, y + s/2,
        x + 3*s/4, y + s/2,
        x + 3*s/4, y + 2*s/3,
        x + s/4, y + 2*s/3
    };
    fillpoly(4, bar);
}

void DrawRankText(int rank, int x, int y) {
    char txt[3] = {0};

    if (rank == 1) {
        DrawA(x, y, 18);
        return;
    }

    if (rank >= 2 && rank <= 10) sprintf(txt, "%d", rank);
    else sprintf(txt, "%s", rankText(rank));

    setbkcolor(WHITE);
    outtextxy(x, y, txt);
}

/* ---------- Baralho ---------- */

void BuildAndShuffleDeck(mt19937& rng) {
    deck.clear();

    for (int s = 1; s <= 4; s++) {
        for (int r = 1; r <= 13; r++) {
            deck.push_back({r, s});
        }
    }

    shuffle(deck.begin(), deck.end(), rng);
    deckPos = 0;
}

void DealInitialHand() {
    hand.clear();
    for (int i = 0; i < 5; i++) {
        hand.push_back(deck[deckPos++]);
    }
}

void DrawReplacementCards() {
    for (int i = 0; i < 5; i++) {
        if (!hold[i]) {
            hand[i] = deck[deckPos++];
        }
    }
}

void ResetRoundState() {
    for (int i = 0; i < 5; i++) hold[i] = false;
    drawState = 0;
    on_bet = NO_BET;
    bet_select = 0;
    result = 0;
    pos = 1;
}

/* ---------- Avaliação ---------- */

vector<int> extractRanks(const vector<card>& h) {
    vector<int> r;
    for (int i = 0; i < 5; i++) r.push_back(h[i].rank);
    return r;
}

int countPairs(const int counts[14]) {
    int pairs = 0;
    for (int r = 1; r <= 13; r++) {
        if (counts[r] == 2) pairs++;
    }
    return pairs;
}

bool isFlush(const vector<card>& h) {
    for (int i = 1; i < 5; i++) {
        if (h[i].suit != h[0].suit) return false;
    }
    return true;
}

bool isRoyalRanks(vector<int> r) {
    sort(r.begin(), r.end());
    return r[0] == 1 && r[1] == 10 && r[2] == 11 && r[3] == 12 && r[4] == 13;
}

bool isStraight(vector<int> r) {
    sort(r.begin(), r.end());

    if (r[0] == 1 && r[1] == 2 && r[2] == 3 && r[3] == 4 && r[4] == 5) return true;
    if (isRoyalRanks(r)) return true;

    if (r[0] == 1) r[0] = 14;
    sort(r.begin(), r.end());

    for (int i = 0; i < 4; i++) {
        if (r[i + 1] != r[i] + 1) return false;
    }
    return true;
}

int evaluate(const vector<card>& h) {
    int counts[14] = {0};
    vector<int> ranks = extractRanks(h);

    for (int i = 0; i < 5; i++) counts[h[i].rank]++;

    bool flush = isFlush(h);
    bool straight = isStraight(ranks);

    bool four = false;
    bool three = false;
    bool jacksOrBetter = false;
    int pairs = countPairs(counts);

    for (int r = 1; r <= 13; r++) {
        if (counts[r] == 4) four = true;
        if (counts[r] == 3) three = true;
        if (counts[r] == 2 && (r == 1 || r >= 11)) jacksOrBetter = true;
    }

    if (flush && isRoyalRanks(ranks)) return 9;
    if (flush && straight) return 8;
    if (four) return 7;
    if (three && pairs == 1) return 6;
    if (flush) return 5;
    if (straight) return 4;
    if (three) return 3;
    if (pairs == 2) return 2;
    if (jacksOrBetter) return 1;
    return 0;
}

/*int payout(int evaluation, int bet) {
    switch(evaluation) {
        case 1: return 1 * bet;
        case 2: return 2 * bet;
        case 3: return 3 * bet;
        case 4: return 4 * bet;
        case 5: return 6 * bet;
        case 6: return 9 * bet;
        case 7: return 25 * bet;
        case 8: return 50 * bet;
        case 9: return (bet < 5) ? (250 * bet) : 4000;
        default: return 0;
    }
}*/

int payout(int evaluation, int bet) {
    if (bet < 1) return 0;
    if (bet > 5) bet = 5;

    static const int pay[10][6] = {
        {0,   0,   0,   0,    0,    0},    // 0 = Nothing
        {0,   1,   2,   3,    4,    5},    // 1 = Jacks or Better
        {0,   2,   4,   6,    8,   10},    // 2 = Two Pairs
        {0,   3,   6,   9,   12,   15},    // 3 = Three of a Kind
        {0,   4,   8,  12,   16,   20},    // 4 = Straight
        {0,   6,  12,  18,   24,   30},    // 5 = Flush
        {0,   9,  18,  27,   36,   45},    // 6 = Full House
        {0,  25,  50,  75,  100,  125},    // 7 = Four of a Kind
        {0,  50, 100, 150,  200,  250},    // 8 = Straight Flush
        {0, 250, 500, 750, 1000, 4000}     // 9 = Royal Flush
    };

    return pay[evaluation][bet];
}

/* ---------- Desenho das cartas ---------- */

void DrawCardShapeAtX(int x) {
    int cardpoly[32];

    cardpoly[0] = x;      cardpoly[1] = 380;
    cardpoly[2] = x;      cardpoly[3] = 220;
    cardpoly[4] = x + 1;  cardpoly[5] = 219;
    cardpoly[6] = x + 2;  cardpoly[7] = 218;
    cardpoly[8] = x + 3;  cardpoly[9] = 217;
    cardpoly[10] = x + 103; cardpoly[11] = 217;
    cardpoly[12] = x + 104; cardpoly[13] = 218;
    cardpoly[14] = x + 105; cardpoly[15] = 219;
    cardpoly[16] = x + 106; cardpoly[17] = 220;
    cardpoly[18] = x + 106; cardpoly[19] = 380;
    cardpoly[20] = x + 105; cardpoly[21] = 381;
    cardpoly[22] = x + 104; cardpoly[23] = 382;
    cardpoly[24] = x + 103; cardpoly[25] = 383;
    cardpoly[26] = x + 3;   cardpoly[27] = 383;
    cardpoly[28] = x + 2;   cardpoly[29] = 382;
    cardpoly[30] = x + 1;   cardpoly[31] = 381;

    fillpoly(16, cardpoly);
}

void DrawCardBackAtX(int x) {
    setbkcolor(RED);
    setfillstyle(XHATCH_FILL, WHITE);
    setcolor(WHITE);
    DrawCardShapeAtX(x);
    setfillstyle(SOLID_FILL, BLACK);
    setbkcolor(BLACK);
}

void DrawCardFrontAtX(int x) {
    setbkcolor(BLUE);
    setfillstyle(SOLID_FILL, WHITE);
    setcolor(WHITE);
    DrawCardShapeAtX(x);
    setfillstyle(SOLID_FILL, BLACK);
    setbkcolor(BLACK);
}

void DrawCardBack(int n) {
    int x = CARD_X0 + CARD_STEP * n;
    DrawCardBackAtX(x);
}

void DrawScaledCardShapeAtX(int centerX, int visibleWidth, int yTop, int yBottom, int fillPattern, int fillColor, int borderColor, int bkColorValue){
    if (visibleWidth < 2) visibleWidth = 2;

    int half = visibleWidth / 2;
    int left = centerX - half;
    int right = centerX + half;

    int poly[32] = {
        left,      yBottom,
        left,      yTop + 3,
        left + 1,  yTop + 2,
        left + 2,  yTop + 1,
        left + 3,  yTop,
        right - 3, yTop,
        right - 2, yTop + 1,
        right - 1, yTop + 2,
        right,     yTop + 3,
        right,     yBottom - 3,
        right - 1, yBottom - 2,
        right - 2, yBottom - 1,
        right - 3, yBottom,
        left + 3,  yBottom,
        left + 2,  yBottom - 1,
        left + 1,  yBottom - 2
    };

    setbkcolor(bkColorValue);
    setfillstyle(fillPattern, fillColor);
    setcolor(borderColor);
    fillpoly(16, poly);
}

void DrawCard(int n, int rank, int suit) {
    int x = CARD_X0 + CARD_STEP * n;

    DrawCardFrontAtX(x);

    int color = isRedSuit(suit) ? RED : BLACK;
    setcolor(color);
    setfillstyle(SOLID_FILL, color);

    DrawRankText(rank, x + 8, 224);
    DrawSuitSymbol(suit, x + 22, 236, 8);
    DrawSuitSymbol(suit, x + 53, 300, 24);
}

void DrawDeck(const vector<card>& h) {
    for (int i = 0; i < 5; i++) {
        DrawCard(i, h[i].rank, h[i].suit);
    }
}

void DrawBackDeck() {
    for (int i = 0; i < 5; i++) {
        DrawCardBack(i);
    }
}

void DrawSelectedCardOutline(){
    int x = CARD_X0 + CARD_STEP * (pos - 1);
    setcolor(YELLOW);
    rectangle(x - 2, CARD_Y1 - 2, x + CARD_W + 2, CARD_Y2 + 2);
}

/* ---------- UI ---------- */

void DisplayPayColumn(int select, int target, int left, int right,
                      const char* a, const char* b, const char* c,
                      const char* d, const char* e, const char* f,
                      const char* g, const char* h, const char* i) {
    if (select == target) {
        setfillstyle(SOLID_FILL, RED);
        setbkcolor(RED);
    } else {
        setfillstyle(SOLID_FILL, BLACK);
        setbkcolor(BLACK);
    }

    bar(left + 1, 11, right - 1, 159);

    int tx = left + 12;
    outtextxy(tx, 12,  (char*)a);
    outtextxy(tx, 28,  (char*)b);
    outtextxy(tx, 44,  (char*)c);
    outtextxy(tx, 60,  (char*)d);
    outtextxy(tx, 76,  (char*)e);
    outtextxy(tx, 92,  (char*)f);
    outtextxy(tx, 108, (char*)g);
    outtextxy(tx, 124, (char*)h);
    outtextxy(tx, 140, (char*)i);
}

void DisplayNumbers() {
    setfillstyle(SOLID_FILL, BLACK);
    setcolor(YELLOW);
    bar(20, 10, getmaxx() - 20, 160);

    setbkcolor(BLACK);
    outtextxy(24, 12,  "Royal Flush");
    outtextxy(24, 28,  "Straight Flush");
    outtextxy(24, 44,  "Four of a Kind");
    outtextxy(24, 60,  "Full House");
    outtextxy(24, 76,  "Flush");
    outtextxy(24, 92,  "Straight");
    outtextxy(24, 108, "Three of a Kind");
    outtextxy(24, 124, "Two Pairs");
    outtextxy(24, 140, "Jacks or Better");

    rectangle(20, 10, getmaxx() - 20, 160);
    line(getmaxx() - 500, 10, getmaxx() - 500, 160);
    line(getmaxx() - 468, 10, getmaxx() - 468, 160);
    line(getmaxx() - 368, 10, getmaxx() - 368, 160);
    line(getmaxx() - 268, 10, getmaxx() - 268, 160);
    line(getmaxx() - 160, 10, getmaxx() - 160, 160);
}

void DisplayButtons() {
    setcolor(BLACK);
    setfillstyle(SOLID_FILL, YELLOW);

    bar(20, getmaxy() - 40, 90, getmaxy() - 20);
    rectangle(20, getmaxy() - 40, 90, getmaxy() - 20);

    bar(110, getmaxy() - 40, 180, getmaxy() - 20);
    rectangle(110, getmaxy() - 40, 180, getmaxy() - 20);

    bar(getmaxx() - 140, getmaxy() - 40, getmaxx() - 90, getmaxy() - 20);
    rectangle(getmaxx() - 140, getmaxy() - 40, getmaxx() - 90, getmaxy() - 20);

    bar(getmaxx() - 80, getmaxy() - 40, getmaxx() - 20, getmaxy() - 20);
    rectangle(getmaxx() - 80, getmaxy() - 40, getmaxx() - 20, getmaxy() - 20);

    setbkcolor(YELLOW);
    outtextxy(30, getmaxy() - 38, "Bet one");
    outtextxy(120, getmaxy() - 38, "Bet max");
    outtextxy(getmaxx() - 130, getmaxy() - 38, "Hold");
    outtextxy(getmaxx() - 66, getmaxy() - 38, "Draw");

    setbkcolor(BLUE);
    setcolor(WHITE);
    outtextxy(50, getmaxy() - 58, "+");
    outtextxy(140, getmaxy() - 58, "M");
    outtextxy(getmaxx() - 118, getmaxy() - 58, "E");
    outtextxy(getmaxx() - 68, getmaxy() - 58, "Space");
}

void DisplayInfo() {
    setbkcolor(BLUE);
    setcolor(WHITE);

    char credit_text[80];
    char bet_text[40];

    sprintf(credit_text, "Credits %d", credits);
    sprintf(bet_text, "Bet %d", bet_select);

    outtextxy(getmaxx()/2 - 14, getmaxy() - 64, bet_text);
    outtextxy(getmaxx() - 130, getmaxy() - 74, credit_text);
}

void DisplayStart() {
    setbkcolor(BLUE);
    setcolor(WHITE);

    if (bet_select == 0 && drawState == 0) {
        outtextxy(getmaxx()/2 - 90, getmaxy() - 84, "Press + or M to start");
    }
}

void DrawHoldUI() {
    setbkcolor(BLUE);
    setcolor(WHITE);

    char blank[200];
    strcpy(blank, "                                                                                                    ");
    outtextxy(1, 194, blank);

    char cursor[2] = "V";
    outtextxy(74 + (123 * (pos - 1)), 194, cursor);

    for (int i = 0; i < 5; i++) {
        if (hold[i]) outtextxy(123 * i + 60, 390, "Hold");
        else         outtextxy(123 * i + 60, 390, "     ");
    }
}

void DisplayResultText(int r) {
    setbkcolor(BLUE);
    setcolor(WHITE);

    char blank[80];
    strcpy(blank, "                              ");
    outtextxy(getmaxx()/2 - 80, 174, blank);

    switch(r) {
        case 1: outtextxy(getmaxx()/2 - 50, 174, "Jacks or Better"); break;
        case 2: outtextxy(getmaxx()/2 - 28, 174, "Two Pairs"); break;
        case 3: outtextxy(getmaxx()/2 - 40, 174, "Three of a Kind"); break;
        case 4: outtextxy(getmaxx()/2 - 20, 174, "Straight"); break;
        case 5: outtextxy(getmaxx()/2 - 10, 174, "Flush"); break;
        case 6: outtextxy(getmaxx()/2 - 28, 174, "Full House"); break;
        case 7: outtextxy(getmaxx()/2 - 45, 174, "Four of a Kind"); break;
        case 8: outtextxy(getmaxx()/2 - 40, 174, "Straight Flush"); break;
        case 9: outtextxy(getmaxx()/2 - 28, 174, "Royal Flush"); break;
    }
}

/* ---------- Render ---------- */

void RenderScene() {
    setbkcolor(BLUE);
    cleardevice();

    DisplayNumbers();

    DisplayPayColumn(bet_select, 1, getmaxx() - 500, getmaxx() - 468, "250","50","25","9","6","4","3","2","1");
    DisplayPayColumn(bet_select, 2, getmaxx() - 468, getmaxx() - 368, "500","100","50","18","12","8","6","4","2");
    DisplayPayColumn(bet_select, 3, getmaxx() - 368, getmaxx() - 268, "750","150","75","27","18","12","9","6","3");
    DisplayPayColumn(bet_select, 4, getmaxx() - 268, getmaxx() - 160, "1000","200","100","36","24","16","12","8","4");
    DisplayPayColumn(bet_select, 5, getmaxx() - 160, getmaxx() - 20, "4000","250","125","45","30","20","15","10","5");

    DisplayButtons();
    DisplayInfo();
    DisplayStart();

    if (drawState == 0) DrawBackDeck();
    else DrawDeck(hand);

	if (drawState >= 1) {
   		DrawHoldUI();
    	DisplayResultText(result);
    	if (on_bet == BET_INITIATED) DrawSelectedCardOutline();
	}

    if (on_bet == BET_FINISH) {
        setbkcolor(BLUE);
        setcolor(WHITE);
        outtextxy(getmaxx()/2 - 110, getmaxy() - 100, "Press Space for next hand");
    }
}

/* ---------- Animação ---------- */

void PresentFrame(int& page) {
    setvisualpage(page);
    page = 1 - page;
    setactivepage(page);
}

void AnimateFlipCard(int index, const card& c, int& page){
    int x = CARD_X0 + CARD_STEP * index;
    int centerX = x + CARD_W / 2;

    // fase 1: encolhe verso
    for (int w = CARD_W; w >= 6; w -= 12) {
        RenderScene();
        DrawScaledCardShapeAtX(centerX, w, CARD_Y1, CARD_Y2, XHATCH_FILL, WHITE, WHITE, RED);
        PresentFrame(page);
        delay(18);
    }

    // fase 2: abre frente
    for (int w = 6; w <= CARD_W; w += 12) {
        RenderScene();
        DrawScaledCardShapeAtX(centerX, w, CARD_Y1, CARD_Y2, SOLID_FILL, WHITE, WHITE, BLUE);

        if (w > CARD_W / 2) {
            int color = isRedSuit(c.suit) ? RED : BLACK;
            setcolor(color);
            setfillstyle(SOLID_FILL, color);

            int offset = (CARD_W - w) / 2;
            int drawX = x + offset;

            DrawRankText(c.rank, drawX + 8, 224);
            DrawSuitSymbol(c.suit, drawX + 22, 236, 8);
            DrawSuitSymbol(c.suit, drawX + w/2, 300, 24);
        }

        PresentFrame(page);
        delay(18);
    }
}

/*void AnimateReveal(int& page) {
    for (int i = 0; i < 5; i++) {
        RenderScene();
        for (int j = i + 1; j < 5; j++) DrawCardBack(j);
        for (int j = 0; j <= i; j++) DrawCard(j, hand[j].rank, hand[j].suit);
        PresentFrame(page);
        delay(70);
    }
}*/

void AnimateReveal(int& page){
    for (int i = 0; i < 5; i++) {
        AnimateFlipCard(i, hand[i], page);
    }
}

/*void AnimateDraw(int& page, const bool oldHold[5]) {
    for (int i = 0; i < 5; i++) {
        if (!oldHold[i]) {
            RenderScene();
            for (int j = 0; j < 5; j++) {
                if (j == i) DrawCardBack(j);
                else DrawCard(j, hand[j].rank, hand[j].suit);
            }
            DrawHoldUI();
            PresentFrame(page);
            delay(80);

            RenderScene();
            DrawHoldUI();
            PresentFrame(page);
            delay(80);
        }
    }
}*/

void AnimateDraw(int& page, const bool oldHold[5]){
    for (int i = 0; i < 5; i++) {
        if (!oldHold[i]) {
            AnimateFlipCard(i, hand[i], page);
        }
    }
}

/* ---------- Interação ---------- */

int CardIndexFromMouse(int mx, int my) {
    if (my < CARD_Y1 || my > CARD_Y2) return -1;

    for (int i = 0; i < 5; i++) {
        int x1 = CARD_X0 + CARD_STEP * i;
        int x2 = x1 + CARD_W;
        if (mx >= x1 && mx <= x2) return i;
    }
    return -1;
}

void HandleMouse() {
    if (drawState != 1 || on_bet != BET_INITIATED) return;

    if (ismouseclick(WM_LBUTTONDOWN)) {
        int mx, my;
        getmouseclick(WM_LBUTTONDOWN, mx, my);
        clearmouseclick(WM_LBUTTONDOWN);

        int idx = CardIndexFromMouse(mx, my);
        if (idx >= 0) {
            hold[idx] = !hold[idx];
            pos = idx + 1;
        }
    }
}

void HandleSelectionKeys() {
    if (drawState != 1 || on_bet != BET_INITIATED) return;

    if (keyPressedOnce(VK_LEFT, leftLatch)) {
        pos--;
        if (pos < 1) pos = 5;
    }

    if (keyPressedOnce(VK_RIGHT, rightLatch)) {
        pos++;
        if (pos > 5) pos = 1;
    }

    if (keyPressedOnce(0x45, eLatch)) { // E
        hold[pos - 1] = !hold[pos - 1];
    }
}

void StartNewHand(mt19937& rng) {
    BuildAndShuffleDeck(rng);
    DealInitialHand();
    ResetRoundState();
}

void HandleButtonMouse(mt19937& rng, int& page){
    if (!ismouseclick(WM_LBUTTONDOWN)) return;

    int mx, my;
    getmouseclick(WM_LBUTTONDOWN, mx, my);
    clearmouseclick(WM_LBUTTONDOWN);

    int betOneL = 20,                betOneT = getmaxy() - 40, betOneR = 90,               betOneB = getmaxy() - 20;
    int betMaxL = 110,               betMaxT = getmaxy() - 40, betMaxR = 180,              betMaxB = getmaxy() - 20;
    int holdL   = getmaxx() - 140,   holdT   = getmaxy() - 40, holdR   = getmaxx() - 90,   holdB   = getmaxy() - 20;
    int drawL   = getmaxx() - 80,    drawT   = getmaxy() - 40, drawR   = getmaxx() - 20,   drawB   = getmaxy() - 20;

    // Bet one
    if (PointInRect(mx, my, betOneL, betOneT, betOneR, betOneB)) {
        if (drawState == 0 && bet_select < 5 && credits > 0) {
            bet_select++;
            credits--;
            on_bet = (bet_select == 5) ? BET_MAX : BET_INCREMENT;
        }
        return;
    }

    // Bet max
    if (PointInRect(mx, my, betMaxL, betMaxT, betMaxR, betMaxB)) {
        if (drawState == 0 && bet_select < 5 && credits >= (5 - bet_select)) {
            credits -= (5 - bet_select);
            bet_select = 5;
            on_bet = BET_MAX;
        }
        return;
    }

    // Hold button: alterna hold da carta selecionada por cursor
    if (PointInRect(mx, my, holdL, holdT, holdR, holdB)) {
        if (drawState == 1 && on_bet == BET_INITIATED) {
            hold[pos - 1] = !hold[pos - 1];
        }
        return;
    }

    // Draw button
	if (PointInRect(mx, my, drawL, drawT, drawR, drawB)) {
    	if (drawState == 0) {
        	if (bet_select == 0) {
            	if (credits >= 5) {
                	credits -= 5;
                	bet_select = 5;
                	on_bet = BET_MAX;

                	drawState = 1;
                	on_bet = BET_INITIATED;
                	result = evaluate(hand);
                	AnimateReveal(page);
            	}
        	}
        	else {
            	drawState = 1;
            	on_bet = BET_INITIATED;
            	result = evaluate(hand);
            	AnimateReveal(page);
        	}
    	}	
    	else if (drawState == 1 && on_bet == BET_INITIATED) {
        	bool oldHold[5];
        	for (int i = 0; i < 5; i++) oldHold[i] = hold[i];

        	DrawReplacementCards();
        	result = evaluate(hand);
        	credits += payout(result, bet_select);

        	drawState = 2;
        	on_bet = BET_FINISH;

        	AnimateDraw(page, oldHold);
    	}
    	else if (drawState == 2 && on_bet == BET_FINISH) {
        	StartNewHand(rng);
    	}
    	return;
}

    // clique direto nas cartas para HOLD
    if (drawState == 1 && on_bet == BET_INITIATED) {
        int idx = CardIndexFromMouse(mx, my);
        if (idx >= 0) {
            hold[idx] = !hold[idx];
            pos = idx + 1;
        }
    }
}

/* ---------- Main ---------- */

int main() {
    random_device rd;
    mt19937 rng(rd());

    int gd = DETECT, gm;
    initgraph(&gd, &gm, (char*)"");

    setbkcolor(BLUE);
    cleardevice();

    setactivepage(0);
    setvisualpage(1);
    cleardevice();
    setactivepage(1);
    setvisualpage(0);
    cleardevice();

    StartNewHand(rng);

    int page = 0;
    setactivepage(page);

    while (game) {
		HandleButtonMouse(rng, page);
	    HandleSelectionKeys();
	
	    if (keyPressedOnce(VK_OEM_PLUS, plusLatch) && drawState == 0 && bet_select < 5 && credits > 0) {
	        bet_select++;
	        credits--;
	        on_bet = (bet_select == 5) ? BET_MAX : BET_INCREMENT;
	    }
	
	    if (keyPressedOnce(0x4D, mLatch) && drawState == 0 && bet_select < 5 && credits >= (5 - bet_select)) {
	        credits -= (5 - bet_select);
	        bet_select = 5;
	        on_bet = BET_MAX;
	    }
	
	    if (keyPressedOnce(VK_SPACE, spaceLatch)) {

    	if (drawState == 0) {
        	if (bet_select == 0) {
            	if (credits >= 5) {
            	    credits -= 5;
             	   bet_select = 5;
            	    on_bet = BET_MAX;

            	    drawState = 1;
            	    on_bet = BET_INITIATED;
            	    result = evaluate(hand);
            	    AnimateReveal(page);
            	}
        	}
        else {
            on_bet = BET_INITIATED;
            drawState = 1;
            result = evaluate(hand);
            AnimateReveal(page);
        	}
    	}
    	else if (drawState == 1 && on_bet == BET_INITIATED) {
        	bool oldHold[5];
        	for (int i = 0; i < 5; i++) oldHold[i] = hold[i];

        	DrawReplacementCards();
        	result = evaluate(hand);
        	credits += payout(result, bet_select);

        	drawState = 2;
        	on_bet = BET_FINISH;

        	AnimateDraw(page, oldHold);
    	}
    	else if (drawState == 2 && on_bet == BET_FINISH) {
        	StartNewHand(rng);
    	}
	}
	
	    RenderScene();
	    PresentFrame(page);
	    delay(16);
    }

    closegraph();
    return 0;
}