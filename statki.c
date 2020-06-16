#include<X11/Xlib.h>
#include<X11/Xutil.h>
#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>
#include<string.h>
#include<sys/shm.h>
#include<signal.h>
#include<unistd.h>

//########################### STALE ###########################

//-----Klucze------
#define klucz_1 1111
#define klucz_2 1112
#define klucz_3 1113



//------STALE Planszy------
#define ROZMIAR 9
#define ROZMIAR_POLA 60
#define ROZMIAR_PLANSZY ROZMIAR * ROZMIAR_POLA
#define ILOSC_STATKOW_NA_GRACZA 3

#define POLE_PUSTE 0
#define POLE_STATEK 1
#define POLE_TRAFIONE 2
#define POLE_NIETRAFIONE 3



//-----Zmienne Xlib-------
#define POLE_PUSTE_KOLOR "cornflowerblue"
#define POLE_NIETRAFIONE_KOLOR "deepskyblue4"
#define POLE_STATEK_KOLOR "gray36"
#define POLE_TRAFIONE_KOLOR "firebrick3"



//########################### Struktury ###########################

//------Pole na planszy------
typedef struct XY_Strukt {
  int x;
  int y;
  int ktora_plansza;
} XY;

//------Stan gry------/
typedef struct stan_Strukt {
  int ruch;
  int ilosc;
} Stan;


//########################### Zmienne globalne ###########################


//------zmienne do xlib------
Display *display;
Window window;
int screen;
GC gc;
XEvent event;
Colormap colormap;
XColor color;
XColor color_;
int fd_x11;

//------Zmienne do gry------
int trafione_strzaly;
int pierwsza_czesc_statku_x;
int pierwsza_czesc_statku_y;
int druga_czesc_statku_x;
int druga_czesc_statku_y;
char* wiad;
int gracz1;
int gracz2;


//------Zmienne do wspol pam------
int wspoldzielona_plansza1;
int wspoldzielona_plansza2;
int wspoldzielony_stan_gry;
int** plansza_gracz1;
int** plansza_gracz2;
struct timeval czasomierz;
int* ruch;
Stan* stan_gry;


//########################### Deklaracja funkcji ###########################
//Zwraca pole na odpowiednik x,y
XY getPole(int x, int y);

/* attempt to shoot at given field */
int strzal(XY pole);

//Odpowiada za dodanie statku
bool dodajStatek(XY pole, int ktoraCzescStatku);

//Odpowiada za rozkladanie statkow dla gracza
bool rozlozStatki();

//odpowiada za wyczyszczenie/ustawienie deskryptorów oraz w pętli wykonywana jest logika gry
void gra();

// tworzenie okna itp
void inicjalizacjaXlib();

//usuwa okno
void usunOkno();

//funkcja rysujaca
void rysuj();

//init
void inicjalizacjaPamieci() ;

//wyjscie
void wyjscie();



//######################################Glowny program######################################
int main()
{
  signal(SIGINT, wyjscie);
  inicjalizacjaPamieci();
  inicjalizacjaXlib();
  
  if (!rozlozStatki()) {
    usunOkno();
    return -1;
  }
  stan_gry->ruch = gracz2;
  wiad = "Czekanie na gracza 2";
  rysuj();
  XFlush(display);
  while (stan_gry->ruch != gracz1) { usleep(100 * 1000); }
  gra();
  usunOkno();
  wyjscie();
  return 0;
}

//######################################Ciala Funckji######################################


//--------------------------------------Funkcje Logiki Gry---------------------------------
XY getPole(int x, int y) {
    int ktora_plansza = 0;
    int pole_x = 0;
    int pole_y = 0;

    bool pozycjaXGracz_1 = x >= 0 && x <= ROZMIAR_PLANSZY;
    bool pozycjaYGracz_1 = y >= 80 && y <= ROZMIAR_PLANSZY + 80;
    bool pozycjaXGracz_2 = x >= ROZMIAR_PLANSZY + 20 && x <= 2*ROZMIAR_PLANSZY + 20;
    bool pozycjaYGracz_2 = y >= 80 && y <= 80 + ROZMIAR_PLANSZY;
  
    if (pozycjaXGracz_1  && pozycjaYGracz_1) {
        ktora_plansza = 1;
    }
    if (pozycjaXGracz_2 && pozycjaYGracz_2) {
        ktora_plansza = 2;
        x -= (ROZMIAR_PLANSZY + 20);
    }

    pole_x = x / ROZMIAR_POLA;
    pole_y = (y - 80) / ROZMIAR_POLA;
    XY pole = {.x = pole_x, .y = pole_y, .ktora_plansza = ktora_plansza};
    return pole;
}


