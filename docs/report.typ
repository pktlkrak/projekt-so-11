#import "@preview/fletcher:0.5.8" as fletcher: diagram, node, edge
#import fletcher.shapes: diamond
#set text(lang: "pl")
#set par(justify: true)
#let defaultSize = 10pt
#set text(size: defaultSize)

#set page(
  margin: 5em,
  numbering: "1",

  header: smallcaps({
    grid(columns: (1fr, 1fr, 1fr), align: (left, center, right),
      "Politechnika Krakowska",
      "Zadanie Projektowe - SO",
      "Piotr K."
    )
    v(-.7em)
    line(start: (0%, 0pt), end: (100%, 0pt))
  })
)

== Ogólna architektura projektu

Mój projekt składa się z pięciu programów:

- Łódka (folder `boat`)
- Pasażer (folder `passenger`)
- Dyspozytor (folder `dispatcher`)
- Serwer logów (folder `ioman`)
- Kontroler symulacji (folder `controller`)

Stan podstawowy projetu zakłada uruchomienie dwóch dyspozytorów, oraz łodzi pływającej między nimi.
Pasażerowie łączą się z dyspozytorem u którego zaczynają swoją podróż.
Pytają go czy można stanąć na moście, następnie na łodzi.
Jeżeli u danego dyspozytora w danym momencie nie ma łodzi, pasażer może wejść na most, lecz zostanie z niego
wyproszony przez dyspozytora w momencie przypływu łodzi, jeżeli na łodzi są przyjezdni.
Po opuszczeniu pokładu przez przybyłych pasażerów, nowi pasażerowie zajmują ich miejsce.
Kiedy łódź zostanie zapełniona, dyspozytor wysyła do kapitana sygnał wczesnego odpływu, w innym przypadku łódź czeka przez czas `T1`.
Przed odpływem, z mostu wypraszani są wszyscy pasażerowie.
Po dopłynięciu do drugiego dyspozytora, pasażerowie proszą o zejście z pokładu. Po opuszczeniu łodzi kończą oni swój żywot.

Dyspozytorzy są odpowiedzialni za utrzymywanie swoich kolejek wiadomości, po których wysyłane są wiadomości między nimi, a łodzią i pasażerami.
Pokład łodzi symbolizowany jest przez pamięć współdzieloną.
Sygnały wykorzystywane są do powiadamiania pasażerów i łodzi o zmianie stanu symulacji.

#pagebreak()

=== Opis działania łódki

- Łódka jako argumenty `argv` przyjmuje pliki stanowiące klucz do `ftok()` obu dyspozytorów.
- Łódka w momencie uruchomienia tworzy, a przy wychodzeniu kasuje, swoją pamięć współdzieloną.
- Łódka w momencie otrzymania SIGTERM wysyła wiadomość o zakończeniu symulacji do obu dyspozytorów, oraz propaguje SIGTERM do wszystkich obecnych pasażerów.

#align(center, [
    #diagram(node-stroke: 1pt, 
        node((0, 0), [Start], ),
        edge("-|>"),
        node((0, 1), [Wybierz dyspozytora 1 jako aktywnego]),
        edge("-|>"),
        node((0, 2), [Poinformuj obecnego dyspozytora o przyjeździe łódki]),
        edge("-|>"),
        node((0, 3), [Poinformuj wszystkich pasażerów o końcu podróży]),
        edge("-|>"),
        node((0, 4), [Poczekaj na sygnał odjazdu od dyspozytora, lub przez `T1` sekund]),
        edge("-|>"),
        node((0, 5), [


            Czy łódź powinna skończyć symulację \
            (otrzymano sygnał końca lub odbyto `R` przejazdów?)
        
        ], shape: diamond),
        edge("r,d", "-|>", [Tak]),
        node((1, 6), [
            Zakończ symulację\
            (raise SIGTERM)
        ]),
        edge((0, 5), "d", "-|>", [Nie]),
        node((0, 6), [Wyślij wiadomość o odjeździe do dyspozytora]),
        edge("-|>"),
        node((0, 7), [Poczekaj przez czas `T2`]),
        edge("-|>"),
        node((0, 8), [Przełącz na przeciwny dyspozytor]),
        edge("l,u,u,u,u,u,u,r", "-|>")
        
    )
])

