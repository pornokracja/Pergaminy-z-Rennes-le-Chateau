// Pergaminy z Rennes-le-Chateau - mala gra przygodowa
// WinAPI + GDI, bez zewnetrznych bibliotek
// Kompilacja: g++ -std=c++17 -O2 pergaminy.cpp -o gra.exe -lgdi32 -static
// Sterowanie: WASD / strzalki - ruch, E / spacja - dzialaj, J - dziennik

#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <set>
#include <string>
#include <vector>

// ---------------------------------------------------------------- stale

const int SW = 800, SH = 600;       // rozmiar okna
const int HUD = 44;                 // wysokosc paska na gorze
const int VIEW_H = SH - HUD;        // wysokosc widoku gry
const int PX = 2;                   // jeden "pixel art" = 2x2 prawdziwe
const int TILE = 16 * PX;           // kafelek ma 32x32
const float DT = 0.016f;            // krok czasu (okolo 60 fps)

uint32_t pixels[SW * SH];
bool keys[256], prevKeys[256];
bool running = true;

// ---------------------------------------------------------------- mapa

enum Tile {
    WALL, FLOOR, WINDOW, PEW, ALTAR, CANDLE, STAND, RUBBLE, SIGIL, TRAP,
    SHELF, DESK, STAIRS, SEALED, OPEN, PEDESTAL
};

struct Map {
    int w = 0, h = 0;
    std::vector<int> t;

    void init(int W, int H, int v) { w = W; h = H; t.assign(W * H, v); }
    int get(int x, int y) const {
        if (x < 0 || y < 0 || x >= w || y >= h) return WALL;
        return t[y * w + x];
    }
    void set(int x, int y, int v) {
        if (x >= 0 && y >= 0 && x < w && y < h) t[y * w + x] = v;
    }
};

// po czym mozna chodzic
bool walkable(int t) { return t == FLOOR || t == SIGIL || t == TRAP || t == OPEN; }

// ---------------------------------------------------------------- stan gry

enum State { TITLE, PLAY, DIALOG, JOURNAL, CIPHER, CHOICE, ENDING };
State state = TITLE;

struct Player {
    float x = 11.5f, y = 13.5f;
    int facing = 1;                 // 0 dol, 1 gora, 2 lewo, 3 prawo
    float sanity = 100.0f;
    float anim = 0.0f;
    bool moving = false;
};

struct Guardian {
    float x = 14.5f, y = 12.5f;     // pozycja
    float tx = 14.5f, ty = 12.5f;   // dokad idzie
    float wait = 2.0f;
};

struct Flags {
    bool altar = false, trap = false, arrived = false, desk = false, door = false;
    int ending = 0;                 // 0 brak, 1 zachowane zwoje, 2 spalone
};

Player player;
Guardian guardian;
Flags flags;

// ---------------------------------------------------------------- dziennik

enum LoreId {
    L_INTRO, L_ALTAR, L_SIGIL, L_DESK, L_DOOR, L_WINDOW, L_MICHAEL,
    L_SAECULUM, L_MAROZIA, L_GOETIA, L_PAIMON, L_CHURCH, L_CAESAR,
    LORE_COUNT
};

struct Lore {
    const char* category;
    const char* title;
    const char* text;
};

const Lore lore[LORE_COUNT] = {
    { "Fabuła", "Zlecenie Kurii",
      "Kuria w Carcassonne zleciła ci inwentaryzację opuszczonego kościoła św. Marii Magdaleny w Rennes-le-Château. W starym spisie parafialnym figuruje jedna tajemnicza pozycja: Pergaminy Saunière’a.\nNa marginesie ktoś dopisał: „Nie szukaj przy ołtarzu. Szukaj pod pieczęcią.”" },
    { "Fabuła", "Karta z ołtarza",
      "„Ostatnia msza. Trzy świece płonęły, gdy zamykałem księgę. Kto chce szukać, niech wpierw znajdzie znak pięciu ran zbawiciela. Gdy Strażnik je rozum, a myśli runą, lampa zostaje ci jedną lubą.\nTrzymaj się światła i strzeż się wzroku Asmodeusza.”" },
    { "Fabuła", "Pieczęć w nawie",
      "Pod kamienną płytą z wyrytym znakiem pięciu ran ukryte było wejście do podziemi probostwa." },
    { "Fabuła", "Notatki ks. Saunière'a",
      "„Pergaminy ukryłem w skryptorium za drzwiami z pieczęcią. Hasło zaszyfrowałem Cezarem: każdą literę przesunąłem w alfabecie o tyle miejsc, ile świec płonęło przy ołtarzu podczas ostatniej mszy.\nZapisane imię należy do klucza dawnych wieków.”" },
    { "Fabuła", "Pieczęć złamana",
      "Pieczęć ustąpiła po odczytaniu imienia TEODORA. Za drzwiami czeka sala z piedestałem, na której spoczywają zwoje." },
    { "Historia", "Witraż: Sąd Ostateczny",
      "Sąd Ostateczny. Po lewej zbawieni, po prawej potępieni. Wśród potępionych rozpoznajesz postać w papieskiej tiarze i dwie kobiety u jej boku. Ktoś zeskrobał twarze, ale nie napis: SAECULUM OBSCURUM. Pod spodem, drobnym pismem, ktoś wyrył inicjały B.S." },
    { "Historia", "Witraż: Archanioł Michał",
      "Archanioł Michał waży dusze. W szalce potępionych leżą zwoje pergaminu, których szukasz." },
    { "Historia", "Saeculum obscurum",
      "Saeculum obscurum (ok. 904-964) to okres, w którym papiestwo pozostawało pod silnym wpływem rzymskich rodów arystokratycznych, zwłaszcza Teofylaktów.\nDawniej nazywano go „pornokracją” (gr. pórne - nierządnica, krátos - władza), czyli rządami kobiet. Wątki te splatają się w tajemnicach wzgórza w Rennes-le-Château." },
    { "Historia", "Ród i Zagadka",
      "Tajemnice wokół Rennes-le-Château obejmują ślady dawnych królów Wizygotów, Merowingów oraz sekretne dokumenty odkryte przez księdza Bérengera Saunière’a podczas renowacji świątyni." },
    { "Demonologia", "Asmodeus i Strażnik",
      "U progu kościoła w Rennes-le-Château wita przybyłych demon Asmodeus, strażnik podziemnych skarbów i tajemnic.\nW tej opowieści jego cień i strażnik podziemi pilnują, by prawda nie wyszła na światło nieprzygotowana." },
    { "Demonologia", "Paimon",
      "W starych grymuarach Paimon jest królem i nauczycielem sztuk ukrytych. W podziemiach pod nawą jego imię nosi strażnik korytarzy, który czuwa nad rozwagą poszukiwacza." },
    { "Demonologia", "Demony w teologii",
      "Teologowie scholastyczni opisywali demony jako upadłe anioły. W tradycji ludowej Langwedocji granica między świętością a pogańską tajemnicą zaciera się w kamiennych murach." },
    { "Szyfr", "Szyfr Cezara",
      "Szyfr Cezara to klasyczny szyfr podstawieniowy: każdą literę zastępuje się inną, położoną o stałą liczbę miejsc dalej w alfabecie, używany do tajnych depesz." },
};

const char* whispers[] = {
    "...Teodora...", "...pergaminy...", "...nie oglądaj się...", "...Asmodeus patrzy...",
    "...ciemność też czyta...", "...wyrok...", "...kto szuka, ten znajdzie...",
};
const int WHISPER_COUNT = 7;

std::vector<int> journal;   // id odczytanych wpisow
int journalSel = 0;

// ---------------------------------------------------------------- poziomy

// co sie dzieje po nacisnieciu E na danym polu
enum ActKind { A_READ, A_ALTAR, A_SIGIL, A_DESK, A_STAIRS, A_DOOR, A_PEDESTAL };

struct Act {
    ActKind kind;
    int lore = 0;                   // dla A_READ: ktory wpis
    const char* title = "";
    const char* flavor = nullptr;   // dodatkowa pierwsza strona dialogu
};

// kiedy swiatlo dziala
enum LightCond { ALWAYS, DOOR_OPEN, DOOR_SEALED };

struct Light {
    float x, y;                     // pozycja w pikselach swiata
    float radius;
    float r, g, b;
    float strength;
    bool safe;                      // czy chroni przed cieniem
    bool flame;                     // czy migocze i ma plomien
    LightCond cond;
    float seed;
};

struct Level {
    std::string name;
    Map map;
    std::vector<Light> lights;
    std::map<int, Act> acts;
    std::set<int> seen;             // pola juz odczytane
    int style = 0;                  // wyglad podlogi
    bool hasGuardian = false;
    float startX = 1.5f, startY = 1.5f;
    float guardX = 0, guardY = 0;
};

Level levels[2];
int cur = 0;                        // numer aktualnego poziomu

int key(int x, int y) { return y * 64 + x; }

void addLight(Level& L, float tx, float ty, float yoff, float rad, float strength,
              float r, float g, float b, bool safe, bool flame, LightCond cond) {
    Light l;
    l.x = tx * TILE + TILE / 2;
    l.y = ty * TILE + yoff;
    l.radius = rad * TILE;
    l.r = r; l.g = g; l.b = b;
    l.strength = strength;
    l.safe = safe;
    l.flame = flame;
    l.cond = cond;
    l.seed = tx * 7.0f + ty;
    L.lights.push_back(l);
}

// stojak ze swieca + jego swiatlo
void addStand(Level& L, int x, int y, LightCond cond = ALWAYS) {
    L.map.set(x, y, STAND);
    addLight(L, x, y, 6, 4.6f, 1.0f, 1.0f, 0.78f, 0.50f, true, true, cond);
}

void addAct(Level& L, int x, int y, ActKind kind, int loreId = 0, const char* title = "", const char* flavor = nullptr) {
    L.acts[key(x, y)] = Act{ kind, loreId, title, flavor };
}

Level buildChurch() {
    Level L;
    L.name = "KOŚCIÓŁ ŚW. MARII MAGDALENY";
    L.map.init(24, 15, FLOOR);
    L.style = 0;
    L.startX = 11.5f;
    L.startY = 13.5f;
    Map& m = L.map;

    // sciany dookola
    for (int x = 0; x < 24; x++) { m.set(x, 0, WALL); m.set(x, 14, WALL); }
    for (int y = 0; y < 15; y++) { m.set(0, y, WALL); m.set(23, y, WALL); }

    // witraze w gornej scianie, kazdy swieci innym kolorem
    int winX[6] = { 3, 7, 11, 12, 16, 20 };
    float tint[5][3] = { {0.55f, 0.32f, 1.0f}, {1.0f, 0.25f, 0.35f}, {0.30f, 0.50f, 1.0f},
                         {1.0f, 0.80f, 0.35f}, {0.30f, 0.90f, 0.60f} };
    for (int i = 0; i < 6; i++) {
        m.set(winX[i], 0, WINDOW);
        float* c = tint[winX[i] % 5];
        addLight(L, winX[i], 1, 0, 3.2f, 0.30f, c[0], c[1], c[2], false, false, ALWAYS);
    }

    // oltarz z trzema swiecami
    for (int x = 10; x <= 12; x++) {
        m.set(x, 3, ALTAR);
        m.set(x, 2, CANDLE);
        addLight(L, x, 2, 8, 3.8f, 1.0f, 1.0f, 0.78f, 0.50f, true, true, ALWAYS);
        addAct(L, x, 3, A_ALTAR);
    }

    addStand(L, 9, 4);
    addStand(L, 14, 4);
    addStand(L, 3, 12);
    addStand(L, 20, 12);

    // lawki
    for (int y = 6; y <= 10; y += 2) {
        for (int x = 3; x <= 8; x++) m.set(x, y, PEW);
        for (int x = 15; x <= 20; x++) m.set(x, y, PEW);
    }
    m.set(6, 8, FLOOR);     // przerwy w lawkach
    m.set(17, 10, FLOOR);

    int rubble[5][2] = { {2, 13}, {21, 13}, {1, 9}, {22, 5}, {17, 2} };
    for (auto& r : rubble) m.set(r[0], r[1], RUBBLE);

    // plyta z pieczecia
    m.set(11, 8, SIGIL);
    addAct(L, 11, 8, A_SIGIL);
    addAct(L, 7, 0, A_READ, L_WINDOW, "Witraż", "Światło witraża rzuca fioletowe refleksy na posadzkę.");
    addAct(L, 16, 0, A_READ, L_MICHAEL, "Witraż");
    return L;
}