bool rozlozStatki()
{
  wiad = "Rozloz swoje statki po lewej stronie";
  int temp = ILOSC_STATKOW_NA_GRACZA*3;
  while (temp > 0) {
    rysuj();
    XFlush(display);
    XNextEvent(display, &event);
    if(event.type == ButtonPress) {
      int ktoraCzescStatku = 0;
      if(temp % 3 == 0)
        ktoraCzescStatku = 1;
      if(temp % 3 == 2) 
        ktoraCzescStatku = 2;
      if(temp % 3 == 1)
        ktoraCzescStatku = 3;
      if(dodajStatek(getPole(event.xbutton.x, event.xbutton.y), ktoraCzescStatku))
        temp = temp - 1;
    }
    if(event.type == ClientMessage)
    {
      printf("Zamknieto!\n");
      stan_gry->ruch = -gracz2;
      return false;
    }
  }
  return true;
}



bool dodajStatek(XY pole, int ktoraCzescStatku) {
  int x = pole.x;
  int y = pole.y;

  if (pole.ktora_plansza == 0 || pole.ktora_plansza == 2) {
    wiad = "Poloz statek na wlasna plansze";
    return 0;
  }
  if (pole.ktora_plansza == 1) {
    if (plansza_gracz1[x][y] == POLE_PUSTE) {
      if (ktoraCzescStatku == 3) {
        bool dobreUlozenie = false; 
        if((x == druga_czesc_statku_x && y == druga_czesc_statku_y - 1) || (x == druga_czesc_statku_x && y == druga_czesc_statku_y + 1)|| (x == druga_czesc_statku_x + 1 && y == druga_czesc_statku_y) || (x == druga_czesc_statku_x - 1 && y == druga_czesc_statku_y))
          if((x == pierwsza_czesc_statku_x && y == pierwsza_czesc_statku_y - 2) || (x == pierwsza_czesc_statku_x && y == pierwsza_czesc_statku_y + 2)|| (x == pierwsza_czesc_statku_x + 2 && y == pierwsza_czesc_statku_y) || (x == pierwsza_czesc_statku_x - 2 && y == pierwsza_czesc_statku_y))
            dobreUlozenie = true;
        if((x == druga_czesc_statku_x && y == druga_czesc_statku_y - 2) || (x == druga_czesc_statku_x && y == druga_czesc_statku_y + 2)|| (x == druga_czesc_statku_x + 2 && y == druga_czesc_statku_y) || (x == druga_czesc_statku_x - 2 && y == druga_czesc_statku_y))
          if((x == pierwsza_czesc_statku_x && y == pierwsza_czesc_statku_y - 1) || (x == pierwsza_czesc_statku_x && y == pierwsza_czesc_statku_y + 1)|| (x == pierwsza_czesc_statku_x + 1 && y == pierwsza_czesc_statku_y) || (x == pierwsza_czesc_statku_x - 1 && y == pierwsza_czesc_statku_y))
            dobreUlozenie = true;
        if (!dobreUlozenie) {
          wiad = "Ukladaj statki w prostej linii!";
          return false;
        }
      }
      if (ktoraCzescStatku == 2) {
        bool dobreUlozenie = false; 
        if((x == druga_czesc_statku_x && y == druga_czesc_statku_y - 1) || (x == druga_czesc_statku_x && y == druga_czesc_statku_y + 1)|| (x == druga_czesc_statku_x + 1 && y == druga_czesc_statku_y) || (x == druga_czesc_statku_x - 1 && y == druga_czesc_statku_y))
          dobreUlozenie = true;
        if (!dobreUlozenie) {
          wiad = "Ukladaj statki w prostej linii!";
          return false;
        }
      }
      plansza_gracz1[pole.x][pole.y] = POLE_STATEK;
      
      pierwsza_czesc_statku_x = druga_czesc_statku_x;
      pierwsza_czesc_statku_y = druga_czesc_statku_y;
      druga_czesc_statku_x = pole.x;
      druga_czesc_statku_y = pole.y;
      wiad = "Dodano";
      return true;
    }
    

    if (plansza_gracz1[x][y] == POLE_STATEK) {
      wiad = "Miejsce zajete";
      return false;
    }
  }
}



