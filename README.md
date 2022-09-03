# Progetto File System FAT per il corso di Sistemi Operativi A.A. 2021/2022
Questa è una libreria, più programmi ausiliari, scritti in c89, che gestisce un filesystem pseudo "FAT" salvato su un file.

## Libreria
Il file [FAT.c](https://github.com/edo9300/Simple-FAT/blob/master/FAT.c) contiene l'implementazione della libreria,
il file [FAT.h](https://github.com/edo9300/Simple-FAT/blob/master/FAT.h) contiene l'header da includere per l'utilizzo.

### Struttura file disco
Il file corrisponde ad una rappresentazione della memoria corrispondente allo struct ``Disk``
https://github.com/edo9300/Simple-FAT/blob/41e9b09ba58408120af307f95b5f1df0feed3f3a/FAT.c#L47-L51
Questo struct al suo interno contiene la tabella FAT come primo elemento
https://github.com/edo9300/Simple-FAT/blob/41e9b09ba58408120af307f95b5f1df0feed3f3a/FAT.c#L39-L41
una tabella delle directory come secondo elemento
https://github.com/edo9300/Simple-FAT/blob/41e9b09ba58408120af307f95b5f1df0feed3f3a/FAT.c#L43-L45
ed un array di blocchi che verranno poi utilizzati per salvare i contenuti dei file.

La struttura DirectoryEntry contiene tutti i dati necessari per localizzare un file o una cartella
https://github.com/edo9300/Simple-FAT/blob/41e9b09ba58408120af307f95b5f1df0feed3f3a/FAT.c#L25-L33
Per individuare le directory entry non utilizzate, avere un ``filename`` con primo carattere ``0`` significa che quella entry è disponibile.

La tabella FAT è strutturata come un array di elementi di 32 bit, se un elemento ha un valore di
https://github.com/edo9300/Simple-FAT/blob/41e9b09ba58408120af307f95b5f1df0feed3f3a/FAT.c#L20
vuol dire che quella entry non è associata ad un file, altrimenti quella entry contiene l'indice del
blocco successivo per il file associato.
Per indicare l'ultimo elemento di una entry,
https://github.com/edo9300/Simple-FAT/blob/41e9b09ba58408120af307f95b5f1df0feed3f3a/FAT.c#L21
viene utilizzato.

Le dimensioni delle strutture utilizzate sono modificabili cambiando le costanti rispettive nel file sorgente
https://github.com/edo9300/Simple-FAT/blob/41e9b09ba58408120af307f95b5f1df0feed3f3a/FAT.c#L11-L15
(per poter utilizzare al meglio i programmi di prova e gestire cartelle con tanti file o con file grandi
è consigliato aumentare questi valori)

### Funzioni implementate
Le funzioni disponibile con le rispettive descrizioni sono presenti in [FAT.h](https://github.com/edo9300/Simple-FAT/blob/master/FAT.h).

Sono state implementate le funzioni richieste dalla consegna
```
createFile
eraseFile
write
read
seek
createDir
eraseDir
changeDir
listDir
```


## Programmi ausiliari
[directory_copy.c](https://github.com/edo9300/Simple-FAT/blob/master/directory_copy.c) e
[directory_expand.c](https://github.com/edo9300/Simple-FAT/blob/master/directory_expand.c) sono 2 semplici programmi che rispettivamente:

* Legge una cartella e la salva in un file disco
* Legge un file disco e ne estrae i contenuti in una cartella

Il file [main.c](https://github.com/edo9300/Simple-FAT/blob/master/main.c) contiene dei semplici test per la libreria.

# Compilazione ed Esecuzione
Per compilare il tutto, eseguire ``make`` nella cartella.
Il programma ``fat_test`` crea un disco ed esegue varie operazioni su di esso per verificare il corretto funzionamento,
eseguirlo con ```./fat_test nome_disco```

Il programma ``directory_copy`` copia i contenuti di una cartella in un file di disco, si può utilizzare ad esempio con
```
./directory_copy ./ /tmp/file_disco
```
e ciò copierà i contenuti della cartella corrente nel file ``/tmp/file_disco``

N.B. con le impostazioni di default non riuscirà a copiare tutti i contenuti dato che ci saranno troppi elementi troppo grandi a causa della cartella ``.git``
con una configurazione con variabili di dimensioni maggiori, ad esempio
```
#define TOTAL_BLOCKS 32768
#define BLOCK_BUFFER_SIZE 1024
#define DIRECTORY_ENTRY_MAX_NAME 256
#define TOTAL_DIR_ENTRIES 4092
#define MAX_DIR_CHILDREN 255
```
non ci dovrebbero essere problemi.

Il programma ``directory_expand`` invece fa l'opposto, passato un file di disco, lo estrae in una cartella, utilizzando il file di prima, eseguendo
```
./directory_expand /tmp/file_disco out
```
verrà creata una cartella ``out`` con dentro tutti i file e le cartelle che erano state copiati nel disco.