#pagebreak()

=== Opis działania pasażera

- Pasażer w `argv` otrzymuje plik stanowiący klucz początkowego dyspozytora, u którego rozpocznie swoją podróż
- Pasażer sam w sobie nie zarządza żadnymi zasobami, jedynie je wykorzystuje


#align(center, [
    #diagram(node-stroke: 1pt, edge-stroke: 0.5pt, 
        node((0, 0), [Start], ),
        edge("-|>"),
        node((0, 1), [Połącz z początkowym dyspozytorem]),
        edge("-|>"),
        node((0, 2), [
            Zapytaj dyspozytora o możliwość wejścia na most,\
            czekaj na zgodę
        ]),
        edge("-|>"),
        node((0, 3), [
            Czekaj na wiadomość od dyspozytora z nowym miejscem na łodzi
        ]),
        edge("-|>"),
        node((0, 4), [
            Otrzymano miejsce na łodzi?
        ], shape: diamond),
        edge("l,u,u,r", "-|>", [Nie]),
        edge("-|>", [Tak]),
        node((0, 5), [
            Połącz się z pamięcią współdzieloną\
            łodzi, zapisz swój PID na łodzi
        ]),
        edge("-|>"),
        node((0, 6), [
            Wyślij wiadomość dyspozytorowi,\
            że zeszliśmy z mostu i jesteśmy \
            na łodzi
        ]),
        edge("-|>"),
        node((0, 7), [
            Czekaj na sygnał końca podróży\
            od łodzi
        ]),
        edge("-|>"),
        node((0, 8), [
            Wyślij wiadomość nowemu dyspozytorowi,\
            że chcemy zejść z łodzi
        ]),
        edge("-|>"),
        node((0, 9), [
            Czekaj na sygnał zejścia z łodzi
        ]),
        edge("-|>"),
        node((0, 10), [Koniec])
    )
])

#pagebreak()

=== Opis działania dyspozytora

- Dyspozytor w `argv` otrzymuje nazwę pliku klucza, dla którego zostanie utworzona kolejka wiadomości
- Po zakończeniu programu, dyspozytor usunie kolejkę wiadomości
- Dyspozytor czeka na wiadomości, a następnie przesyła odpowiednie sygnały i wiadomości do podległych procesów