int strzal(XY pole) {
  if (pole.ktora_plansza == 0)  {
    wiad = "Brak planszy";
    return -1;
  }
  if (pole.ktora_plansza == 1) {
    wiad = "Nie mozna strzelac w wlasna";
    return 0;
  }
  if (pole.ktora_plansza == 2) {
    if (plansza_gracz2[pole.x][pole.y] == POLE_STATEK) {
      plansza_gracz2[pole.x][pole.y] = POLE_TRAFIONE;
      trafione_strzaly = trafione_strzaly + 1;
      wiad = "Trafione!";
      return 1;
    }
    if (plansza_gracz2[pole.x][pole.y] == POLE_PUSTE) {
     plansza_gracz2[pole.x][pole.y] = POLE_NIETRAFIONE;
      wiad = "Kolej przeciwnika!";
    }
    if (plansza_gracz2[pole.x][pole.y] == POLE_NIETRAFIONE) {
      wiad = "Kolej przeciwnika!";
    }
    if (plansza_gracz2[pole.x][pole.y] == POLE_TRAFIONE) {
      wiad = "Kolej przeciwnika!";
    }
    return 2;
  }
}



void gra() {
  wiad = "Start!";
  
  fd_set deskryptory_plikow;

  while(stan_gry->ruch > 0)
  {
    FD_ZERO(&deskryptory_plikow);
    FD_SET(fd_x11, &deskryptory_plikow);
    
    //Konfig czasomierz
    czasomierz.tv_sec = 1;
    czasomierz.tv_usec = 0;
    
    //Monitorowanie
    if (select(fd_x11 + 1, &deskryptory_plikow, NULL, NULL, &czasomierz) == 0) {
        if (stan_gry -> ruch == gracz1) {
          wiad= "Twoj ruch!";
          rysuj();
          XFlush(display);
        }
    }

    // Petla: odpowiada za calą grę/sprawdza warunki wygranej przez strzaly
    while(XPending(display)) {
      XNextEvent(display, &event);
      if(event.type == ButtonPress) {
        if (stan_gry -> ruch == gracz1) {

          int temp_strzal = strzal(getPole(event.xbutton.x, event.xbutton.y));
          if (temp_strzal == 1) {
            if (trafione_strzaly >= ILOSC_STATKOW_NA_GRACZA * 3) {
              stan_gry->ruch = -gracz1;
            }
          }
          else {
            if(temp_strzal == 2)
              stan_gry->ruch = gracz2;
          }
          rysuj();
          XFlush(display);
        }
      }
      if(event.type == ClientMessage) {
        stan_gry->ruch = -gracz2;
        return;
      }
      //Oczyszcza buffer wyjscia
      XFlush(display);
    }
  }
  //Warunki wygranej
  if (stan_gry->ruch == -gracz1) {
    wiad = "Wygrana!";
  }
  if (stan_gry->ruch == -gracz2)
  {
    wiad = "Przegrana!";
  }
  rysuj();
  XFlush(display);
  XNextEvent(display, &event);
}