Level buildArchive() {
    Level L;
    L.name = "PODZIEMIA SAUNIÈRE’A";
    L.map.init(28, 18, FLOOR);
    L.style = 1;
    L.hasGuardian = true;
    L.startX = 3.5f;
    L.startY = 15.5f;
    L.guardX = 14.5f;
    L.guardY = 12.5f;
    Map& m = L.map;

    for (int x = 0; x < 28; x++) { m.set(x, 0, WALL); m.set(x, 17, WALL); }
    for (int y = 0; y < 18; y++) { m.set(0, y, WALL); m.set(27, y, WALL); }

    // komnata z piedestalem w prawym gornym rogu
    for (int y = 1; y <= 9; y++) m.set(19, y, WALL);
    for (int x = 19; x <= 26; x++) m.set(x, 9, WALL);
    m.set(19, 5, SEALED);
    m.set(23, 3, PEDESTAL);

    // regaly
    for (int x = 2; x <= 17; x++) m.set(x, 1, SHELF);
    for (int x = 3; x <= 8; x++) { m.set(x, 4, SHELF); m.set(x, 8, SHELF); }
    for (int x = 11; x <= 16; x++) { m.set(x, 4, SHELF); m.set(x, 8, SHELF); }

    m.set(2, 15, STAIRS);
    m.set(22, 13, DESK);
    int rubble[4][2] = { {24, 16}, {26, 12}, {18, 16}, {1, 3} };
    for (auto& r : rubble) m.set(r[0], r[1], RUBBLE);

    // swiece w glownej sali
    int hall[5][2] = { {10, 6}, {10, 12}, {17, 13}, {4, 15}, {21, 12} };
    for (auto& h : hall) addStand(L, h[0], h[1]);

    // swiece w komnacie zapalaja sie dopiero po otwarciu drzwi
    int room[4][2] = { {21, 2}, {25, 2}, {21, 7}, {25, 7} };
    for (auto& r : room) addStand(L, r[0], r[1], DOOR_OPEN);
    addLight(L, 23, 3, 12, 2.8f, 0.9f, 1.0f, 0.85f, 0.45f, true, false, DOOR_OPEN);
    addLight(L, 19, 5, 16, 2.6f, 0.55f, 0.62f, 0.36f, 1.0f, false, false, DOOR_SEALED);

    addAct(L, 4, 4, A_READ, L_SAECULUM, "Księga", "Wyciągasz zakurzony tom. Karty są kruche jak liście.");
    addAct(L, 13, 4, A_READ, L_MAROZIA, "Księga", "Na grzbiecie wytłoczono dawny herb.");
    addAct(L, 5, 8, A_READ, L_GOETIA, "Księga", "Ten tom stoi osobno, przewiązany sznurem.");
    addAct(L, 12, 8, A_READ, L_PAIMON, "Księga", "Notatka o strażnikach sekretu.");
    addAct(L, 9, 1, A_READ, L_CHURCH, "Księga", "Dokumenty kurialne dotyczące parafii w Rennes-le-Château.");
    addAct(L, 15, 1, A_READ, L_CAESAR, "Księga", "Traktat o szyfrach i tajnym piśmie.");
    addAct(L, 22, 13, A_DESK);
    addAct(L, 2, 15, A_STAIRS);
    addAct(L, 19, 5, A_DOOR);
    addAct(L, 23, 3, A_PEDESTAL);
    return L;
}

bool lightOn(const Light& l) {
    if (l.cond == DOOR_OPEN) return flags.door;
    if (l.cond == DOOR_SEALED) return !flags.door;
    return true;
}

// czy punkt (w kafelkach) jest w poblizu bezpiecznego swiatla
bool nearSafeLight(float tx, float ty, float factor) {
    float px = tx * TILE, py = ty * TILE;
    for (auto& l : levels[cur].lights) {
        if (!l.safe || !lightOn(l)) continue;
        if (std::hypot(px - l.x, py - l.y) < l.radius * factor) return true;
    }
    return false;
}

bool solidAt(float fx, float fy) {
    int tx = (int)std::floor(fx);
    int ty = (int)std::floor(fy);
    return !walkable(levels[cur].map.get(tx, ty));
}

// ---------------------------------------------------------------- rysowanie: podstawy

uint32_t rgb(int r, int g, int b) {
    r = std::clamp(r, 0, 255);
    g = std::clamp(g, 0, 255);
    b = std::clamp(b, 0, 255);
    return (r << 16) | (g << 8) | b;
}
int getR(uint32_t c) { return (c >> 16) & 255; }
int getG(uint32_t c) { return (c >> 8) & 255; }
int getB(uint32_t c) { return c & 255; }

// przyciemnia / rozjasnia kolor
uint32_t mul(uint32_t c, float r, float g, float b) {
    return rgb((int)(getR(c) * r), (int)(getG(c) * g), (int)(getB(c) * b));
}

void put(int x, int y, uint32_t c) {
    if (x >= 0 && x < SW && y >= 0 && y < SH) pixels[y * SW + x] = c;
}

void clearScreen(uint32_t c) {
    std::fill(std::begin(pixels), std::end(pixels), c);
}

void rect(int x, int y, int w, int h, uint32_t c) {
    int x0 = std::max(x, 0), x1 = std::min(x + w, SW);
    int y0 = std::max(y, 0), y1 = std::min(y + h, SH);
    for (int j = y0; j < y1; j++)
        for (int i = x0; i < x1; i++) pixels[j * SW + i] = c;
}

// prostokat w "malych pikselach" wewnatrz kafelka (px, py to lewy gorny rog kafelka)
void spr(int px, int py, int x, int y, int w, int h, uint32_t c) {
    rect(px + x * PX, py + y * PX, w * PX, h * PX, c);
}

void line(int x0, int y0, int x1, int y1, uint32_t c) {
    int steps = std::max(std::abs(x1 - x0), std::abs(y1 - y0));
    if (steps == 0) { put(x0, y0, c); return; }
    for (int i = 0; i <= steps; i++) {
        float t = (float)i / steps;
        put((int)std::lround(x0 + (x1 - x0) * t), (int)std::lround(y0 + (y1 - y0) * t), c);
    }
}

void circle(int cx, int cy, int r, uint32_t c) {
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++)
            if (dx * dx + dy * dy <= r * r) put(cx + dx, cy + dy, c);
}

uint32_t hash2(int x, int y) {
    return ((uint32_t)x * 73856093u) ^ ((uint32_t)y * 19349663u);
}

uint32_t lcg(uint32_t& s) {
    s = s * 1664525u + 1013904223u;
    return s >> 8;
}

bool keyDown(int k) { return keys[k]; }
bool keyPressed(int k) { return keys[k] && !prevKeys[k]; }

// ---------------------------------------------------------------- czcionka 5x7 (ASCII 32..126)

const uint8_t FONT[95][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x5F, 0x00, 0x00},
    {0x00, 0x07, 0x00, 0x07, 0x00},
    {0x14, 0x7F, 0x14, 0x7F, 0x14},
    {0x24, 0x2A, 0x7F, 0x2A, 0x12},
    {0x23, 0x13, 0x08, 0x64, 0x62},
    {0x36, 0x49, 0x55, 0x22, 0x50},
    {0x00, 0x05, 0x03, 0x00, 0x00},
    {0x00, 0x1C, 0x22, 0x41, 0x00},
    {0x00, 0x41, 0x22, 0x1C, 0x00},
    {0x14, 0x08, 0x3E, 0x08, 0x14},
    {0x08, 0x08, 0x3E, 0x08, 0x08},
    {0x00, 0x50, 0x30, 0x00, 0x00},
    {0x08, 0x08, 0x08, 0x08, 0x08},
    {0x00, 0x60, 0x60, 0x00, 0x00},
    {0x20, 0x10, 0x08, 0x04, 0x02},
    {0x3E, 0x51, 0x49, 0x45, 0x3E},
    {0x00, 0x42, 0x7F, 0x40, 0x00},
    {0x42, 0x61, 0x51, 0x49, 0x46},
    {0x21, 0x41, 0x45, 0x4B, 0x31},
    {0x18, 0x14, 0x12, 0x7F, 0x10},
    {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3C, 0x4A, 0x49, 0x49, 0x30},
    {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36},
    {0x06, 0x49, 0x49, 0x29, 0x1E},
    {0x00, 0x36, 0x36, 0x00, 0x00},
    {0x00, 0x56, 0x36, 0x00, 0x00},
    {0x08, 0x14, 0x22, 0x41, 0x00},
    {0x14, 0x14, 0x14, 0x14, 0x14},
    {0x00, 0x41, 0x22, 0x14, 0x08},
    {0x02, 0x01, 0x51, 0x09, 0x06},
    {0x32, 0x49, 0x79, 0x41, 0x3E},
    {0x7E, 0x11, 0x11, 0x11, 0x7E},
    {0x7F, 0x49, 0x49, 0x49, 0x36},
    {0x3E, 0x41, 0x41, 0x41, 0x22},
    {0x7F, 0x41, 0x41, 0x22, 0x1C},
    {0x7F, 0x49, 0x49, 0x49, 0x41},
    {0x7F, 0x09, 0x09, 0x09, 0x01},
    {0x3E, 0x41, 0x49, 0x49, 0x7A},
    {0x7F, 0x08, 0x08, 0x08, 0x7F},
    {0x00, 0x41, 0x7F, 0x41, 0x00},
    {0x20, 0x40, 0x41, 0x3F, 0x01},
    {0x7F, 0x08, 0x14, 0x22, 0x41},
    {0x7F, 0x40, 0x40, 0x40, 0x40},
    {0x7F, 0x02, 0x0C, 0x02, 0x7F},
    {0x7F, 0x04, 0x08, 0x10, 0x7F},
    {0x3E, 0x41, 0x41, 0x41, 0x3E},
    {0x7F, 0x09, 0x09, 0x09, 0x06},
    {0x3E, 0x41, 0x51, 0x21, 0x5E},
    {0x7F, 0x09, 0x19, 0x29, 0x46},
    {0x46, 0x49, 0x49, 0x49, 0x31},
    {0x01, 0x01, 0x7F, 0x01, 0x01},
    {0x3F, 0x40, 0x40, 0x40, 0x3F},
    {0x1F, 0x20, 0x40, 0x20, 0x1F},
    {0x3F, 0x40, 0x38, 0x40, 0x3F},
    {0x63, 0x14, 0x08, 0x14, 0x63},
    {0x07, 0x08, 0x70, 0x08, 0x07},
    {0x61, 0x51, 0x49, 0x45, 0x43},
    {0x00, 0x7F, 0x41, 0x41, 0x00},
    {0x02, 0x04, 0x08, 0x10, 0x20},
    {0x00, 0x41, 0x41, 0x7F, 0x00},
    {0x04, 0x02, 0x01, 0x02, 0x04},
    {0x40, 0x40, 0x40, 0x40, 0x40},
    {0x00, 0x01, 0x02, 0x04, 0x00},
    {0x20, 0x54, 0x54, 0x54, 0x78},
    {0x7F, 0x48, 0x44, 0x44, 0x38},
    {0x38, 0x44, 0x44, 0x44, 0x20},
    {0x38, 0x44, 0x44, 0x48, 0x7F},
    {0x38, 0x54, 0x54, 0x54, 0x18},
    {0x08, 0x7E, 0x09, 0x01, 0x02},
    {0x0C, 0x52, 0x52, 0x52, 0x3E},
    {0x7F, 0x08, 0x04, 0x04, 0x78},
    {0x00, 0x44, 0x7D, 0x40, 0x00},
    {0x20, 0x40, 0x44, 0x3D, 0x00},
    {0x7F, 0x10, 0x28, 0x44, 0x00},
    {0x00, 0x41, 0x7F, 0x40, 0x00},
    {0x7C, 0x04, 0x18, 0x04, 0x78},
    {0x7C, 0x08, 0x04, 0x04, 0x78},
    {0x38, 0x44, 0x44, 0x44, 0x38},
    {0x7C, 0x14, 0x14, 0x14, 0x08},
    {0x08, 0x14, 0x14, 0x18, 0x7C},
    {0x7C, 0x08, 0x04, 0x04, 0x08},
    {0x48, 0x54, 0x54, 0x54, 0x20},
    {0x04, 0x3F, 0x44, 0x40, 0x20},
    {0x3C, 0x40, 0x40, 0x20, 0x7C},
    {0x1C, 0x20, 0x40, 0x20, 0x1C},
    {0x3C, 0x40, 0x30, 0x40, 0x3C},
    {0x44, 0x28, 0x10, 0x28, 0x44},
    {0x0C, 0x50, 0x50, 0x50, 0x3C},
    {0x44, 0x64, 0x54, 0x4C, 0x44},
    {0x00, 0x08, 0x36, 0x41, 0x00},
    {0x00, 0x00, 0x7F, 0x00, 0x00},
    {0x00, 0x41, 0x36, 0x08, 0x00},
    {0x08, 0x08, 0x2A, 0x1C, 0x08}
};
// accent: 0 brak, 1 kreska (ć ń ó ś ź), 2 kropka (ż), 3 ogonek (ą ę), 4 przekreslenie (ł)
struct Fold { uint32_t cp; char base; int accent; };