#set text(size: 6pt)
#align(center, [
    #diagram(node-stroke: 1pt, edge-stroke: 0.5pt,
        node((0, 0), [Start], ),
        edge("-|>"),
        node((0, 1), [Utwórz kolejkę wiadomości]),
        edge("-|>"),
        node((0, 2), [Czekaj na nową wiadomość]),
        edge("-|>"),
        node((-2, 3), [
            Pasażer chce \
            wejść na most
        ]),
        edge("-|>"),
        node((-2, 4), [
            Spróbuj dodać\
            pasażera na most
        ]),
        edge("-|>"),
        node((-2, 5), [
            Zmieścił się?
        ], shape: diamond),
        edge((-2, 5), "l,u", "-", [Tak], label-pos: (0, 0.5)),
        edge((-2, 5), "d", "-|>", [Nie]),
        node((-2, 6), [Dopisz do kolejki]),
        edge((-2, 6), "l,u"),

        edge((0, 2), (-1, 3), "-|>"),
        node((-1, 3), [Łódź dopłynęła]),
        edge("-|>"),
        node((-1, 4), [
            Zapisz dane \
            potrzebne do komunikacji \
            z łodzią
        ]),
        edge("-|>"),
        node((-1, 5), [
            Czy łódź ma pasażerów?
        ], shape: diamond),
        edge("-|>", [Tak]),
        node((-1, 6), [
            Opróżnij most
        ]),
        edge((-1, 6), (-1, 6.5), "l,l,u", "-"),
        edge((-1.55, 5), (-1.5, 5), "d,d", "-|>", [Nie]),
        node((-1, 7), [Zezwól pasażerom na wejście na most]),
        edge("l,l,u", "-"),


        edge((0, 2), (0, 3), "-|>"),
        node((0, 3), [Łódź odpływa]),
        edge("-|>"),
        node((0, 4), [
            Zerwij komunikację \
            z łodzią
        ]),
        edge("d,d,l", "-|>"),

        edge((0, 2), (1, 3), "-|>"),
        node((1, 3), [
            Pasażer chce\
            zejść z łodzi
        ]),
        edge("-|>"),
        node((1, 4), [
            Odeślij zgodę na zejście
        ]),
        edge("-|>"),
        node((1, 5), [
            Zmniejsz licznik pasażerów \
            na łodzi
        ]),
        edge("-|>"),
        node((1, 6), [
            Czy zero?
        ], shape: diamond),
        edge("r,d,d,l", "-", [Nie], label-pos: (0, 0.5)),
        edge((1, 6), "d", "-|>", [Tak]),
        node((1, 7), "Wyczyść łódź"),
        edge("l,l", "-|>"),

        edge((0, 2), (2, 3), "-|>"),
        node((2, 3), [
            Pasażer wszedł\
            na łódź
        ]),
        edge("-|>"),
        node((2, 4), [
            Zajmij miejsce na łodzi
        ]),
        edge("-|>"),
        node((2, 5), [
            Czy łódź pełna?
        ], shape: diamond),
        edge("d,d", "-", [Nie], label-pos: (0, 0.5)),
        edge((2, 5), "r", "-|>", [Tak]),
        node((3, 5), [
            Wyślij łodzi sygnał\
            wczesnego odpływu
        ]),
        edge((3, 5), "d,d,d,l,l,l,l,l,l,u,u,u,u,u,u,r,r,r", "-|>"),
        edge((0, 2), (2, 2), "-|>"),
        node((2, 2), [
            Zakończ symulację
        ]),
    )
])

Dodatkowo, dyspozytor ma drugi wątek, który przenosi pasażerów na łódź
z mostu, jeżeli jest to możliwe:


#set text(size: 6pt)
#align(center, [
    #diagram(node-stroke: 1pt, edge-stroke: 0.5pt,
        node((0, 0), [Poczekaj aż zmieni się sytuacja na moście]),
        edge("-|>"),
        node((0, 1), [
            Czy łódź jest przy naszym dyspozytorze,\
            mamy pasażera na moście \
            i łódź nie jest rozładowywana?
        ], shape: diamond),
        edge("l,u,r", "-|>", [Nie]),
        edge((0, 1), (0, 2), "-|>", [Tak]),
        node((0, 2), [
            Czy pasażer stojący najbliżej łodzi ma rower?
        ], shape: diamond),
        edge("-|>", [Tak]),
        node((-1, 3), [
            Czy na łodzi jest wolne miejsce na rower?
        ], shape: diamond),
        edge("-|>", [Nie]),
        node((-2, 4), [
            Poinformuj pasażera,\
            że ma zejść z mostu i wrócić do kolejki
        ]),
        edge((-1, 3), (-1, 4), "-|>", [Tak]),
        node((-1, 4), [
            Zmniejsz ilość wolnych miejsc na rowery
        ]),
        edge((-1, 4), (0, 5), "-|>"),
        edge((0, 2), "d,d,d", "-|>", [Nie]),
        node((0, 5), [
            Poinformuj pasażera o jego numerze miejsca na łodzi
        ]),
        edge("d,r", "-"),
        edge((-2, 4), "d,d,r,r,r,r,u,u,u,u,u,l,l", "-|>"),
    )
])
#set text(size: defaultSize)


#pagebreak()

== Problemy napotkane podczas pisania projektu

Największe problemy oczywiście pojawiły się z dwukierunkową komunikacją międzyprocesową oraz synchronizacją logów.