//--------------------------------------Rysowanie---------------------------------
void rysuj() {
  //Rysowanie tla
  XAllocNamedColor(display, colormap, "dodgerblue4", &color, &color_);
  XSetForeground(display, gc, color.pixel);
  XFillRectangle(display, window, gc, 0, 0, ROZMIAR_PLANSZY * 2 + 20, ROZMIAR_PLANSZY + 80);
  //Rysowanie wiadomosci
  XAllocNamedColor(display, colormap, "dodgerblue4", &color, &color_);
  XSetForeground(display, gc, color.pixel);
  XFillRectangle(display, window, gc, 0, 10, ROZMIAR_PLANSZY * 2 + 20, 20);
  XAllocNamedColor(display, colormap, "white", &color, &color_);
  XSetForeground(display, gc, color.pixel);
  XDrawString(display, window, gc, (ROZMIAR_PLANSZY * 2 + 20)/2, 20, wiad, strlen(wiad));
  //rysowanie ramek pol
  for (int i = 0; i < ROZMIAR + 1; i++) {
    XAllocNamedColor(display, colormap, "green", &color, &color_);
    XSetForeground(display, gc, color.pixel);
    XDrawLine(display, window, gc, 0, ROZMIAR_POLA * i + 80, ROZMIAR_PLANSZY, ROZMIAR_POLA * i + 80 );
    XDrawLine(display, window, gc, ROZMIAR_POLA * i, 80, ROZMIAR_POLA * i, ROZMIAR_PLANSZY + 80);
    
    XAllocNamedColor(display, colormap, "red", &color, &color_);
    XSetForeground(display, gc, color.pixel);
    XDrawLine(display, window, gc, ROZMIAR_PLANSZY + 20, ROZMIAR_POLA * i + 80, 2 * ROZMIAR_PLANSZY + 20, ROZMIAR_POLA * i + 80);
    XDrawLine(display, window, gc, ROZMIAR_POLA * i + ROZMIAR_PLANSZY + 20, 80, ROZMIAR_POLA * i + ROZMIAR_PLANSZY + 20, ROZMIAR_PLANSZY + 80);
  }
  //wypelnianie pol planszy 1
  for (int i = 0; i < ROZMIAR; i++) {
    for (int j = 0; j < ROZMIAR; j++) {
      if(plansza_gracz1[i][j] == POLE_PUSTE) {
        XAllocNamedColor(display, colormap, POLE_PUSTE_KOLOR, &color, &color_);
      }
      if(plansza_gracz1[i][j] == POLE_NIETRAFIONE) {
          XAllocNamedColor(display, colormap, POLE_NIETRAFIONE_KOLOR, &color, &color_);
      }
      if(plansza_gracz1[i][j] == POLE_STATEK) {
        XAllocNamedColor(display, colormap, POLE_STATEK_KOLOR, &color, &color_);
      }
      if(plansza_gracz1[i][j] == POLE_TRAFIONE) {
        XAllocNamedColor(display, colormap, POLE_TRAFIONE_KOLOR, &color, &color_);
      }
      XSetForeground(display, gc, color.pixel);
      XFillRectangle(display, window, gc, ROZMIAR_POLA * i + 1, ROZMIAR_POLA * j + 80 + 1, ROZMIAR_POLA - 1, ROZMIAR_POLA - 1);
    }
  }
  //wypelnianie pol planszy 2
  for (int i = 0; i < ROZMIAR; i++) {
    for (int j = 0; j < ROZMIAR; j++) {
      if(plansza_gracz2[i][j] == POLE_PUSTE) {
        XAllocNamedColor(display, colormap, POLE_PUSTE_KOLOR, &color, &color_);
      }
      if(plansza_gracz2[i][j] == POLE_NIETRAFIONE) {
        XAllocNamedColor(display, colormap, POLE_NIETRAFIONE_KOLOR, &color, &color_);
      }
      if(plansza_gracz2[i][j] == POLE_STATEK) {
        XAllocNamedColor(display, colormap, POLE_PUSTE_KOLOR, &color, &color_);
      }
      if(plansza_gracz2[i][j] == POLE_TRAFIONE) {
        XAllocNamedColor(display, colormap, POLE_TRAFIONE_KOLOR, &color, &color_);              
      }
      XSetForeground(display, gc, color.pixel);
      XFillRectangle(display, window, gc, ROZMIAR_POLA*i + 1 + ROZMIAR_PLANSZY + 20, ROZMIAR_POLA * j + 80 + 1, ROZMIAR_POLA - 1, ROZMIAR_POLA-1);
    }
  }
}




