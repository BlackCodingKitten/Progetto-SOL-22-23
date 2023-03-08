
/**
 * @file MasterThread.c
 * @author Michela Deodati 597983
 * @brief implementazione dell'intefraccia MasterThread
 * @version 0.1
 * @date 15-03-2023
 * 
 * 
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 2001112L
#endif
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <getopt.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <signal.h>

#include <Collector.h>
#include <WorkerPool.h>
#include "./MasterThread.h"


#define NTHREAD_DEFAULT 4
#define QUEUE_SIZE_DEFAULT 8
#define DELAY_DEFAULT 0



typedef struct s{
    int * stop;         //condizione di terminazione del while del masterthread;
    sigset_t set;     // set dei segnali da gestire mascherati
    int signal_pipe;    //descrittore di scrittura di una pipe senza nome usato per comunicare al collector che è terminata l'esecuzione
}sigHarg;

/**
 * @brief controlla il valore passato all'opzione -n
 * 
 * @param nthread 
 * @return int 
 */
int checkNthread(const int nthread){
    //controllo che il valore di nthread passato al main sia >0;
    if(nthread>0){
        return nthread;
    }else{
        return NTHREAD_DEFAULT;
    }
}

/**
 * @brief controlla il valore passato all'opzione -q
 * 
 * @param qsize 
 * @return int 
 */
int checkqSize (const int qsize){
    //controllo che il valore della lunghezza della coda condivisa sia >0
    return((qsize>0)? qsize:QUEUE_SIZE_DEFAULT);
}

/**
 * @brief controlla il valore passato all'opzione -t
 * 
 * @param time 
 * @return int 
 */
int checkDelay (const int time){
    //controllo che il delay specificato sia >=0
    return((time>0)? time: DELAY_DEFAULT);
}

/**
 * @brief controlla che il file che gli viene passato sia un regular file
 * 
 * @param filePath 
 * @return int 
 */
int isFile(const string filePath){
    struct stat path_stat;
    string tmp =malloc(sizeof(char)*strlen(filePath)+3);
    memset(tmp, '\0', strlen(filePath)+3);
    strcpy(tmp, "./");
    strcat(tmp, filePath);
    if(stat(tmp,&path_stat)!=0){
        perror("stat");
        return 0;
    }
    if(S_ISREG(path_stat.st_mode)){
        //controllo anche che sia il tipo di file corretto che i miei thread possono leggere
        if(strstr(filePath,".dat") && strstr(filePath, "file")){
            return 1;
        }
    }
    return 0;
}

/**
 * @brief task del thread signal handler che gestisce i segnali di terminazione e i segnali di stampa 
 * invia un messaggio sulla pipe passata come argomento per comunicare al collector la terminazione o 
 * che deve stampare
 * 
 * @param arg 
 * @return void* 
 */
static void* sigHandlerTask (void*arg){
    sigHarg sArg = *(sigHarg*)arg;

    while (true){
        int sig;
        if(sigwait(&(sArg.set),&sig)==-1){
            errno=EINVAL;
            perror("SIGNAL_HANDLER-Masterthread.c-123: errore sigwait");
            exit(EXIT_FAILURE);
        }
        switch (sig){
            case SIGINT:
            case SIGHUP:
            case SIGTERM:
            if(writen(sArg.signal_pipe,"t", 2)==-1){
                fprintf(stderr, "SIGNAL_HANDLER-Masterthread.c-131: errore writen sulla pipe");
            }
            close(sArg.signal_pipe); //notifico la ricezione del segnale al Collector;
            //cambio il vlore della guardia del while in masterthread
            *(sArg.stop)=1;
            return NULL;          
            case SIGUSR1:
                //invio al collector il segnale di stampa
                if(writen(sArg.signal_pipe, "s", 2)==-1){
                    fprintf(stderr, "SIGNAL_HANDLER-Masterthread.c-137: errore writen sulla pipe");
                }
        }
    }
    return NULL;

}


/**
 * @brief salva nell'array di stringhe tutti i file che trova
 *  esplorando la cartella
 * 
 * @param dirName nome della cartella da esplorare
 * @param saveFile string array
 * @param index posizione dello stringarray in cui scrivere
 */