- #[
    Problem logów został rozwiązany przy użyciu serwera, którego zadaniem jest zapisywanie i wyświetlanie logów w odpowiedniej kolejności.
    Ponieważ opóźnienia mogą pojawić się pomiędzy wysyłaniem wiadomości po sockecie a otrzymaniem go przez serwer, każdy pakiet logów
    zawiera swój własny timestamp. Serwer logów posiada bufor kilku ostatnich wiadomości. Kiedy otrzymywana seria wiadomości jest tak duża, że przepełnia
    bufor serwera logów, serwer wybiera najstarszy log, wypisuje go, a następnie na zwolnione miejsce wprowadza nowy log. Jeżeli przez więcej niż sekundę nie otrzymywany
    jest żaden nowy log, cały bufor jest opróżniany zgodnie z kolejnością zapisaną w timestamp'ach logów.
]

- Problem komunikacji dwukierunkowej został rozwiązany przy użyciu dodatkowych funkcji kolejek komunikatów:
  - Jeżeli argumentem `msgtyp` `msgrcv()` jest wartość `0`, odczyta on każdy przychodzący komunikat - był to pierwszy (jednak nietrafiony)
    wybór na funkcję przyjmującą wiadomości w dyspozytorze.
  - Jeżeli argumentowi temu przypisze się wartość dodatnią, będzie czekał na konkretny komunikat, o ustalonym ID (danym w `msgtyp`) - jest to funkcja wykorzystana w
    implementacji pasażera.
  - Jeżeli argumentem tym będzie liczba ujemna, `msgrcv()` będzie czekało na komunikaty o ID mniejszym lub równym wartości bezwzględnej `msgtyp` - jest to idealne
    rozwiązanie dla dystrybutora, ponieważ w ten sposób wiadomości wysyłane do konkretnych pasażerów mogą nie być wykrywane przez dystrybutora, o ile `msgtyp` w pasażerach
    będzie większy niż ustalona wartość (w moim przypadku ustalona wartość to 0x40000000 - drugi najwyższy bit liczby 32-bitowej - pierwszy nie mógł zostać użyty z uwagi na kodowanie U2).

- #[
    Innym problemem okazało się zmuszenie wątków uruchomionych przez `pthread_create` do nie wywoływania handlerów sygnałów (zdefiniowanych przez `signal()`) - ten problem udało mi
    się rozwiązać przy użyciu wywołań `pthread_sigmask()`.
]

- #[
    Dośc ciekawym problemem, który zdecydowałem się rozwiązać jest sama dokumentacja, oraz linkowanie odpowiednich fragmentów kodu tak, aby linki nie uległy
    uszkodzeniu w razie potrzeby poprawy plików, co spowoduje przesunięcie niektórych fragmentów kodu.
    W tym celu wykorzystałem skryptowe możliwości języka `typst`, w którym napisany jest ten raport, oraz języka python.
    Skrypt `genref.py` generuje plik `codelinks.json`, wyszukując konkretne linijki kodu, który następnie jest parsowany przez kompilator `typst`,
    a odpowiednie linki automatycznie zamieszczane są we wskazanych przeze mnie miejscach.
]

== Funkcje specjalne

Aby uporządkować logi, zdecydowałem się zaimplementować funckję `iomanTakeoverStdio()` w kliencie serwera logów. Wywołanie funkcji tej przekieruje wszystkie logi wysyłane
na STDERR i STDOUT, do serwera logowania. Udało się to dzięki użyciu funkcji `dup2()` która klonuje i podmienia podany `FD` na inny - w moim przypadku na wcześniej utworzony
`FD` struktury fifo utworzonej przez syscall `pipe()`.

== Testy


=== Test 1: Test obciążeniowy

Test ten przeszedł pomyślnie - utworzyłem 10000 pasażerów połączonych do każdego z dwóch dyspozytorów (przy użyciu opcji `1` kontrolera).
Każdy z nich bezpiecznie odbył swoją podróż.

Jedynym napotkanym problemem okazał się limit bajtów na każdą z kolejek sys-v. Problem ten naprawiłem komendą `echo $((5 * 1024 * 1024)) | sudo tee /proc/sys/kernel/msgmnb`, zwiększając limit do 5MiB.

=== Test 2: Natłok rowerzystów