//--------------------------------------Xlib---------------------------------
void inicjalizacjaXlib() {
  display = XOpenDisplay(NULL);
  if(display == NULL) {
    wyjscie();
  }
  screen = DefaultScreen(display);
  gc = DefaultGC(display, screen);
  window = XCreateSimpleWindow(display, RootWindow(display, screen), 0, 0, 2 * ROZMIAR_PLANSZY + 20, ROZMIAR_PLANSZY + 80, 1, BlackPixel(display, screen), WhitePixel(display, screen));
  Atom delete_window = XInternAtom(display, "WM_DELETE_WINDOW", 0);
  XSetWMProtocols(display, window, &delete_window, 1);
  XGrabPointer(display, window, False, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
  XSelectInput(display, window, ExposureMask | KeyPressMask | ButtonPressMask | StructureNotifyMask);
  XMapWindow(display, window);
  colormap = DefaultColormap(display, screen);
  fd_x11 = ConnectionNumber(display);
}



void usunOkno() {
  XDestroyWindow(display, window);
  XCloseDisplay(display);
}



//--------------------------------------Pamiec Wspoldzielona---------------------------------
void inicjalizacjaPamieci() {
  if ((wspoldzielona_plansza1 = shmget(klucz_1, 1024, 0666 | IPC_CREAT | IPC_EXCL)) != -1) {
    gracz1 = 1;
    gracz2 = 2;
    wspoldzielona_plansza2 = shmget(klucz_2, 1024, 0666 | IPC_CREAT);
    wspoldzielony_stan_gry = shmget(klucz_3, sizeof(Stan), 0666 | IPC_CREAT);
  }
  else {
    gracz1 = 2;
    gracz2 = 1;
    wspoldzielona_plansza1 = shmget(klucz_1, 1024, 0666 | IPC_CREAT);
    wspoldzielona_plansza2 = shmget(klucz_2, 1024, 0666 | IPC_CREAT);
    wspoldzielony_stan_gry = shmget(klucz_3, sizeof(Stan), 0666 | IPC_CREAT);
  }

  //Pobranie wspolnej pamieci
  int* shm_board_1 = shmat(wspoldzielona_plansza1, 0, 0);
  int* shm_board_2 = shmat(wspoldzielona_plansza2, 0, 0);
  stan_gry = shmat(wspoldzielony_stan_gry, 0, 0);

  plansza_gracz1 = malloc(ROZMIAR * sizeof(plansza_gracz1[0]));
  plansza_gracz2 = malloc(ROZMIAR * sizeof(plansza_gracz2[0]));

  if(stan_gry -> ilosc > 1) {
    exit(-1);
  }

  for (int i = 0; i < ROZMIAR; i++) {
    if (gracz1 == 1) {
      plansza_gracz1[i] = shm_board_1 + i * ROZMIAR;
      plansza_gracz2[i] = shm_board_2 + i * ROZMIAR;
    }
    if (gracz1 == 2) {
      plansza_gracz1[i] = shm_board_2 + i * ROZMIAR;
      plansza_gracz2[i] = shm_board_1 + i * ROZMIAR;
    }
  }

  //Jesli jest pierwszym graczem: stworz plansze
  if (gracz1 == 1)
  {
    for (int i = 0; i < ROZMIAR; i++)
    {
      for (int j = 0; j < ROZMIAR; j++)
      {
        plansza_gracz1[i][j] = POLE_PUSTE;
        plansza_gracz2[i][j] = POLE_PUSTE;
      }
    }
    stan_gry -> ilosc = 0;
    stan_gry -> ruch = gracz1;
  }
  stan_gry -> ilosc += 1;
}



void wyjscie()
{
  shmctl(wspoldzielona_plansza1, IPC_RMID, 0);
  shmctl(wspoldzielona_plansza2, IPC_RMID, 0);
  shmctl(wspoldzielony_stan_gry, IPC_RMID, 0);
  stan_gry -> ruch = -gracz2;
  stan_gry -> ilosc -= 1;
  exit(0);
}