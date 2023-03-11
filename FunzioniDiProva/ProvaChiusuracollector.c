/*
in questo file implemento la possibile chiusura tramite segnali 
SIGHUP,SIGINT,SIGTERM, del processo collector testandone il funzionamento
in sostanza utilizzo una pipe che vine passata anche al signal_handelr_thread che mi 
serve per comunicare tra il Masterthread e il collector se sono stati ricevuti dei segnali
l'idea è quella di implementare il collector come un server che acetta connessioni tramite 
una select anche se in realtà ogni thread apre e chiude la propria sochet di comunicazione 
con il thread Collector
****
forse si potrebbe implementare  il masterthread in modo che stabilisca n_thread socket e
 ogni thread avrebbe la propria socket che poi verrebbero chiuse solo alla fine dal masterthread?
vedo se rieco a fare una versione dummy di questa cosa in questo.c di prova */

#define _POSIX_C_SOURCE 200112L
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/un.h>
#include <assert.h>
#include <signal.h>

typedef struct arg
{
    int *stop;
    sigset_t set;
    int signal_pipe;
} handler_arg;

void mT();
void cT(int signal_pipe);
static void *sigH(void *arg);

int main(void)
{
    mT();
    return 0;
}

void mT()
{
    int stop = 1;

    // ignoro sigpipe
    struct sigaction s;
    memset(&s, 0, sizeof(s));
    s.sa_handler = SIG_IGN;
    if ((sigaction(SIGPIPE, &s, NULL)) == -1)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    // creo la pipe usta come canale di comunicazione per notificare la terminazione
    int s_pipe[2];
    if (pipe(s_pipe) == -1)
    {
        perror("pipe()");
        exit(EXIT_FAILURE);
    }

    // eseguo la fork dei processi
    pid_t process_id = fork();
    if (process_id < 0)
    {
        puts("ERRORE process_id dopo la fork <0 ");
        exit(EXIT_FAILURE);
    }
    else
    {
        if (process_id == 0)
        {
            puts("FIGLIO: fork eseguita con successo adesso sono vivo");
            cT(s_pipe[0]);
        }
        else
        {   // avvio il signal handler
            puts("PADRE: creo il sig_handler dei segnali");
            sigset_t mask;
            sigemptyset(&mask);
            sigaddset(&mask, SIGINT);
            sigaddset(&mask, SIGTERM);
            sigaddset(&mask, SIGHUP);
            sigaddset(&mask, SIGQUIT);
            sigaddset(&mask, SIGUSR1);

            if (pthread_sigmask(SIG_BLOCK, &mask, NULL) != 0)
            {
                puts("PADRE: fatal error pthread_sigmask()");
                exit(EXIT_FAILURE);
            }
            pthread_t sig_handler_thread;
            // assegno gli argomenti del signal handler
            handler_arg arg = {&stop, mask, s_pipe[1]};

            printf("PADRE: prova segnale di stop: %d\n", stop);
            if (pthread_create(&sig_handler_thread, NULL, &sigH, (void *)&arg) != 0)
            {
                puts("error create sighandler");
                exit(EXIT_FAILURE);
            }
            // aspetto il processo figlio
            waitpid(process_id, NULL, 0);
            printf("PADRE: prova segnale di stop: %d\n", stop);
            //attestato che effettivamente cambia il segnale di stop
            puts("PADRE: il figlio è terminato posso terminare anche io");
            exit(EXIT_SUCCESS);
        }
    }
}

