#include <iostream>
#include <ncurses.h>
#include <cstdlib>
#include <ctime>

#define FPS 16

int MX, MY;
int BASE = 0;
int BASEx = 0;

const char* mensaje[] = {
    "                                                                                 _nnnn_      ,\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\".",
    "                                                                                dGGGGMMb     | Hope, peace and love for you)",
    "                                                                               @p~qp~~qMb    | Merry Christmas!            )",
    "                                                                               M|@||@) M|   _;.............................'",
    " ██████╗██╗  ██╗██████╗  ██████╗  ██████╗ ████████╗██╗   ██╗██╗  ██╗           @,----.JM| -'",
    "██╔════╝██║  ██║██╔══██╗██╔═══██╗██╔═══██╗╚══██╔══╝██║   ██║╚██╗██╔╝          JS^\\__/  qKL",
    "██║     ███████║██████╔╝██║   ██║██║   ██║   ██║   ██║   ██║ ╚███╔╝          dZP        qKRb",
    "██║     ██╔══██║██╔══██╗██║   ██║██║   ██║   ██║   ██║   ██║ ██╔██╗         dZP          qKKb",
    "╚██████╗██║  ██║██║  ██║╚██████╔╝╚██████╔╝   ██║   ╚██████╔╝██╔╝ ██╗       fZP            SMMb",
    " ╚═════╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝  ╚═════╝    ╚═╝    ╚═════╝ ╚═╝  ╚═╝       HZM            MMMM",
    "                                                                           FqM            MMMM",
    "v0.3.7                                      Tool Written By Rompelhd     __| \".        |\\dS\"qML",
    "                                                                         |    `.       | `' \\Zq",
    "                  TO QUIT PRESS Q                                       _)      \\.___.,|     .'",
    "                                                                        \\____   )MMMMMM|   .'",
    "                                                                             `-'       `--' "
};

int mensaje_height = sizeof(mensaje) / sizeof(mensaje[0]);
int text_x = 50, text_y = 19;

bool is_christmas_season() {
    time_t now = time(0);
    struct tm* local_time = localtime(&now);

    int month = local_time->tm_mon + 1;
    int day = local_time->tm_mday;

    return (month == 12);
}

class CopoDeNieve {
private:
    float x, y;
    bool dead;
    float speed;
    char c;

public:
    CopoDeNieve(int x, int y);
    ~CopoDeNieve();
    void loop();
    void kill();
    bool to_remove();
};

CopoDeNieve::CopoDeNieve(int x, int y) {
    int _ = rand() % 2;
    c = (_ == 0) ? '.' : '*';
    this->x = (float) x;
    this->y = (float) y;
    dead = false;
    speed = (float) (rand() % 30) + 0.02;
    speed /= 1000;
    speed += 0.03 * (c == '*');
}

CopoDeNieve::~CopoDeNieve() {
    move(y, x); printw(" ");
}

void CopoDeNieve::kill() {
    dead = true;
}

bool CopoDeNieve::to_remove() {
    return dead;
}

void CopoDeNieve::loop() {
    move(y, x); printw(" ");
    this->y += speed;
    move(y, x); printw("%c", c);
    if (y > MY - 1 - BASE) kill();
}

class Controler {
private:
    CopoDeNieve** array;
    int n;
    int timed;
    int timed_spawn;
    int limite;
    int elim;

public:
    Controler();
    ~Controler();
    void add(CopoDeNieve*);
    void remove();
    void loop();
    void clean();
};

Controler::Controler() {
    array = NULL;
    n = 0;
    timed = 0;
    timed_spawn = 10;
    limite = 60;
    elim = 0;
}

Controler::~Controler() {}

void Controler::add(CopoDeNieve* copo) {
    if (n > 0) {
        CopoDeNieve** arrayx = new CopoDeNieve*[n + 1];
        for (int i = 0; i < n; i++) {
            arrayx[i] = array[i];
        }
        arrayx[n] = copo;
        delete[] array;
        array = arrayx;
        n++;
    } else {
        this->array = new CopoDeNieve*[1];
        array[0] = copo;
        n = 1;
    }
}

void Controler::remove() {
    int nx = 0;
    for (int i = 0; i < n; i++) {
        if (array[i]->to_remove() == false) nx++;
    }
    if (nx) {
        CopoDeNieve** arrayx = new CopoDeNieve*[nx];
        int j = 0;
        for (int i = 0; i < n; i++) {
            if (array[i]->to_remove()) {
                delete array[i];
                elim++;
            } else {
                arrayx[j] = array[i];
                j++;
            }
        }
        delete[] array;
        array = arrayx;
    } else {
        delete[] array;
        array = NULL;
    }
    n = nx;
}

void Controler::loop() {
    timed++;
    if (timed > timed_spawn && n < limite) {
        add(new CopoDeNieve(rand() % MX, 1));
        timed = 0;
    }
    if (elim > 20) {
        elim = 0;
        BASE++;
    }
    for (int i = 0; i < n; i++) {
        array[i]->loop();
    }
    remove();
}

void Controler::clean() {
    if (n) {
        for (int i = 0; i < n; i++) {
            delete array[i];
        }
        delete[] array;
        array = NULL;
        n = 0;
    }
    clear();
}

void draw_snow() {
    for (int j = 0; j < BASE; j++) {
        for (int i = 1; i < MX - 1; i++) {
            move(MY - j - 1, i);
            addch(ACS_CKBOARD);
        }
    }
}

void draw_message() {
    for (int i = 0; i < mensaje_height; i++) {
        move(i + text_y, text_x);
        printw("%s", mensaje[i]);
    }
}

void GInit() {
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    timeout(0);
}

void GEnd() {
    endwin();
}

void Gloop(int sleep_) {
    getmaxyx(stdscr, MY, MX);
    refresh();
    napms(sleep_);
}

int snowtest() {
    if (!is_christmas_season()) {
        return 0;
    }

    srand(time(NULL));
    GInit();

    Controler controler;
    bool quit = false;

    while (!quit) {
        int ch = getch();
        if (ch == 'q' || ch == 'Q') {
            quit = true;
        }

        draw_snow();
        controler.loop();
        draw_message();
        Gloop(FPS);
    }

    GEnd();
    return 0;
}
