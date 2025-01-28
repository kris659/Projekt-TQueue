---
title: Publish-subscribe
subtitle: Programowanie systemowe i współbieżne
author: Krzysztof Pędzich 160416 \<<krzysztof.pedzich@student.put.poznan.pl>\>
date: v1.2, 2025-01-29
lang: pl-PL
---

Projekt jest dostępny w repozytorium pod adresem:
<https://github.com/kris659/Projekt-TQueue>. 

# Struktury danych

1. Struktura subskrybenta `Subscriber`:

	```C
	struct  Subscriber{
		pthread_t  key; // Identyfikator subskrybenta
		int  messageToReadIndex;
		int  messagesCount;
		Subscriber*  next; // Wskaźnik na kolejnego subskrybenta
	};
	```

2. Struktura kolejki `TQueue`:
 

	```C
	struct  TQueue {
		int  maxSize;
		int  firstMessage;
		int  newMessageIndex;
	
		Subscriber*  firstSub;
		int  subCount;
		
		void**  messages;
		int*  notReceivedCount;
 	
		pthread_mutex_t  mx;	
		pthread_cond_t  cond_read; // usypianie/budzenie wątków czytających
		pthread_cond_t  cond_write; // usypianie/budzenie wątków piszących
	};
	``` 

1. Zmienna `int  firstMessage` indeks najstarszej wiadomości (wiadomości odczytane są usuwane dopiero przy braku miejsca na nową). Wartość `-1` oznacza brak wiadomości.
2. Zmienna `int  newMessageIndex` indeks na którym będzie zapisana kolejna wiadomość. Jeżeli `firstMessage == newMessageIndex` oznacza to, że kolejka jest pełna.

3. Zmienna `Subscriber*  firstSub;` to wskaźnik na najstarszego subskrybenta, można przeiterować się po wszystkich subskrybentach używając `firstSub->next`.

4.  Tablica `void** messages` zawiera wskaźniki do wiadomości.

5.  Tablica `int*  notReceivedCount` zawiera informacje o tym ile subskrybentów nie odczytało jeszcze danej wiadomości.

  

# Funkcje  

1.  `Subscriber*  getSubscriber(TQueue  *queue, pthread_t  thread)` -- wyszukuje i zwraca dane subskrybenta na podstawie `pthread_t` które jest traktowane jako klucz.  

1.  `int  mod(TQueue  *queue, int  x)` -- zwraca dodatnią wartość `x` modulo wielkość kolejki (operator `%` zwraca również wartości ujemne)
  
  

# Algorytm / dodatkowy opis

 Wiadomości do odczytania są przechowywane w jednej tablicy, wykorzystujemy ją jak bufor cykliczny. Mamy indeks pierwszej wiadomości i indeks wskazujący na miejsce na którym będzie zapisana kolejna wiadomość.
 
 ## Subskrybent
 W momencie subskrybowania, subskrybent ma ustawianą wartość zmiennej:
 `sub.messageToRead = queue->newMessageIndex` 
 
 Gdy jest dodana nowa wiadomość zwiększana jest wartość zmiennej każdego subkskrybenta:
 `sub.messagesCount++` 
 
 Gdy subskrybent odczytuje wiadomość zmienia wartości:
 ```C
queue->notReceivedCount[messageToRead]--
sub->messageToReadIndex++
sub->messagesCount--
 ```
 
 Jeżeli wartość `messageCount` jest równa `0` przy odczytywaniu to wątek usypia do momentu dodania nowej wiadomości

## Pisarz

Kolejka jest pusta jeżeli `queue.firstMessage == -1`.

Jeżeli `queue.firstMessage == queue.newMessageIndex` oznacza to, że należy sprawdzić czy najstarsza wiadomość została już odczytana przez wszystkich. Jeśli tak, to można zwiększyć `queue.firstMessage++` .  Jeśli nie to wątek usypia do momentu odczytania wiadomości, zwiększenia wielkości kolejki lub odsubskrybowania.


## Odporność algorytmu na typowe problemy przetwarzania współbieżnego

* Zakleszczenie - mutexy są zawsze odblokowywane przed końcem funkcji oraz przy oczekiwaniu na wiadomość/miejsce na wiadomość
* Aktywne oczekiwanie - pisarz i czytelnik usypiają `pthread_cond_wait()` gdy nie mogą wykonać swojej akcji
* Głodzenie -  używany jest jeden mutex, więc za sprawiedliwe przydzielanie czasu jest odpowiedzialny procesor 
  
  

# Przykład użycia

Plik `main.c` zawiera prosty program sprawdzający działanie przy jednym wątku piszącym i dwóch wątkach czytających z różnymi szybkościami (wątek `0` czyta dwa razy szybciej)

Początkowo kolejka szybko się wypełnia (pojemność 4):

![Wypełnianie kolejki](https://i.postimg.cc/rw2ds4FW/Zrzut-ekranu-2025-01-25-235327.png)

Następnie szybszy wątek odczytuje wiadomości szybciej od drugiego:

![Odczytywanie wiadomości w różnym tempie](https://i.postimg.cc/GmP4ypHf/Zrzut-ekranu-2025-01-25-235634.png)

Do momentu gdy odczytał już wszystkie i również musi czekać na odczytanie wiadomości przez wolniejszy wątek:

![Odczytywanie wiadomości w tym samym tempie](https://i.postimg.cc/43JmbTcx/Zrzut-ekranu-2025-01-25-235653.png)
  

-------------------------------------------------------------------------------