const Fold FOLD[] = {
    {0x105, 'a', 3}, {0x104, 'A', 3}, {0x107, 'c', 1}, {0x106, 'C', 1},
    {0x119, 'e', 3}, {0x118, 'E', 3}, {0x142, 'l', 4}, {0x141, 'L', 4},
    {0x144, 'n', 1}, {0x143, 'N', 1}, {0xF3, 'o', 1},  {0xD3, 'O', 1},
    {0x15B, 's', 1}, {0x15A, 'S', 1}, {0x17A, 'z', 1}, {0x179, 'Z', 1},
    {0x17C, 'z', 2}, {0x17B, 'Z', 2},
    // francuskie litery z nazw wlasnych - bez ozdobnikow
    {0xC8, 'E', 0}, {0xE8, 'e', 0}, {0xE9, 'e', 0}, {0xC2, 'A', 0}, {0xE2, 'a', 0},
    // znaki interpunkcyjne
    {0x2013, '-', 0}, {0x2014, '-', 0}, {0x2019, '\'', 0}, {0x2026, '.', 0},
    {0x201E, '"', 0}, {0x201D, '"', 0}, {0xAB, '"', 0}, {0xBB, '"', 0},
};

struct Glyph { char c; int accent; };

Glyph toGlyph(uint32_t cp) {
    if (cp < 128) return { (char)cp, 0 };
    for (auto& f : FOLD)
        if (f.cp == cp) return { f.base, f.accent };
    return { '?', 0 };
}

// rozbija tekst UTF-8 na kody znakow
std::vector<uint32_t> decodeUtf8(const std::string& s) {
    std::vector<uint32_t> out;
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = s[i++];
        uint32_t cp = c;
        int extra = 0;
        if (c >= 0xF0) { cp = c & 0x07; extra = 3; }
        else if (c >= 0xE0) { cp = c & 0x0F; extra = 2; }
        else if (c >= 0xC0) { cp = c & 0x1F; extra = 1; }
        for (int k = 0; k < extra && i < s.size(); k++, i++)
            cp = (cp << 6) | (s[i] & 0x3F);
        out.push_back(cp);
    }
    return out;
}

int textLen(const std::string& s) { return (int)decodeUtf8(s).size(); }

void drawGlyph(int x, int y, Glyph g, uint32_t color, int scale) {
    char c = g.c;
    if (c < 32 || c > 126) c = '?';
    const uint8_t* cols = FONT[c - 32];
    for (int col = 0; col < 5; col++)
        for (int row = 0; row < 7; row++)
            if ((cols[col] >> row) & 1)
                rect(x + col * scale, y + row * scale, scale, scale, color);

    bool upper = (c >= 'A' && c <= 'Z');
    auto dot = [&](int col, int row) { rect(x + col * scale, y + row * scale, scale, scale, color); };
    if (g.accent == 1) {
        if (upper) { dot(3, -2); dot(2, -1); } else { dot(3, 0); dot(2, 1); }
    } else if (g.accent == 2) {
        dot(2, upper ? -2 : 0);
    } else if (g.accent == 3) {
        dot(4, 7); dot(4, 8);
    } else if (g.accent == 4) {
        if (upper) { dot(0, 4); dot(1, 3); dot(2, 2); } else { dot(1, 4); dot(2, 3); dot(3, 2); }
    }
}

// maxChars: ile pierwszych znakow narysowac (do efektu pisania), -1 = wszystkie
void drawText(int x, int y, const std::string& str, uint32_t color, int scale = 1, bool shadow = true, int maxChars = -1) {
    int cx = x, n = 0;
    for (uint32_t cp : decodeUtf8(str)) {
        if (maxChars >= 0 && n >= maxChars) break;
        n++;
        if (cp == '\n') { y += 9 * scale; cx = x; continue; }
        Glyph g = toGlyph(cp);
        if (shadow) drawGlyph(cx + scale, y + scale, g, 0x000000, scale);
        drawGlyph(cx, y, g, color, scale);
        cx += 6 * scale;
    }
}

void drawTextCentered(int midX, int y, const std::string& str, uint32_t color, int scale = 1) {
    drawText(midX - textLen(str) * 6 * scale / 2, y, str, color, scale);
}

// dzieli tekst na akapity (po \n)
std::vector<std::string> split(const std::string& text) {
    std::vector<std::string> out;
    size_t start = 0;
    while (true) {
        size_t nl = text.find('\n', start);
        if (nl == std::string::npos) { out.push_back(text.substr(start)); break; }
        out.push_back(text.substr(start, nl - start));
        start = nl + 1;
    }
    return out;
}

// zawija tekst, zeby linia miala maks. maxChars znakow
std::vector<std::string> wrap(const std::string& text, int maxChars) {
    std::vector<std::string> lines;
    for (auto& para : split(text)) {
        std::string line;
        int len = 0;
        size_t i = 0;
        while (i <= para.size()) {
            size_t sp = para.find(' ', i);
            if (sp == std::string::npos) sp = para.size();
            std::string word = para.substr(i, sp - i);
            int wl = textLen(word);
            if (len > 0 && len + 1 + wl > maxChars) {
                lines.push_back(line);
                line.clear();
                len = 0;
            }
            if (len > 0) { line += ' '; len++; }
            line += word;
            len += wl;
            i = sp + 1;
        }
        lines.push_back(line);
    }
    return lines;
}

// ---------------------------------------------------------------- rysowanie: kafelki

void drawWall(int px, int py) {
    for (int dy = 0; dy < TILE; dy++) {
        int rowInBrick = dy % 8;
        int offset = ((dy / 8) % 2 == 0) ? 0 : 8;    // co drugi rzad cegiel przesuniety
        for (int dx = 0; dx < TILE; dx++) {
            uint32_t c;
            if (rowInBrick == 0 || (dx + offset) % 16 == 0) c = rgb(23, 20, 31);   // zaprawa
            else if (rowInBrick == 1) c = rgb(72, 64, 88);                          // jasna gorna krawedz
            else if (rowInBrick == 7) c = rgb(28, 24, 37);                          // ciemna dolna krawedz
            else {
                int noise = ((dx * 13 + dy * 17) % 7) - 3;
                c = rgb(47 + noise, 42 + noise, 59 + noise);
            }
            put(px + dx, py + dy, c);
        }
    }
}

void drawFloor(int px, int py, int tx, int ty, int style) {
    for (int dy = 0; dy < TILE; dy++) {
        for (int dx = 0; dx < TILE; dx++) {
            uint32_t c;
            if (dx == 0 || dy == 0)
                c = style == 0 ? rgb(56, 51, 66) : rgb(43, 40, 56);
            else if (dx == TILE - 1 || dy == TILE - 1)
                c = style == 0 ? rgb(23, 21, 29) : rgb(18, 16, 26);
            else {
                int speck = (dx * 7 + dy * 11 + tx * 3 + ty * 5) % 5;
                c = style == 0 ? rgb(43 + speck * 2, 40 + speck * 2, 51 + speck * 2)
                               : rgb(32 + speck * 2, 30 + speck * 2, 41 + speck * 2);
            }
            put(px + dx, py + dy, c);
        }
    }
    uint32_t h = hash2(tx, ty);
    if (style == 0 && h % 6 == 0) {            // pekniecie w kosciele
        line(px + 6, py + 8, px + 14, py + 18, rgb(20, 17, 26));
        line(px + 14, py + 18, px + 17, py + 26, rgb(20, 17, 26));
    }
    if (style == 1 && h % 5 == 1) {            // mech w podziemiach
        spr(px, py, 2, 11, 3, 1, rgb(34, 64, 58));
        spr(px, py, 3, 12, 2, 1, rgb(34, 64, 58));
    }
}

void drawWindow(int px, int py, int tx) {
    drawWall(px, py);
    spr(px, py, 2, 1, 10, 14, rgb(13, 10, 20));
    uint32_t cols[5] = { rgb(91, 42, 134), rgb(163, 32, 58), rgb(42, 79, 156), rgb(201, 162, 75), rgb(47, 122, 90) };
    for (int yy = 0; yy < 4; yy++)
        for (int xx = 0; xx < 3; xx++)
            spr(px, py, 3 + xx * 3, 2 + yy * 3, 2, 2, cols[(xx * 2 + yy * 3 + tx) % 5]);
}