void findFileDir(const char *dirName, char **saveFile, int index)
{
    string *dirTree = malloc(sizeof(string) * 100);
    int treeI = 0;
    struct stat buf;
    if (stat(dirName, &buf) == -1)
    {
        perror("facendo la stat");
        fprintf(stderr, "Errore nella directory %s\n", dirName);
        exit(EXIT_FAILURE);
    }
    DIR *d;
    // fprintf(stdout, "SONO NELLA DIRECTORY: %s\n", dirName);

    if ((d = opendir(dirName)) == NULL)
    {
        perror("opendir");
        fprintf(stderr, "errore nella directory %s\n", dirName);
        exit(EXIT_FAILURE);
    }
    else
    {
        struct dirent *f;
        while ((errno = 0, f = readdir(d)) != NULL)
        {
            struct stat buf;
            string filename = malloc(sizeof(string) * PATH_LEN);
            memset(filename, '\0', PATH_LEN);
            strncpy(filename, dirName, PATH_LEN);
            strcat(filename, "/");
            strcat(filename, f->d_name);
            if (stat(filename, &buf) == -1)
            {
                perror("eseguendo la stat");
                fprintf(stderr, "erre in %s", filename);
                exit(EXIT_FAILURE);
            }
            if (S_ISDIR(buf.st_mode))
            {
                if (strlen(filename) > 0 && (filename[strlen(filename) - 1] != '.'))
                {
                    dirTree[treeI] = malloc(sizeof(char) * PATH_LEN);
                    strncpy(dirTree[treeI], filename, PATH_LEN);
                    ++treeI;
                }
            }
            else
            {
                if (strstr(f->d_name, ".dat") && strstr(f->d_name, "file"))
                {
                    saveFile[index] = malloc(sizeof(char) * PATH_LEN);
                    memset(saveFile[index], '\0', PATH_LEN);
                    strncpy(saveFile[index], filename, PATH_LEN);
                    ++index;
                }
            }
        }
        if (errno != 0)
        {
            perror("readdir");
            fprintf(stderr, "readdir della cartella %s fallito", dirName);
            closedir(d);
            exit(EXIT_FAILURE);
        }
        for (int k = 0; k < treeI; k++)
        {
            findFileDir(dirTree[k], saveFile, index);
            free(dirTree[k]);
        }
        free(dirTree);
        closedir(d);
    }
}

/**
 * @brief core del masterthread che esegue tutte le funzioni richieste: controllo dell'input, creazione della workerpool
 * gestione dei segnali, fork per creare il processo figlio collector
 */