static void *sigH(void *arg)
{
    puts("SIG_HANDLER: avvio...");
    handler_arg args = *(handler_arg *)arg;

    int recived_sig;
    while (1)
    {
        if (sigwait(&(args.set), &recived_sig) != 0)
        {
            puts("Fatal error sigwait");
            exit(EXIT_FAILURE);
        }
        printf("SIG_HANDLER: ho ricevuto segnale %d\n", recived_sig);
        switch (recived_sig)
        {
        case SIGINT:
        case SIGTERM:
        case SIGHUP:
            *(args.stop) = 151100;
            puts("SIG_HANDLER: ho ricevuto un segnale di terminazione\nSIG_HANDLER: invio al child_process tramite la pipe la terminazione");
            write(args.signal_pipe, "t", 2);
            puts("SIG_HANDLER: chiudo la pipe");
            close(args.signal_pipe);
            pthread_exit(NULL);
        case SIGUSR1:
            puts("SIG_HANDLER: ricevuto segnale SIGUSR1\nSIG_HANDLER: invio al child_process tramite la pipe il segnale di stampa");
            write(args.signal_pipe, "s", 2);
        }
        //invio una lettera sola perchè c'è anche la possibilità che la select legga a pezzi il buffer che gli 
        //viene mandato quindi per evitare letture parziali invio 1 solo elemento
    }
}

void cT(int signal_pipe)
{
    struct sockaddr_un serv_addr;
    memset(&serv_addr, '\0', sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, "./sk_prova.sck", 108);
    int listen_socket;
    listen_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listen_socket == -1)
    {
        exit(EXIT_FAILURE);
    }
    if (bind(listen_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)))
    {
        perror("bind");
        close(listen_socket);
        unlink("./sk_prova.sck");
        exit(EXIT_FAILURE);
    }
    if (listen(listen_socket, SOMAXCONN) == -1)
    {
        perror("listen()");
        close(listen_socket);
        unlink("./sk_prova.sck");
        exit(EXIT_FAILURE);
    }
    puts("FIGLIO: sono in ascolto");
    // creo i set
    fd_set set;
    fd_set tmp_set;
    FD_ZERO(&set);
    FD_ZERO(&tmp_set);

    FD_SET(listen_socket, &set); // aggiungo la socket al master set
    FD_SET(signal_pipe, &set);   // aggiungo il descittore di lettura passato come argomento al collector

    // tengo treaccia del file descriptor con id più grande
    int fdmax = (listen_socket > signal_pipe) ? listen_socket : signal_pipe;
    bool loop = 1;
    while (loop)
    {
        // copio il set nella variabile temporanea per la select
        tmp_set = set;
        if (select(fdmax + 1, &tmp_set, NULL, NULL, NULL) == -1)
        {
            perror("select()");
            close(listen_socket);
            close(signal_pipe);
            unlink("./sk_prova.sck");
            exit(EXIT_FAILURE);
        }
        // adesso iteriamo per capire da quale file descriptor abbiamo ricevuto una richiesta
        for (int fd = 0; fd < (fdmax + 1); fd++)
        {
            if (FD_ISSET(fd, &tmp_set))
            {
                int fd_conn;
                if (fd == listen_socket)
                {
                    // nuova richiesta di connessione
                    fd_conn = accept(listen_socket, NULL, NULL);
                    if (fd_conn == -1)
                    {
                        perror("accept");
                        close(listen_socket);
                        unlink("./sk_prova.sck");
                        exit(EXIT_FAILURE);
                    }
                    //***FACCIO*COSE****//
                }
                if (fd == signal_pipe)
                {

                    char *buffer = malloc(sizeof(char) * 100);
                    read(fd, buffer, 100);
                    if (strcmp(buffer, "s") == 0)
                    {
                        printf("------Segnale di stampa ricevuto---------\n");
                    }
                    else
                    {
                        if (strcmp(buffer, "t") == 0)
                        {
                            printf("FIGLIO: chiudo la pipe e termino\n");
                            close(signal_pipe);
                            printf("FIGLIO: unlink della socket\n");
                            close(listen_socket);
                            unlink("sk_prova.sck");
                            execl("bin/rm","rm","-r","./","sk_prova.sck", NULL);
                            exit(0);
                        }
                    }
                }
            }
        }
    } // end while
}