void drawPew(int px, int py) {
    uint32_t dark = rgb(43, 27, 20), mid = rgb(74, 48, 34), light = rgb(93, 61, 43);
    spr(px, py, 1, 3, 14, 3, mid);
    spr(px, py, 1, 3, 14, 1, light);
    spr(px, py, 1, 6, 14, 1, dark);
    spr(px, py, 1, 8, 14, 5, rgb(61, 38, 25));
    spr(px, py, 1, 8, 14, 1, light);
    spr(px, py, 1, 13, 14, 1, dark);
    spr(px, py, 2, 13, 2, 2, dark);
    spr(px, py, 12, 13, 2, 2, dark);
}

void drawAltar(int px, int py) {
    uint32_t gold = rgb(201, 162, 75);
    spr(px, py, 0, 4, 16, 12, rgb(75, 70, 88));
    spr(px, py, 0, 4, 16, 2, rgb(95, 90, 112));
    spr(px, py, 0, 14, 16, 2, rgb(42, 38, 54));
    spr(px, py, 1, 6, 14, 7, rgb(90, 22, 48));      // oltarzowy material
    spr(px, py, 1, 6, 14, 1, gold);
    spr(px, py, 1, 12, 14, 1, gold);
    spr(px, py, 7, 7, 2, 5, gold);                  // krzyz
    spr(px, py, 5, 8, 6, 1, gold);
}

void drawCandle(int px, int py) {
    spr(px, py, 5, 12, 6, 3, rgb(59, 53, 71));
    spr(px, py, 7, 6, 2, 7, rgb(232, 220, 192));
    spr(px, py, 8, 6, 1, 7, rgb(184, 169, 138));
}

void drawStand(int px, int py) {
    uint32_t iron = rgb(42, 38, 48);
    spr(px, py, 7, 5, 2, 9, iron);
    spr(px, py, 4, 14, 8, 2, iron);
    spr(px, py, 5, 12, 6, 2, rgb(59, 53, 71));
    spr(px, py, 5, 4, 6, 2, iron);
    spr(px, py, 7, 1, 2, 4, rgb(232, 220, 192));
    spr(px, py, 8, 1, 1, 4, rgb(184, 169, 138));
}

void drawRubble(int px, int py, int tx, int ty) {
    uint32_t s = hash2(tx, ty) + 5u;
    uint32_t cols[3] = { rgb(58, 53, 68), rgb(44, 40, 54), rgb(72, 66, 90) };
    for (int i = 0; i < 9; i++) {
        int w = 3 + lcg(s) % 4;
        int h = 2 + lcg(s) % 3;
        int x = lcg(s) % (16 - w);
        int y = 4 + lcg(s) % (12 - h);
        uint32_t c = cols[lcg(s) % 3];
        spr(px, py, x, y, w, h, c);
        spr(px, py, x, y, w, 1, mul(c, 1.4f, 1.4f, 1.4f));
    }
}

// gwiazda pieciokatna w kole, pulsuje
void drawSigil(int px, int py, float time) {
    float pulse = 0.65f + 0.35f * std::sin(time * 2.0f);
    uint32_t c = rgb((int)(106 * pulse + 30), (int)(69 * pulse + 20), (int)(160 * pulse + 40));
    int cx = px + TILE / 2, cy = py + TILE / 2;
    const int R = 13;
    const float PI = 3.14159f;
    for (int a = 0; a < 360; a += 3) {
        float ang = a * PI / 180.0f;
        put(cx + (int)std::lround(std::cos(ang) * R), cy + (int)std::lround(std::sin(ang) * R), c);
    }
    int vx[5], vy[5];
    for (int i = 0; i < 5; i++) {
        float ang = -PI / 2 + i * PI * 4 / 5;       // co drugi wierzcholek = gwiazda
        vx[i] = cx + (int)std::lround(std::cos(ang) * R);
        vy[i] = cy + (int)std::lround(std::sin(ang) * R);
    }
    for (int i = 0; i < 5; i++) {
        int j = (i + 1) % 5;
        line(vx[i], vy[i], vx[j], vy[j], c);
    }
}

void drawTrap(int px, int py) {
    spr(px, py, 1, 1, 14, 14, rgb(20, 16, 28));
    spr(px, py, 2, 2, 12, 12, rgb(74, 48, 34));
    spr(px, py, 2, 6, 12, 1, rgb(43, 27, 20));
    spr(px, py, 2, 10, 12, 1, rgb(43, 27, 20));
    spr(px, py, 2, 2, 12, 1, rgb(93, 61, 43));
    spr(px, py, 7, 7, 2, 2, rgb(138, 138, 153));    // uchwyt
}

// iskierka na nieprzeczytanych polach
bool sparkle(float time, float phase) { return std::sin(time * 4.0f + phase) > 0.3f; }

void drawShelf(int px, int py, int tx, int ty, bool unread, float time) {
    spr(px, py, 0, 0, 16, 16, rgb(42, 27, 20));
    uint32_t s = (hash2(tx, ty) % 6u) * 7919u + 101u;
    uint32_t cols[6] = { rgb(91, 42, 42), rgb(42, 59, 91), rgb(59, 91, 58), rgb(107, 90, 42), rgb(74, 42, 91), rgb(91, 74, 58) };
    int rows[3] = { 1, 6, 11 };
    for (int row : rows) {
        int x = 1;
        while (x < 15) {
            int w = 1 + lcg(s) % 2;
            int h = 2 + lcg(s) % 3;
            uint32_t c = cols[lcg(s) % 6];
            spr(px, py, x, row + 4 - h, std::min(w, 15 - x), h, c);
            x += w;
        }
        spr(px, py, 0, row + 4, 16, 1, rgb(74, 48, 34));
    }
    if (unread && sparkle(time, (float)tx))
        spr(px, py, 2 + hash2(tx, ty) % 12u, 5, 1, 1, rgb(255, 210, 122));
}

void drawDesk(int px, int py, bool unread, float time) {
    spr(px, py, 1, 5, 14, 7, rgb(74, 48, 34));
    spr(px, py, 1, 5, 14, 1, rgb(93, 61, 43));
    spr(px, py, 2, 12, 2, 3, rgb(43, 27, 20));
    spr(px, py, 12, 12, 2, 3, rgb(43, 27, 20));
    spr(px, py, 4, 6, 4, 4, rgb(217, 207, 192));     // otwarta ksiega
    spr(px, py, 8, 6, 4, 4, rgb(217, 207, 192));
    spr(px, py, 8, 6, 1, 4, rgb(138, 127, 106));
    spr(px, py, 12, 3, 1, 4, rgb(232, 220, 192));    // piorko
    if (unread && sparkle(time, 0)) spr(px, py, 9, 6, 1, 1, rgb(255, 210, 122));
}

void drawStairs(int px, int py) {
    spr(px, py, 0, 0, 16, 16, rgb(13, 10, 20));
    for (int i = 0; i < 5; i++) {
        spr(px, py, 0, 2 + i * 3, 16, 3, rgb(60 + i * 14, 55 + i * 12, 75 + i * 14));
        spr(px, py, 0, 4 + i * 3, 16, 1, rgb(23, 20, 31));
    }
}

void drawSealedDoor(int px, int py, float time) {
    spr(px, py, 0, 0, 16, 16, rgb(47, 42, 59));
    spr(px, py, 2, 0, 12, 16, rgb(13, 10, 20));
    spr(px, py, 3, 1, 10, 15, rgb(59, 42, 32));
    spr(px, py, 3, 6, 10, 1, rgb(43, 27, 20));
    spr(px, py, 3, 12, 10, 1, rgb(43, 27, 20));
    spr(px, py, 3, 3, 10, 2, rgb(27, 27, 36));       // okucia
    spr(px, py, 3, 9, 10, 2, rgb(27, 27, 36));
    float pulse = 0.55f + 0.35f * std::sin(time * 3.0f);
    circle(px + 16, py + 16, 7, rgb((int)(154 * pulse), (int)(92 * pulse), (int)(255 * pulse)));
    circle(px + 16, py + 16, 3, rgb(230, 210, 255));
}

void drawOpenDoor(int px, int py) {
    spr(px, py, 0, 0, 2, 16, rgb(47, 42, 59));
    spr(px, py, 14, 0, 2, 16, rgb(47, 42, 59));
    spr(px, py, 0, 0, 16, 2, rgb(47, 42, 59));
}

void drawPedestal(int px, int py) {
    uint32_t gold = rgb(201, 162, 75);
    spr(px, py, 3, 8, 10, 7, rgb(59, 53, 71));
    spr(px, py, 2, 14, 12, 2, rgb(42, 38, 54));
    spr(px, py, 4, 4, 8, 5, rgb(90, 22, 48));        // poduszka ze zwojami
    spr(px, py, 4, 4, 8, 1, gold);
    spr(px, py, 4, 8, 8, 1, gold);
    spr(px, py, 7, 5, 2, 3, gold);
}

float gameTime = 0.0f;

void drawTile(int t, int px, int py, int tx, int ty) {
    const Level& L = levels[cur];
    if (t == WALL) { drawWall(px, py); return; }
    if (t == WINDOW) { drawWindow(px, py, tx); return; }

    drawFloor(px, py, tx, ty, L.style);   // reszta kafelkow stoi na podlodze
    bool unread = L.acts.count(key(tx, ty)) && !L.seen.count(key(tx, ty));
    switch (t) {
        case PEW: drawPew(px, py); break;
        case ALTAR: drawAltar(px, py); break;
        case CANDLE: drawCandle(px, py); break;
        case STAND: drawStand(px, py); break;
        case RUBBLE: drawRubble(px, py, tx, ty); break;
        case SIGIL: drawSigil(px, py, gameTime); break;
        case TRAP: drawTrap(px, py); break;
        case SHELF: drawShelf(px, py, tx, ty, unread, gameTime); break;
        case DESK: drawDesk(px, py, unread, gameTime); break;
        case STAIRS: drawStairs(px, py); break;
        case SEALED: drawSealedDoor(px, py, gameTime); break;
        case OPEN: drawOpenDoor(px, py); break;
        case PEDESTAL: drawPedestal(px, py); break;
    }
}

void drawFlame(int sx, int sy, float seed) {
    float f = std::sin(gameTime * 13.0f + seed) + std::sin(gameTime * 7.3f + seed * 2.0f);
    int h = f > 0.8f ? 8 : 6;
    rect(sx - 2, sy - h, 4, h, rgb(255, 122, 42));
    rect(sx - 2, sy - h + 2, 2, h - 2, rgb(255, 210, 122));
    rect(sx - 1, sy - h + 4, 1, 2, rgb(255, 255, 220));
}