Test ten miał zweryfikować, czy dyspozytor poprawnie odrzuca rowerzystów, jeżeli na łodzi nie ma miejsca dla ich rowerów - on także wypadł pomyślnie.

Przeprowadziłem go poprzez ustalenie wartości BOAT_CAPACITY = 10, BOAT_BIKE_CAPACITY = 1, a następnie przejście w tryb `2` - sterowanie ręczne.
Stworzyłem 10 pasażerów z rowerem, oraz jednego bez. Na łodzi podczas pierwszej podróży będzie, zgodnie z oczekiwaniem, bedzie dwóch pasażerów - jeden z rowerem,
drugi bez.

=== Test 3: 

=== Test 4: Test sygnału wczesnego zatrzymania podczas załadunku

Po otrzymaniu sygnału wczesnego zatrzymania podczas trasy,
pasażerowie byli zmuszeni do opuszczenia łodzi. Kiedy ostatni z niej zszedł, symulacja została zatrzymana.

=== Test 5: Test sygnału wczesnego zatrzymania podczas trasy

Po otrzymaniu sygnału wczesnego zatrzymania podczas trasy,
łódź dopłynęła do swojego celu, a następnie zakończyła symulację.

=== Test 6: Test sygnału wczesnego wypłynięcia

Test ten został zdany jako część testu pierwszego.
Dzięki temu sygnałowi test pierwszy zajął bardzo mało czasu.

* TODO *

== Istotne fragmenty kodu

#let codeLinks = json("codelinks.json")
#let coderef(number, text: none) = {
    if text == none {
        text = codeLinks.at(number).at(0)
    }
    show link: underline
    let ref = codeLinks.at(number);
    link(ref.at(1))[#text]
}

=== Tworzenie i obsługa plików

Ponieważ moja symulacja operuje głównie na strukturach kontrolnych SYS-V, pliki wykorzystane są głównie do logów:

- #coderef(0, text: `creat()`), #coderef(1, text: `open()`) - wykorzystywane do tworzenia plików logów
- #coderef(2, text: `read()`), #coderef(3, text: `write()`) - są m.in. użyte do przekierowywania logów z stdio
- #coderef(4, text: `close()`) - wykorzystywane do zamykania wszystkich struktur kontrolnych jako fragment hook'a `atexit()`
- #coderef(5, text: `unlink()`) - służy do usunięcia pliku logów jeżeli do serwera logów nie zostanie podana opcja `-a` - append

=== Tworzenie procesów

Za tworzenie procesów odpowiedzialny jest "kontroler" - jest to frontend do całej symulacji:

- #coderef(6, text: `fork()`), #coderef(6, text: `exec()`), #coderef(7, text: `waitpid()`) - Wykorzystane są jako makra w aplikacji kontrolera symulacji. Służą do przygotowania środowiska symulacji poprzez tworzenie wymaganych procesów
- #coderef(8, text: `exit()`) - Wykorzystywane jest jako sposób zakończenia aplikacji w przypadku błędu

=== Tworzenie i obsługa wątków

W mojej symulacji wątki użyte są między innymi do przenoszenia pasażerów z mostu na łódź wewnątrz dyspozytora,
oraz do oczekiwania na zakończenie konkretnego procesu utworzonego przez kontroler:

- #coderef(16, text: `pthread_join()`) - Wywołanie to zatrzymuje wątek przekierowywania IO jako część funkcji kończących symulację
- #coderef(17, text: `pthread_create()`) - Tworzenie wątku mostu
- #coderef(18, text: `pthread_mutex_lock()`), #coderef(19, text: `pthread_mutex_unlock()`) - Muteksy zabezpieczają dostęp do struktury mostu wewnątrz kodu dyspozytora
- #coderef(20, text: `pthread_cond_wait()`), #coderef(21, text: `pthread_cond_broadcast()`) - Zmienne warunkowe służą do przekazywania informacji do wątku mostu, kiedy należy zreewaluować, czy można przeładowywać pasażerów na łódź.
- #coderef(22, text: `pthread_detach()`) - Kontroler wykorzystuje wątki do asynchronicznego czekania na zakończenie procesu dziecka - wątek jest tymczasowy i kontroler może pozwolić na samoistne usunięcie zasobów po zakończeniu wątku