void runMasterThread(int argc,string argv[]){
    for(int i=0; i<argc; i++){
        printf("%s\n", argv[i]);
    }
    //creo la condizione del while per i segnali di stop:
    int stop=1;
    //associo alle variabili della threadpool i valori di default
    int n_nthread = 0;
    int q_queueLen= 0;
    int t_delay= 0;
    string d_directoryName = NULL;
    
    int fileIndex=1; //tiene traccia dell'indice di argv[] in cui si trovano i file
    int opt=0;
    opterr=0;
    int argvalue;
    printf("DEBUG Masterthread:prima di entrare il getopt\n");
    //getopt per controllare gli argomanti del main
    //inizio la scan degli argomenti del main
    while((opt=getopt(argc, argv, "n:q:d:t:"))!=-1){
        printf("DEBUG Masterthread: sono dentro getopt\n");
        //switch su opt:
        switch (opt)
        {
        case 'n':
        fileIndex+=2;
            printf("NUMERO DI THREAD: %s\n", optarg);
            argvalue=StringToNumber(optarg);
            n_nthread=checkNthread(argvalue);
            break;
        case 'q':
        fileIndex+=2;
            printf("LUNGHEZZA DELLA CODA: %s\n", optarg);
            argvalue=StringToNumber(optarg);
            q_queueLen=checkqSize(argvalue);
            break;
        case 't':
        fileIndex+=2;
            printf("DELAY: %s\n", optarg);
            argvalue=StringToNumber(optarg);
            t_delay=checkDelay(argvalue);
            break;
        case 'd':
        fileIndex+=2;
            printf("DIRECTORY: %s\n", optarg);        
            //dentro optarg ho il nome della cartella quindi:
            d_directoryName =(string)malloc(sizeof(char)*PATH_LEN);
            memset(d_directoryName, '\0', PATH_LEN);
            strcpy(d_directoryName,optarg);
        default:
            //printf("sono in default\n");
            //NON FACCIO NULLA 
            break;
        }
    }//end while optarg
    printf("DEBUG MasterThread: sono fuori da getopt\n");
    int x=0;
    string*tmp=NULL;
    if(d_directoryName!=NULL){
        printf("DEBUG Masterthread: non ho passato nessuna cartella\n");
        //alloco una stringa temporanea per salvare i file trovati nella cartella 
        tmp=(string*)malloc(sizeof(string)*20);
        for(int i=0; i<20; i++){
            tmp[i]=NULL;
        }
        //cerco e salvo i file nella cartella
        findFileDir(d_directoryName,tmp,0);
        
        for(;tmp[x]!=NULL;){
            //conto quanti file ci sono
            ++x;
        }
    }

    printf("DEBUG Masterthread: creo la pipe per comunicare con il collector tramite il signal handler\n");
    //creo la pipe che mi permette di segnalare al collector che sto terminando o per dirgli di stamapare
    int s_pipe[2];
    if(pipe(s_pipe)==-1){
        perror("pipe()");
        exit(EXIT_FAILURE);
    }
    //calcolo la dimensione del'array che conterrà i nomi dei file jsu cui fare i calcoli
    int dim=(argc-fileIndex)+x;
    //faccio partire il processo collector facendo la fork perchè adesso conosco quanti file ho il totale da calcolare
    pid_t process_id=fork();
    if(process_id==0){
        //fprint(stdout, "Sono il processo collector avviato dal MasterThread");
        //avvio il processo collector 
        runCollector(dim,s_pipe[0]);
    }else{
        if(process_id<0){
            fprintf(stderr, "Errore fatale riga 110 Masterthread.c la fork ha ritornato un id negativo process_id=%d\n", process_id);
            REMOVE_SOCKET();
            free(tmp);
            if(d_directoryName){
                free(d_directoryName);
            }
            exit(EXIT_FAILURE);
        }else{
            //process_id>0 //processo padre
            // gestisco il signal handler
            sigset_t mask;
            sigemptyset(&mask);
            sigaddset(&mask, SIGINT);
            sigaddset(&mask, SIGHUP);
            sigaddset(&mask, SIGTERM);
            sigaddset(&mask, SIGUSR1);
            if (pthread_sigmask(SIG_BLOCK, &mask, NULL) != 0)
            {
                fprintf(stderr, "fatal error pthread_sigmask\n");
                exit(EXIT_FAILURE);
            }
            pthread_t sigHandler;
            sigHarg argument = {&stop, mask, s_pipe[1]};
            if (pthread_create(&sigHandler, NULL, sigHandlerTask, (void *)&argument))
            {
                fprintf(stderr, "errore nella creazione del sighandler\n");
                exit(EXIT_FAILURE);
            }
            //creo l'array che contiene i nomi dei file da passare ai thread della threadpool
            string*argarray=malloc(sizeof(string)*dim);
            int i=0;
            //alloco i file che sono stati passati al main
            for(i=0; fileIndex<argc; i++){
                argarray[i]=malloc(sizeof(char)*PATH_LEN);
                memset(argarray[i],'\0', PATH_LEN);
                strcpy(argarray[i], argv[fileIndex]);
                ++fileIndex;
            }
            //inserisco anche i file che ho trovato nella cartella passata
            if(x!=0){
                for(int k=0;k<x;k++){
                    argarray[i]=malloc(sizeof(char)*PATH_LEN);
                    memset(argarray[i],'\0', PATH_LEN);
                    strcpy(argarray[i], tmp[k]);
                    free(tmp[i]);
                    ++i;
                }
                free(tmp);
                free(d_directoryName);
            }

            //libero lo spazio di tmp e dirName

            //ignoro il segnale sigpipe per evitare di essere terminato da una scrittura su socket
            struct sigaction s;
            memset(&s,0,sizeof(s));
            s.sa_handler=SIG_IGN;
            if((sigaction(SIGPIPE,&s,NULL))==-1){
                perror("sigaction()");
                for(int h=0; h<dim; h++){
                    free(argarray[h]);
                }free(argarray);
                exit(EXIT_FAILURE);
            }

            //creo la threadpool
            workerpool_t *wpool=NULL;
            
            //controllo che nella creazione della threadpool vada tutto bene
            if((wpool =createWorkerpool(n_nthread,q_queueLen))==NULL){
                fprintf(stderr, "ERRORE FATALE NELLA CREAZIONE DELLA THREADPOOL\n");
                for(int h=0; h<dim; h++){
                    free(argarray[h]);
                }free(argarray);
                exit(EXIT_FAILURE);
            }
            int index=0;
            //itero fino a che non termina con segnale o finno a che non ho mandato tutti i file 
            while(!stop && index<dim){
                int check=addToWorkerpool(wpool,leggieSomma,(void*)&argarray[index]);
                if(check==0){
                    //incremento l'indice solo se riesco ad assegnare correttamente la task alla threadpool
                    ++index;
                    sleep(t_delay);
                    continue;
                }else{
                    if(check==1){
                        //coda piena
                        //fprintf(stderr, "Coda delle task piena");
                        //non incremento l'indice e riprovo al giro successivo //non aspetto nemmeno i secondi di delay
                        continue;
                    }else{
                        fprintf(stderr, "FATAL ERROR IN THREADPOOL");
                        break;
                    }
                }
            }
            //distruggo la threadpool ma aspetto che siano completate le task pendenti
            destroyWorkerpool(wpool,false);
            //aspetto che termini il collector
            waitpid(process_id,NULL,0);
            //libero la memoria
            for(int y=0; y<dim; y++){
                free(argarray[i]);
            }free(argarray);
            unlink(SOCKET_NAME);
        }
    }//END Else della fork
    
}