void drawPlayer(int sx, int sy) {
    int step = player.moving ? ((int)(player.anim * 8.0f) % 2) : 0;
    int y = sy + (step ? -PX : 0);                   // podskok przy chodzeniu
    uint32_t skin = rgb(224, 199, 166), eye = rgb(21, 13, 28);

    rect(sx + 4 * PX, sy + 14 * PX, 8 * PX, 2 * PX, rgb(8, 6, 12));    // cien
    spr(sx, y, 5, 7, 6, 7, rgb(58, 47, 79));                           // plaszcz
    spr(sx, y, 5, 7, 1, 7, rgb(77, 63, 102));
    spr(sx, y, 4, 2, 8, 6, rgb(42, 33, 58));                           // kaptur

    if (player.facing == 0) {            // przodem
        spr(sx, y, 6, 5, 4, 3, skin);
        spr(sx, y, 6, 6, 1, 1, eye);
        spr(sx, y, 9, 6, 1, 1, eye);
    } else if (player.facing == 2) {     // w lewo
        spr(sx, y, 4, 5, 3, 3, skin);
        spr(sx, y, 4, 6, 1, 1, eye);
    } else if (player.facing == 3) {     // w prawo
        spr(sx, y, 9, 5, 3, 3, skin);
        spr(sx, y, 11, 6, 1, 1, eye);
    }

    spr(sx, sy, 5 + step, 14, 2, 2, rgb(18, 12, 24));                  // buty
    spr(sx, sy, 9 - step, 14, 2, 2, rgb(18, 12, 24));

    int lx = player.facing == 2 ? 2 : (player.facing == 3 ? 12 : 11);  // lampa
    spr(sx, y, lx, 8, 2, 1, rgb(168, 106, 28));
    spr(sx, y, lx, 9, 2, 2, rgb(255, 207, 106));
}

void drawGuardianBody(int sx, int sy) {
    int sway = (int)std::lround(std::sin(gameTime * 3.0f) * PX);
    uint32_t c = rgb(7, 3, 12);
    rect(sx + 4 * PX + sway, sy + 3 * PX, 8 * PX, 11 * PX, c);
    rect(sx + 3 * PX + sway, sy + 6 * PX, 10 * PX, 8 * PX, c);
    for (int i = 0; i < 4; i++) {                    // poszarpany dol
        int extra = std::sin(gameTime * 5.0f + i * 1.7f) > 0.0f ? 2 : 0;
        rect(sx + (3 + i * 3) * PX + sway, sy + 14 * PX, 2 * PX, (2 + extra) * PX, c);
    }
}

void drawGuardianEyes(int sx, int sy, float alpha) {
    int sway = (int)std::lround(std::sin(gameTime * 3.0f) * PX);
    uint32_t c = mul(rgb(255, 46, 85), alpha, alpha, alpha);
    rect(sx + 5 * PX + sway, sy + 7 * PX, 2 * PX, PX, c);
    rect(sx + 9 * PX + sway, sy + 7 * PX, 2 * PX, PX, c);
}

// ---------------------------------------------------------------- oswietlenie

float lightBuf[SW * SH * 3];   // swiatlo RGB dla kazdego piksela
float vig[SW * SH];            // winieta (ciemne rogi), liczona raz

void buildVignette() {
    for (int y = 0; y < SH; y++) {
        for (int x = 0; x < SW; x++) {
            float nx = (x - SW / 2.0f) / (SW / 2.0f);
            float ny = (y - SH / 2.0f) / (SH / 2.0f);
            float v = std::clamp((std::sqrt(nx * nx + ny * ny) - 0.55f) / 0.85f, 0.0f, 1.0f);
            vig[y * SW + x] = v * v;
        }
    }
}

// dodaje okragle swiatlo do bufora
void addLightToBuf(float lx, float ly, float rad, float cr, float cg, float cb, float str, int camX, int camY) {
    int sx = (int)lx - camX;
    int sy = (int)ly - camY + HUD;
    int ir = (int)rad;
    int x0 = std::max(0, sx - ir), x1 = std::min(SW - 1, sx + ir);
    int y0 = std::max(HUD, sy - ir), y1 = std::min(SH - 1, sy + ir);
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            float dx = (float)(x - sx), dy = (float)(y - sy);
            float d = std::sqrt(dx * dx + dy * dy);
            if (d >= rad) continue;
            float a = 1.0f - d / rad;
            a = a * a * (3.0f - 2.0f * a) * str;      // smoothstep - miekkie krawedzie
            float* p = &lightBuf[(y * SW + x) * 3];
            p[0] += a * cr;
            p[1] += a * cg;
            p[2] += a * cb;
        }
    }
}

void applyLighting(int camX, int camY) {
    // swiatlo otoczenia - ciemne, lekko fioletowe
    for (int i = HUD * SW; i < SW * SH; i++) {
        lightBuf[i * 3] = 0.17f;
        lightBuf[i * 3 + 1] = 0.15f;
        lightBuf[i * 3 + 2] = 0.24f;
    }
    for (auto& l : levels[cur].lights) {
        if (!lightOn(l)) continue;
        float k = 1.0f;
        if (l.flame) k += std::sin(gameTime * 7.0f + l.seed) * 0.03f + std::sin(gameTime * 13.0f + l.seed * 2.0f) * 0.02f;
        addLightToBuf(l.x, l.y, l.radius * k, l.r, l.g, l.b, l.strength, camX, camY);
    }
    // lampa gracza - im mniej rozumu, tym mniejszy zasieg
    float sanityK = 0.6f + 0.4f * player.sanity / 100.0f;
    float pr = 4.6f * TILE * sanityK * (1.0f + std::sin(gameTime * 9.0f) * 0.02f);
    addLightToBuf(player.x * TILE, player.y * TILE - 6.0f, pr, 1.0f, 0.90f, 0.72f, 1.0f, camX, camY);

    for (int i = HUD * SW; i < SW * SH; i++) {
        const float* p = &lightBuf[i * 3];
        pixels[i] = mul(pixels[i], std::min(1.5f, p[0]), std::min(1.5f, p[1]), std::min(1.5f, p[2]));
    }
}

// ciemne rogi; przy niskim rozumie robia sie czerwone
void applyVignette(float insane) {
    for (int y = HUD; y < SH; y++) {
        for (int x = 0; x < SW; x++) {
            int i = y * SW + x;
            float v = vig[i];
            uint32_t c = pixels[i];
            float k = 1.0f - 0.55f * v;
            pixels[i] = rgb((int)(getR(c) * k + 140.0f * v * insane),
                            (int)(getG(c) * k),
                            (int)(getB(c) * k + 20.0f * v * insane));
        }
    }
}

void applyFade(float a) {
    float k = 1.0f - std::clamp(a, 0.0f, 1.0f);
    for (int i = 0; i < SW * SH; i++) pixels[i] = mul(pixels[i], k, k, k);
}

// ---------------------------------------------------------------- dialogi, przejscia, komunikaty

enum After { DO_NONE, DO_CIPHER, DO_CHOICE, DO_DIG, DO_DESCEND, DO_FAINT };
enum FadeAct { FADE_NONE, FADE_LEVEL, FADE_FAINT, FADE_END };

struct Dialog {
    std::string title;
    std::vector<std::string> pages;
    int page = 0;
    float chars = 0.0f;         // ile znakow juz "napisano"
    After after = DO_NONE;      // co zrobic po zamknieciu
};

struct Fade {
    float a = 0.0f;             // 0 = widac, 1 = czarny ekran
    int dir = 0;                // 1 sciemnianie, -1 rozjasnianie, 0 nic
    FadeAct action = FADE_NONE;
    int level = 0;
    float sx = 0, sy = 0;
};

struct Toast { std::string text; float timer = 0.0f; };
struct Whisper { std::string text; int x = 0, y = 0; float life = 0.0f; };

Dialog dlg;
Fade fade;
Toast toast;
Whisper whisper;
float nextWhisper = 8.0f;

int cipherShift = 0;
const char* CIPHER_TEXT = "WHRGRUD";   // "TEODORA" przesuniete o 3
const int CIPHER_ANSWER = 3;
int choiceSel = 0;

float camX = 0, camY = 0;
float shakeTimer = 0.0f;
int shakeX = 0, shakeY = 0;

void showToast(const std::string& text) {
    toast.text = text;
    toast.timer = 2.6f;
}

void addJournal(int id) {
    if (std::find(journal.begin(), journal.end(), id) != journal.end()) return;
    journal.push_back(id);
    showToast(std::string("NOWY WPIS: ") + lore[id].title);
}

void say(const std::string& title, const std::vector<std::string>& pages, After after = DO_NONE) {
    dlg.title = title;
    dlg.pages = pages;
    dlg.page = 0;
    dlg.chars = 0.0f;
    dlg.after = after;
    state = DIALOG;
}

void startFade(FadeAct action, int level = 0, float sx = 0, float sy = 0) {
    fade.action = action;
    fade.level = level;
    fade.sx = sx;
    fade.sy = sy;
    fade.dir = 1;
}

// ---------------------------------------------------------------- kamera

void camTarget(float& tx, float& ty) {
    const Map& m = levels[cur].map;
    float mapW = (float)(m.w * TILE), mapH = (float)(m.h * TILE);
    tx = player.x * TILE - SW / 2.0f;
    ty = player.y * TILE - VIEW_H / 2.0f;
    // jesli mapa miesci sie w oknie, centrujemy ja
    tx = mapW <= SW ? -(SW - mapW) / 2 : std::clamp(tx, 0.0f, mapW - SW);
    ty = mapH <= VIEW_H ? -(VIEW_H - mapH) / 2 : std::clamp(ty, 0.0f, mapH - VIEW_H);
}

void snapCamera() { camTarget(camX, camY); }

void followCamera() {
    float tx, ty;
    camTarget(tx, ty);
    camX += (tx - camX) * 0.12f;
    camY += (ty - camY) * 0.12f;
}

// ---------------------------------------------------------------- logika

void loadLevel(int idx, float sx, float sy) {
    cur = idx;
    player.x = sx;
    player.y = sy;
    player.facing = 1;
    if (levels[idx].hasGuardian) {
        guardian.x = guardian.tx = levels[idx].guardX;
        guardian.y = guardian.ty = levels[idx].guardY;
        guardian.wait = 2.0f;
    }
    snapCamera();
}

void resetGame() {
    levels[0] = buildChurch();
    levels[1] = buildArchive();
    flags = Flags();
    journal.clear();
    fade = Fade();
    toast = Toast();
    whisper = Whisper();
    nextWhisper = 8.0f;
    journalSel = 0;
    player.sanity = 100.0f;
    loadLevel(0, levels[0].startX, levels[0].startY);
}

std::string objective() {
    if (cur == 0) {
        if (!flags.altar) return "Zbadaj ołtarz.";
        if (!flags.trap) return "Znajdź pieczęć na posadzce nawy.";
        return "Zejdź do podziemi probostwa.";
    }
    if (!flags.door)
        return flags.desk ? "Odszyfruj inskrypcję na zapieczętowanych drzwiach."
                          : "Przeszukaj podziemia Saunière’a. Szukaj notatek i drzwi z pieczęcią.";
    return "Podejdź do piedestału w skryptorium.";
}

// otwiera dialog z wpisem z dziennika
void readLore(int id, const char* title, const char* flavor) {
    addJournal(id);
    std::vector<std::string> pages;
    if (flavor) pages.push_back(flavor);
    for (auto& p : split(lore[id].text)) pages.push_back(p);
    say(title, pages);
}