=== Obsługa synałów

Sygnały wykorzystane są w wielu miejscach jako prosty sposób na powiadomienie innego procesu o zmianie stanu symulacji

- #coderef(9, text: `kill()`) - w tym przypadku metoda ta powiadamia pasażera, który czeka na sygnał (#coderef(10, text: `sigwait()`)), że dostał zgodę na wejście na most
- #coderef(11, text: `raise()`) - funkcja ta została wykorzystana jako łatwy sposób wyczyszczenia środowiska symulacji - wykonywana jest ta sama ścieżka kodowa, która wykonałaby się w przypadku otrzymania sygnału SIGTERM (zastosowanie zasady `DRY` - Don't Repeat Yourself)
- #coderef(12, text: `signal()`) - w tym miejscu, funkcja ta wykorzystywana jest aby zarejestrować handler dla sygnału wczesnego zakończenia symulacji.

=== Synchronizacja wątków / procesów

Semafory nie zostały wykorzystane. Zastąpiłem je muteksami opsisanymi w sekcji `Tworzenie i obsługa wątków`.

=== Łącza nazwane i nienazwane

Łącza nienazwane stosuję do zarządzania systemem przekierowywania stdio

- #coderef(13, text: `pipe()`) - funkcja ta wykorzysywana jest między innymi do stworzenia "FIFO kontrolnego", które pomaga wybudzić wątek zatrzymany na wywołaniu `select()`
- #coderef(14, text: `dup()`), #coderef(15, text: `dup2()`) - funkcje te umożliwiają mi utrzymanie połączenia z faktycznym stdio procesu, jednocześnie podmieniając je na inny otwarty deskryptor

=== Segmenty pamięci dzielonej

W mojej symulacji za pamięć dzieloną odpowiada proces łodzi.

- #coderef(23, text: `shmget()`), #coderef(24, text: `shmat()`) - tworzy i łączy się do segmentu pamięci dzielonej łodzi
- #coderef(25, text: `shmdt()`), #coderef(26, text: `shmctl()`) - odłącza i usuwa pamięć dzieloną, w momencie zamknięcia procesu łodzi

=== Kolejki komunikatów

Na kolejkach komunikatów opiera się cały system sterowania symulacją przez dystrybutora. Z tego powodu wszystkie odwołania do
funkcji z nimi związanych zostały zamknięte w #coderef(27, text: `makrach`), które są potem wykorzystywane w każdym komponencie symulacji:

- #coderef(28, text: `MSGQUEUE_RECV_GLOBAL()`) - odbiera komunikaty skierowane globalnie (bez konkretnego procesu) - makro wywoływane w głównej pętli dyspozytora
- #coderef(29, text: `MSGQUEUE_SEND()`) - wysyła pakiet do kolejki komunikatów. Ponieważ typ wiadomości zapisany jest w samej jej strukturze, metoda ta działa zarówno do komunikatów bezpośrednich jak i dla wiadomości globalnych
- #coderef(30, text: `MSGQUEUE_RECV_C_DIRECT()`) - odbiera komunikat bezpośredni, skierowany do PIDu obecnie wykonywanej aplikacji

=== Gniazda

Na gniazdach został napisany serwer logów. Aplikacje klienckie (wszystkie programy wysyłające logi) łączą się do niego przy użyciu gnaizd typu SOCK_SEQPACKET,
dzięki czemu w ramach jednej aplikacji wszystkie wiadomości wysyłane są w odpowiedniej kolejności.

- #coderef(31, text: `socket()`), #coderef(32, text: `bind()`), #coderef(33, text: `listen()`), #coderef(34, text: `accept()`) - Funkcje wykorzystywane przez serwer logów, które pozwalają mu nasłuchiwać na połączenia przychodzące od aplikacji klienckich
- #coderef(35, text: `connect()`) - Funkcja ta umożliwia bibliotece klienckiej na połączenie z serwerem logów