void submitCipher() {
    if (cipherShift == CIPHER_ANSWER) {
        flags.door = true;
        levels[1].map.set(19, 5, OPEN);
        levels[1].acts.erase(key(19, 5));
        addJournal(L_DOOR);
        say("Pieczęć", {
            "Pieczęć gaśnie. Odczytujesz imię: TEODORA.",
            "To imię kluczowe dla sekretu archiwów. Drzwi uchylają się, odkrywając ukrytą komnatę ze zwojami." });
    } else {
        player.sanity = std::max(1.0f, player.sanity - 12.0f);
        shakeTimer = 0.5f;
        showToast("Pieczęć syczy: to nie jest właściwe słowo.");
    }
}

void interact(int tx, int ty, const Act& a) {
    Level& L = levels[cur];
    switch (a.kind) {
    case A_READ:
        L.seen.insert(key(tx, ty));
        readLore(a.lore, a.title, a.flavor);
        break;

    case A_ALTAR: {
        bool first = !flags.altar;
        flags.altar = true;
        addJournal(L_ALTAR);
        std::vector<std::string> pages;
        if (first) pages.push_back("Na kamiennej mensie obok Biblii leży karta pergaminu.");
        for (auto& p : split(lore[L_ALTAR].text)) pages.push_back(p);
        if (first) pages.push_back("Podnosisz wzrok. Za ołtarzem płoną trzy świece, a wzrok Asmodeusza przy wejściu wydaje się śledzić każdy twój ruch.");
        say("Ołtarz", pages);
        break;
    }

    case A_SIGIL:
        if (flags.trap)
            say("Pieczęć", { "Przejście w głąb podziemi jest otwarte. Wieje stamtąd chłodem wieków." }, DO_DESCEND);
        else if (!flags.altar)
            say("Znak na posadzce", { "Płyta z wyrytym znakiem. Wygląda na zwykły element posadzki, dopóki nie znasz jego znaczenia." });
        else
            say("Pieczęć", { "Klękasz przy oznaczonym miejscu. Kamień pod dłonią drży.",
                             "Odsuwasz płytę: pod spodem ukryte są schody do dawnych piwnic probostwa." }, DO_DIG);
        break;

    case A_DESK: {
        bool first = !flags.desk;
        flags.desk = true;
        L.seen.insert(key(tx, ty));
        addJournal(L_DESK);
        std::vector<std::string> pages;
        if (first) pages.push_back("Pióro wciąż leży na biurku księdza Saunière’a. Atrament nie wysechł do końca.");
        for (auto& p : split(lore[L_DESK].text)) pages.push_back(p);
        say("Biurko ks. Saunière'a", pages);
        break;
    }

    case A_STAIRS:
        startFade(FADE_LEVEL, 0, 11.5f, 9.5f);
        break;

    case A_DOOR:
        say("Zapieczętowane drzwi", { "Na drzwiach jarzy się fioletowa pieczęć. Wokół niej wyryto szyfr." }, DO_CIPHER);
        break;

    case A_PEDESTAL:
        say("Piedestał", {
            "Na aksamicie spoczywają oryginalne Pergaminy z Rennes.",
            "Teksty zawierają listy rodu Merowingów oraz wskazówki prowadzące do legendarnego skarbu.",
            "Z mroku za twoimi plecami słychać szept: „Zostaw je lub zabierz, ale pamiętaj o cenie.”" }, DO_CHOICE);
        break;
    }
}

void runAfter(After a) {
    switch (a) {
    case DO_CIPHER:
        cipherShift = 0;
        state = CIPHER;
        break;
    case DO_CHOICE:
        choiceSel = 0;
        state = CHOICE;
        break;
    case DO_DIG:
        levels[0].map.set(11, 8, TRAP);
        flags.trap = true;
        addJournal(L_SIGIL);
        break;
    case DO_DESCEND:
        startFade(FADE_LEVEL, 1, levels[1].startX, levels[1].startY);
        break;
    case DO_FAINT:
        startFade(FADE_FAINT);
        break;
    default: break;
    }
}

void advanceDialog() {
    int pageLen = textLen(dlg.pages[dlg.page]);
    if (dlg.chars < pageLen) { dlg.chars = (float)pageLen; return; }   // dokoncz pisanie
    if (dlg.page < (int)dlg.pages.size() - 1) { dlg.page++; dlg.chars = 0.0f; return; }
    After a = dlg.after;
    dlg.after = DO_NONE;
    state = PLAY;
    runAfter(a);
}

void finishGame(int kind) {
    flags.ending = kind;
    startFade(FADE_END);
}

void updateFade() {
    if (fade.dir == 0) return;
    fade.a += fade.dir * 0.05f;

    if (fade.dir > 0 && fade.a >= 1.0f) {        // ekran caly czarny - zmiana
        fade.a = 1.0f;
        fade.dir = -1;
        FadeAct act = fade.action;
        fade.action = FADE_NONE;
        if (act == FADE_LEVEL) {
            loadLevel(fade.level, fade.sx, fade.sy);
            if (cur == 1 && !flags.arrived) {
                flags.arrived = true;
                say("Podziemia", { "Wchodzisz do ukrytych piwnic i skryptorium pod kościołem. Powietrze jest gęste od zapachu starego pergaminu.",
                                   "Gdzieś w mroku ktoś czuwa. Trzymaj się światła lamp." });
            }
        } else if (act == FADE_FAINT) {
            player.sanity = 60.0f;
            loadLevel(cur, levels[cur].startX, levels[cur].startY);
        } else if (act == FADE_END) {
            state = ENDING;
        }
    } else if (fade.dir < 0 && fade.a <= 0.0f) {
        fade.a = 0.0f;
        fade.dir = 0;
    }
}

// ---------------------------------------------------------------- straznik

// gdzie straznik moze chodzic (nie wchodzi do komnaty z piedestalem)
bool guardianOk(float x, float y) {
    const Map& m = levels[cur].map;
    int tx = (int)x, ty = (int)y;
    if (tx < 1 || ty < 1 || tx >= m.w - 1 || ty >= m.h - 1) return false;
    if (tx >= 19 && ty <= 9) return false;
    return true;
}

void pickWaypoint() {
    const Map& m = levels[cur].map;
    for (int i = 0; i < 30; i++) {
        float x = 1.0f + rand() % (m.w - 2) + 0.5f;
        float y = 1.0f + rand() % (m.h - 2) + 0.5f;
        if (guardianOk(x, y)) {
            guardian.tx = x;
            guardian.ty = y;
            return;
        }
    }
}

void updateGuardian() {
    Guardian& g = guardian;
    float dx = player.x - g.x, dy = player.y - g.y;
    float dist = std::hypot(dx, dy);
    bool safe = nearSafeLight(player.x, player.y, 0.65f);

    float mx, my, speed;
    if (!safe && dist < 6.9f) {                // gracz w ciemnosci - straznik goni
        mx = dx; my = dy; speed = 0.032f;
    } else if (safe && dist < 4.4f) {          // gracz przy swietle - straznik sie cofa
        mx = -dx; my = -dy; speed = 0.030f;
    } else {                                   // patroluje
        g.wait -= DT;
        if (g.wait <= 0.0f || std::hypot(g.tx - g.x, g.ty - g.y) < 0.25f) {
            pickWaypoint();
            g.wait = 3.0f + (rand() % 40) / 10.0f;
        }
        mx = g.tx - g.x;
        my = g.ty - g.y;
        speed = 0.014f;
    }

    float len = std::hypot(mx, my);
    if (len < 0.0001f) len = 1.0f;
    float nx = g.x + mx / len * speed;
    float ny = g.y + my / len * speed;
    if (guardianOk(nx, ny)) { g.x = nx; g.y = ny; }
    else pickWaypoint();
}

// ---------------------------------------------------------------- aktualizacja swiata

// pole, z ktorym mozna teraz wejsc w interakcje (pod nogami albo przed soba)
bool findTarget(int& tx, int& ty, Act& act) {
    const Level& L = levels[cur];
    tx = (int)player.x;
    ty = (int)player.y;
    auto it = L.acts.find(key(tx, ty));
    if (it == L.acts.end()) {
        const int DX[4] = { 0, 0, -1, 1 };
        const int DY[4] = { 1, -1, 0, 0 };
        tx = (int)std::floor(player.x + DX[player.facing] * 0.75f);
        ty = (int)std::floor(player.y + DY[player.facing] * 0.75f);
        it = L.acts.find(key(tx, ty));
        if (it == L.acts.end()) return false;
    }
    act = it->second;
    return true;
}

void showWhisper() {
    whisper.text = whispers[rand() % WHISPER_COUNT];
    whisper.x = 80 + rand() % 420;
    whisper.y = HUD + 60 + rand() % 360;
    whisper.life = 1.8f;
    nextWhisper = gameTime + 5.0f + (rand() % 60) / 10.0f;
}

void updateWorld() {
    // ruch
    float mx = 0, my = 0;
    if (keyDown('W') || keyDown(VK_UP))    { my -= 1; player.facing = 1; }
    if (keyDown('S') || keyDown(VK_DOWN))  { my += 1; player.facing = 0; }
    if (keyDown('A') || keyDown(VK_LEFT))  { mx -= 1; player.facing = 2; }
    if (keyDown('D') || keyDown(VK_RIGHT)) { mx += 1; player.facing = 3; }
    player.moving = (mx != 0 || my != 0);
    if (mx != 0 && my != 0) { mx *= 0.7071f; my *= 0.7071f; }   // skos nie jest szybszy

    const float speed = 0.07f, r = 0.28f;
    float x = player.x, y = player.y;
    float nx = x + mx * speed, ny = y + my * speed;
    // osobno X i Y, zeby mozna bylo slizgac sie po scianach
    if (!solidAt(nx + r, y + 0.2f) && !solidAt(nx - r, y + 0.2f) &&
        !solidAt(nx + r, y - 0.1f) && !solidAt(nx - r, y - 0.1f)) x = nx;
    if (!solidAt(x + r, ny + 0.2f) && !solidAt(x - r, ny + 0.2f) &&
        !solidAt(x + r, ny - 0.1f) && !solidAt(x - r, ny - 0.1f)) y = ny;
    player.x = x;
    player.y = y;
    if (player.moving) player.anim += DT;

    if (levels[cur].hasGuardian) updateGuardian();

    // rozum: przy swietle sie regeneruje, w ciemnosci i przy straznik traci
    bool lit = nearSafeLight(x, y, 0.65f);
    float dG = levels[cur].hasGuardian ? std::hypot(x - guardian.x, y - guardian.y) : 1000.0f;
    float sanity = player.sanity;
    if (lit && dG >= 2.5f) {
        sanity = std::min(100.0f, sanity + 5.0f * DT);
    } else {
        float drain = 0.0f;
        if (!lit) drain += 0.9f;
        if (dG < 2.5f) {
            drain += 14.0f * (1.0f - dG / 2.5f) + 2.0f;
            shakeTimer = std::max(shakeTimer, 0.1f);
        }
        sanity = std::max(0.0f, sanity - drain * DT);
    }
    player.sanity = sanity;

    if (sanity <= 0.0f) {
        player.sanity = 1.0f;
        say("Szepty", { "Mrok zamyka się wokół ciebie. Budzisz się przy wejściu, z głową pełną szeptów." }, DO_FAINT);
        return;
    }
    if (sanity < 55.0f && gameTime > nextWhisper) showWhisper();
}

void updateEffects() {
    if (shakeTimer > 0.0f) {
        shakeTimer -= DT;
        shakeX = rand() % 5 - 2;
        shakeY = rand() % 5 - 2;
    } else {
        shakeX = shakeY = 0;
    }
    if (toast.timer > 0.0f) toast.timer -= DT;
    if (whisper.life > 0.0f) whisper.life -= DT;
    if (state == DIALOG) dlg.chars += 1.4f;
}

// ---------------------------------------------------------------- ekrany

const uint32_t GOLD = rgb(215, 175, 40);

void drawPanel(int x, int y, int w, int h, uint32_t bg) {
    rect(x - 3, y - 3, w + 6, h + 6, rgb(59, 43, 18));
    rect(x - 1, y - 1, w + 2, h + 2, GOLD);
    rect(x, y, w, h, bg);
}

void drawHud() {
    rect(0, 0, SW, HUD, rgb(15, 18, 28));
    rect(0, HUD - 2, SW, 2, GOLD);

    drawText(15, 9, "ROZUM", rgb(180, 150, 240));
    rect(62, 8, 110, 12, rgb(24, 16, 40));
    float san = player.sanity;
    int barW = (int)(106.0f * std::clamp(san / 100.0f, 0.0f, 1.0f));
    uint32_t fill = san > 55 ? rgb(140, 95, 225) : (san > 25 ? rgb(200, 60, 120) : rgb(235, 30, 45));
    if (san <= 25 && std::sin(gameTime * 10.0f) > 0) fill = rgb(255, 90, 100);   // miga
    rect(64, 10, barW, 8, fill);
    rect(64, 10, barW, 2, mul(fill, 1.4f, 1.4f, 1.4f));

    drawText(200, 9, levels[cur].name, GOLD);
    drawText(620, 9, "WPISY: " + std::to_string(journal.size()) + "/" + std::to_string(LORE_COUNT) + "  [J]", rgb(180, 220, 255));
    drawText(15, 27, "CEL: " + objective(), rgb(140, 180, 240));
}

void drawMinimap() {
    const Level& L = levels[cur];
    int mw = L.map.w * 4 + 4, mh = L.map.h * 4 + 4;
    int mx = SW - mw - 12, my = HUD + 10;
    rect(mx - 2, my - 2, mw + 4, mh + 4, GOLD);
    rect(mx, my, mw, mh, rgb(10, 12, 18));

    for (int y = 0; y < L.map.h; y++) {
        for (int x = 0; x < L.map.w; x++) {
            int t = L.map.get(x, y);
            uint32_t c = rgb(20, 22, 32);
            if (t == WALL || t == WINDOW) c = rgb(65, 70, 90);
            else if (t == SEALED) c = rgb(154, 92, 255);
            else if (t == OPEN) c = rgb(255, 180, 0);
            else if (t == STAIRS || t == TRAP) c = rgb(0, 255, 255);
            else if (t == PEDESTAL) c = rgb(255, 215, 0);
            else if (!walkable(t)) c = rgb(40, 44, 60);
            rect(mx + 2 + x * 4, my + 2 + y * 4, 3, 3, c);
        }
    }
    // straznik pojawia sie na mapce dopiero gdy jest blisko
    if (L.hasGuardian && std::hypot(player.x - guardian.x, player.y - guardian.y) < 5.0f)
        rect(mx + 2 + (int)(guardian.x * 4), my + 2 + (int)(guardian.y * 4), 3, 3, rgb(255, 30, 40));
    rect(mx + 2 + (int)(player.x * 4), my + 2 + (int)(player.y * 4), 4, 4, rgb(0, 240, 255));
}

void drawScene() {
    const Level& L = levels[cur];
    clearScreen(rgb(0, 0, 0));
    int cx = (int)std::floor(camX) + shakeX;
    int cy = (int)std::floor(camY) + shakeY;

    // tylko kafelki widoczne na ekranie
    int tx0 = std::max(0, (int)std::floor((float)cx / TILE));
    int tx1 = std::min(L.map.w, (cx + SW) / TILE + 1);
    int ty0 = std::max(0, (int)std::floor((float)cy / TILE));
    int ty1 = std::min(L.map.h, (cy + VIEW_H) / TILE + 1);
    for (int ty = ty0; ty < ty1; ty++)
        for (int tx = tx0; tx < tx1; tx++)
            drawTile(L.map.get(tx, ty), tx * TILE - cx, ty * TILE - cy + HUD, tx, ty);

    // postacie (sprite ma 32x32, stoi na srodku pozycji)
    int gx = 0, gy = 0;
    if (L.hasGuardian) {
        gx = (int)(guardian.x * TILE) - 16 - cx;
        gy = (int)(guardian.y * TILE) - 24 - cy + HUD;
        drawGuardianBody(gx, gy);
    }
    drawPlayer((int)(player.x * TILE) - 16 - cx, (int)(player.y * TILE) - 24 - cy + HUD);

    applyLighting(cx, cy);

    // rzeczy, ktore swieca same - rysowane juz po oswietleniu
    for (auto& l : L.lights)
        if (l.flame && lightOn(l)) drawFlame((int)l.x - cx, (int)l.y - cy + HUD + 4, l.seed);
    if (L.hasGuardian) {
        float d = std::hypot(player.x - guardian.x, player.y - guardian.y);
        drawGuardianEyes(gx, gy, std::clamp(1.1f - d / 8.0f, 0.0f, 1.0f));
    }

    // znaczek [E] nad polem, z ktorym mozna cos zrobic
    int tx, ty;
    Act act;
    if (state == PLAY && fade.dir == 0 && findTarget(tx, ty, act)) {
        int sx = tx * TILE - cx + TILE / 2;
        int sy = ty * TILE - cy + HUD - 10 + (int)std::lround(std::sin(gameTime * 5.0f) * 2.0f);
        rect(sx - 9, sy - 8, 18, 14, GOLD);
        rect(sx - 8, sy - 7, 16, 12, rgb(15, 18, 28));
        drawText(sx - 3, sy - 5, "E", rgb(255, 215, 0));
    }

    float insane = std::clamp((60.0f - player.sanity) / 60.0f, 0.0f, 1.0f);
    applyVignette(insane * (0.65f + 0.2f * std::sin(gameTime * 4.0f)));

    drawHud();
    drawMinimap();

    if (toast.timer > 0.0f) {
        int w = textLen(toast.text) * 6 + 24;
        drawPanel(SW / 2 - w / 2, HUD + 14, w, 22, rgb(20, 24, 38));
        drawTextCentered(SW / 2, HUD + 21, toast.text, GOLD);
    }
    if (whisper.life > 0.0f) {
        float k = std::clamp(whisper.life, 0.0f, 1.0f);
        drawText(whisper.x, whisper.y, whisper.text, mul(rgb(215, 70, 105), k, k, k), 2);
    }
}

void drawDialog() {
    const int x = 40, y = SH - 196, w = 720, h = 176;
    drawPanel(x, y, w, h, rgb(14, 12, 24));
    drawText(x + 20, y + 14, dlg.title, GOLD, 2);
    rect(x + 20, y + 36, w - 40, 1, rgb(90, 70, 20));

    // efekt maszyny do pisania: rysujemy tyle znakow, ile wynosi dlg.chars
    auto lines = wrap(dlg.pages[dlg.page], 56);
    int left = (int)dlg.chars;
    for (size_t i = 0; i < lines.size() && left > 0; i++) {
        drawText(x + 20, y + 48 + (int)i * 20, lines[i], rgb(220, 230, 245), 2, true, left);
        left -= textLen(lines[i]);
    }

    bool last = dlg.page == (int)dlg.pages.size() - 1;
    if ((int)(gameTime * 2.5f) % 2 == 0)
        drawText(x + w - 76, y + h - 24, last ? "[E] OK" : "[E] >>", GOLD);
}

void drawJournal() {
    clearScreen(rgb(10, 8, 18));
    rect(0, 0, SW, 64, rgb(15, 18, 28));
    rect(0, 62, SW, 2, GOLD);
    drawText(30, 18, "DZIENNIK ARCHIWISTKI", GOLD, 3);
    drawText(620, 26, "WPISY: " + std::to_string(journal.size()) + "/" + std::to_string(LORE_COUNT), rgb(180, 220, 255));
    drawText(30, SH - 22, "[GORA/DOL] WYBIERZ   [J / ESC] ZAMKNIJ", rgb(140, 150, 175));

    if (journal.empty()) {
        drawTextCentered(SW / 2, 280, "Jeszcze nic nie zapisano.", rgb(140, 150, 175), 2);
        return;
    }

    // lista wpisow po lewej
    drawPanel(24, 84, 270, 480, rgb(16, 14, 26));
    for (size_t i = 0; i < journal.size(); i++) {
        int y = 98 + (int)i * 34;
        const Lore& e = lore[journal[i]];
        bool sel = (int)i == journalSel;
        if (sel) rect(30, y - 6, 258, 30, rgb(52, 40, 18));
        drawText(38, y - 2, e.category, rgb(163, 32, 58));
        drawText(38, y + 9, e.title, sel ? rgb(255, 225, 120) : rgb(200, 190, 175));
    }

    // tresc wybranego wpisu po prawej
    const Lore& e = lore[journal[journalSel]];
    drawPanel(318, 84, 458, 480, rgb(16, 14, 26));
    drawText(340, 102, e.category, rgb(200, 60, 90));
    drawText(340, 118, e.title, GOLD, 2);
    rect(340, 142, 414, 1, rgb(90, 70, 20));
    auto lines = wrap(e.text, 33);
    for (size_t i = 0; i < lines.size(); i++)
        drawText(340, 156 + (int)i * 20, lines[i], rgb(220, 225, 240), 2, false);
}

void drawCipher() {
    clearScreen(rgb(12, 8, 22));
    drawTextCentered(SW / 2, 40, "PIECZĘĆ SKRYPTORIUM", GOLD, 4);
    drawTextCentered(SW / 2, 90, "INSKRYPCJA NA DRZWIACH", rgb(140, 150, 175));

    // zaszyfrowany napis
    float pulse = 0.7f + 0.3f * std::sin(gameTime * 3.0f);
    uint32_t glow = rgb((int)(183 * pulse), (int)(156 * pulse), (int)(255 * pulse));
    std::string inscription;
    for (const char* c = CIPHER_TEXT; *c; c++) { inscription += *c; inscription += ' '; }
    drawTextCentered(SW / 2, 130, inscription, glow, 6);

    // alfabet i przesuniete kolo
    const int x0 = 88, step = 24;
    drawText(x0, 220, "ALFABET", rgb(140, 150, 175));
    drawText(x0, 282, "KOŁO (PRZESUNIĘTE)", rgb(140, 150, 175));
    for (int i = 0; i < 26; i++) {
        drawText(x0 + i * step, 240, std::string(1, 'A' + i), rgb(150, 140, 175), 2);
        drawText(x0 + i * step, 302, std::string(1, 'A' + (i - cipherShift + 26) % 26), GOLD, 2);
    }
    drawTextCentered(SW / 2, 350, "PRZESUNIĘCIE: " + std::to_string(cipherShift), rgb(220, 230, 245), 2);

    // jak wyglada napis po odszyfrowaniu przy obecnym przesunieciu
    std::string decoded;
    for (const char* c = CIPHER_TEXT; *c; c++) {
        decoded += (char)('A' + (*c - 'A' - cipherShift + 26) % 26);
        decoded += ' ';
    }
    drawTextCentered(SW / 2, 410, "ODCZYT", rgb(140, 150, 175));
    drawTextCentered(SW / 2, 430, decoded, GOLD, 5);

    if (toast.timer > 0.0f) drawTextCentered(SW / 2, 490, toast.text, rgb(235, 60, 80), 1);
    drawTextCentered(SW / 2, 520, "[LEWO/PRAWO] ZMIEN PRZESUNIĘCIE    [ENTER] ZATWIERDŹ    [ESC] WYJDŹ", rgb(180, 220, 255));
    drawTextCentered(SW / 2, 545, "WSKAZÓWEK SZUKAJ W DZIENNIKU [J]", rgb(140, 150, 175));
}

void drawChoice() {
    clearScreen(rgb(10, 8, 18));
    drawPanel(130, 110, 540, 380, rgb(20, 14, 30));
    drawTextCentered(SW / 2, 150, "PERGAMINY Z RENNES", GOLD, 4);
    auto lines = wrap("Zachować tajemnicę, która może wstrząsnąć światem, czy zniszczyć ją, by nie przyniosła zguby?", 40);
    for (size_t i = 0; i < lines.size(); i++)
        drawTextCentered(SW / 2, 215 + (int)i * 20, lines[i], rgb(220, 230, 245), 2);

    for (int i = 0; i < 2; i++) {
        int bx = 170 + i * 240;
        bool sel = (i == choiceSel);
        uint32_t textCol = sel ? rgb(255, 225, 120) : rgb(200, 190, 175);
        rect(bx, 330, 220, 70, sel ? GOLD : rgb(90, 70, 20));
        rect(bx + 2, 332, 216, 66, sel ? rgb(60, 46, 14) : rgb(20, 24, 38));
        drawTextCentered(bx + 110, 345, i == 0 ? "[1] ZACHOWAJ" : "[2] SPAL", textCol, 2);
        drawTextCentered(bx + 110, 372, "ZWOJE", textCol, 2);
    }
    drawTextCentered(SW / 2, 440, "[LEWO/PRAWO] WYBIERZ   [ENTER] POTWIERDŹ", rgb(140, 150, 175));
}

void drawTitle() {
    clearScreen(rgb(10, 8, 18));
    drawTextCentered(SW / 2, 110, "PERGAMINY", rgb(255, 215, 0), 7);
    drawTextCentered(SW / 2, 185, "Z RENNES-LE-CHÂTEAU", rgb(140, 100, 230), 3);
    drawTextCentered(SW / 2, 230, "kościół św. Marii Magdaleny, Rennes-le-Château", rgb(140, 180, 240));

    rect(180, 290, 440, 90, rgb(20, 16, 34));
    rect(180, 290, 440, 1, GOLD);
    rect(180, 379, 440, 1, GOLD);
    drawText(200, 303, "[WASD / STRZAŁKI] RUCH     [E / SPACJA] DZIAŁAJ", rgb(220, 230, 245));
    drawText(200, 323, "[J] DZIENNIK              [ESC] WYJŚCIE", rgb(220, 230, 245));
    drawText(200, 343, "Lampy to wyspy w morzu cienia,", rgb(190, 150, 230));
    drawText(200, 360, "kto z nich odpłynie, tego rozum się zmienia.", rgb(190, 150, 230));

    if ((int)(gameTime * 2.5f) % 2 == 0)
        drawTextCentered(SW / 2, 420, ">>> NACIŚNIJ DOWOLNY KLAWISZ <<<", rgb(0, 240, 255));

    // gracz, straznik i swieca
    player.facing = 3;
    player.moving = false;
    drawPlayer(300, 490);
    drawGuardianBody(480, 490);
    drawGuardianEyes(480, 490, 0.5f + 0.5f * std::sin(gameTime * 2.0f));
    drawStand(380, 490);
    drawFlame(380 + 16, 490 + 6, 1.0f);
}

void drawEnding() {
    bool keep = (flags.ending == 1);
    clearScreen(keep ? rgb(12, 20, 26) : rgb(25, 10, 12));
    drawPanel(100, 70, 600, 460, keep ? rgb(18, 30, 38) : rgb(35, 15, 20));
    drawTextCentered(SW / 2, 100, "ZAKOŃCZENIE", rgb(140, 150, 175));
    drawTextCentered(SW / 2, 125, keep ? "PAMIĘĆ" : "POPIÓŁ", keep ? rgb(0, 240, 255) : rgb(255, 80, 70), 6);

    std::string text = keep
        ? "Wynosisz pergaminy na światło dnia. To świadectwo, które trzeba badać krytycznie. Cień Asmodeusza blednie, gdy tajemnica ujawnia się światu."
        : "Płomień pochłania starożytne zwoje. Archiwum w podziemiach zapada się w ciszy. Historia, której nikt nie odkryje, pozostaje nietkniętą legendą w mrokach Langwedocji.";
    auto lines = wrap(text, 44);
    for (size_t i = 0; i < lines.size(); i++)
        drawText(136, 205 + (int)i * 20, lines[i], rgb(220, 230, 245), 2);

    drawText(136, 440, "WPISY W DZIENNIKU: " + std::to_string(journal.size()) + "/" + std::to_string(LORE_COUNT), GOLD, 2);
    if ((int)(gameTime * 2.5f) % 2 == 0)
        drawTextCentered(SW / 2, 495, "[ENTER] ZAGRAJ PONOWNIE", rgb(255, 255, 255));
}

// ---------------------------------------------------------------- sterowanie

bool anyKeyPressed() {
    for (int k = 8; k < 256; k++) {
        if (k == VK_SHIFT || k == VK_CONTROL || k == VK_MENU || k == VK_ESCAPE) continue;
        if (keyPressed(k)) return true;
    }
    return false;
}

bool confirmPressed() {
    return keyPressed('E') || keyPressed(VK_RETURN) || keyPressed(VK_SPACE);
}

void handleInput() {
    switch (state) {
    case TITLE:
        if (keyPressed(VK_ESCAPE)) { running = false; return; }
        if (anyKeyPressed()) {
            addJournal(L_INTRO);
            say("Prolog", {
                "Październik. Kościół św. Marii Magdaleny w Rennes-le-Château od dziesięcioleci jest zamknięty dla zwiedzających. Kuria w Carcassonne wysłała cię, archiwistkę, z jednym zadaniem: odnaleźć zaginioną bibliotekę księdza Saunière’a.",
                "W inwentarzu parafii figurują Pergaminy Saunière’a, lecz od lat nie ma po nich śladu. Na marginesie ktoś dopisał: «Nie szukaj przy ołtarzu. Szukaj pod pieczęcią.»",
                "Drzwi zamknęły się za tobą same. Nad progiem czytasz: Terribilis est locus iste. Świece na ołtarzu wciąż się palą, choć nikt tu nie wchodził od dziesięcioleci, a Asmodeusz przy kropielnicy zdaje się patrzeć prosto na ciebie." });
            player.facing = 1;    // drawTitle zmienia kierunek, wiec przywracam
        }
        break;

    case PLAY: {
        int tx, ty;
        Act act;
        if (confirmPressed()) {
            if (findTarget(tx, ty, act)) interact(tx, ty, act);
        } else if (keyPressed('J')) {
            journalSel = std::max(0, (int)journal.size() - 1);   // zaczynam od najnowszego wpisu
            state = JOURNAL;
        }
        break;
    }

    case DIALOG:
        if (confirmPressed()) advanceDialog();
        break;

    case JOURNAL:
        if (keyPressed('J') || keyPressed(VK_ESCAPE)) state = PLAY;
        else if (keyPressed(VK_DOWN) || keyPressed('S')) journalSel = std::min(std::max(0, (int)journal.size() - 1), journalSel + 1);
        else if (keyPressed(VK_UP) || keyPressed('W')) journalSel = std::max(0, journalSel - 1);
        break;

    case CIPHER:
        if (keyPressed(VK_LEFT) || keyPressed('A')) cipherShift = (cipherShift + 25) % 26;
        else if (keyPressed(VK_RIGHT) || keyPressed('D')) cipherShift = (cipherShift + 1) % 26;
        else if (keyPressed(VK_RETURN) || keyPressed(VK_SPACE)) submitCipher();
        else if (keyPressed(VK_ESCAPE)) state = PLAY;
        break;

    case CHOICE:
        if (keyPressed('1')) finishGame(1);
        else if (keyPressed('2')) finishGame(2);
        else if (keyPressed(VK_LEFT) || keyPressed('A')) choiceSel = 0;
        else if (keyPressed(VK_RIGHT) || keyPressed('D')) choiceSel = 1;
        else if (confirmPressed()) finishGame(choiceSel == 0 ? 1 : 2);
        break;

    case ENDING:
        if (keyPressed(VK_RETURN) || keyPressed(VK_SPACE)) {
            resetGame();
            state = TITLE;
        }
        break;
    }
}

// jedna klatka gry: logika + rysowanie
void frame() {
    gameTime += DT;
    updateFade();
    if (fade.dir == 0) handleInput();
    updateEffects();

    switch (state) {
    case TITLE:   drawTitle(); break;
    case ENDING:  drawEnding(); break;
    case CHOICE:  drawChoice(); break;
    case CIPHER:  drawCipher(); break;
    case JOURNAL: drawJournal(); break;
    default:      // PLAY i DIALOG - swiat jest widoczny w obu
        if (state == PLAY && fade.dir == 0) updateWorld();
        followCamera();
        drawScene();
        if (state == DIALOG) drawDialog();
        break;
    }
    if (fade.a > 0.0f) applyFade(fade.a);

    std::memcpy(prevKeys, keys, sizeof(keys));
}

// ---------------------------------------------------------------- okno Windows

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CLOSE:
    case WM_DESTROY:
        running = false;
        PostQuitMessage(0);
        return 0;
    case WM_KEYDOWN:
        if (wp < 256) keys[wp] = true;
        return 0;
    case WM_KEYUP:
        if (wp < 256) keys[wp] = false;
        return 0;
    case WM_KILLFOCUS:                 // zeby postac nie szla dalej po zmianie okna
        std::memset(keys, 0, sizeof(keys));
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

int main() {
    const char* className = "PergaminyWindow";
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = className;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassEx(&wc);

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE;
    RECT wr = { 0, 0, SW, SH };
    AdjustWindowRect(&wr, style, FALSE);   // okno ma miec SWxSH miejsca na grafike
    HWND hwnd = CreateWindowEx(0, className, "Pergaminy z Rennes", style,
                               CW_USEDEFAULT, CW_USEDEFAULT, wr.right - wr.left, wr.bottom - wr.top,
                               nullptr, nullptr, wc.hInstance, nullptr);

    // opis bufora pikseli dla StretchDIBits (ujemna wysokosc = rzedy od gory)
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = SW;
    bmi.bmiHeader.biHeight = -SH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    srand((unsigned)time(nullptr));
    buildVignette();
    resetGame();

    MSG msg;
    while (running) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) running = false;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        frame();

        HDC hdc = GetDC(hwnd);
        StretchDIBits(hdc, 0, 0, SW, SH, 0, 0, SW, SH, pixels, &bmi, DIB_RGB_COLORS, SRCCOPY);
        ReleaseDC(hwnd, hdc);
        Sleep(16);
    }
    return 0;
}