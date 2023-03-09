/**
 * @file main.c
 * @author Michela Deodati 597983
 * @brief file main
 * @date 15-03-2023
 * 
 */

#define _GNU_SOURCE
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <signal.h>



#include "./WorkerPool.h"
#include "./Collector.h"
#include "./MasterThread.h"

#define NTHREAD_DEFAULT 4
#define QUEUE_SIZE_DEFAULT 8
#define DELAY_DEFAULT 0


/**
 * @brief esegue il controllo dell'input in maniera quantitativa perchè poi il controllo vero e proprio
 * e l'assegnazione dei valori viene lasciata al master thread
 * 
 * @param argc dimensione di argv
 * @param argv parametri passati al main da riga di comando
 */
void InputCheck(int argc, string argv[]){
    if(argc<2){
         fprintf(stderr, "****FATAL ERROR****\nUsage %s: 'filename.....'\nOptions:\n\t-n\t<nthreads>\n\t-q\t<qsize>\n\t-d\t<directory name>\n\t-t\t<delay>\n", argv[0]);
         exit(EXIT_FAILURE);
    }
}

/**
 * @brief controlla il valore passato all'opzione -n e restituisce il valore corretto
 * 
 * @param nthread 
 * @return int valore di default se nthread <0, altrimenti nthread
 */
int checkNthread(const int nthread){
    if(nthread>0){
        return nthread;
    }else{
        return NTHREAD_DEFAULT;
    }
}

/**
 * @brief controlla il valore passato all'opzione -q e restituisce il valore corretto
 * 
 * @param qsize 
 * @return int valore di default se qsize <0, altrimenti qsize
 */
int checkqSize (const int qsize){
    //controllo che il valore della lunghezza della coda condivisa sia >0
    return((qsize>0)? qsize:QUEUE_SIZE_DEFAULT);
}

/**
 * @brief controlla il valore passato all'opzione -t e restituisce il valore corretto
 * 
 * @param time 
 * @return int valore di default se time è <0, altrimenti time
 */
int checkDelay (const int time){
    //controllo che il delay specificato sia >=0
    return((time>0)? time: DELAY_DEFAULT);
}

/**
 * @brief controlla che il file che gli viene passato sia un regular file 
 * e presenti le caratteristiche dei file che possono lggere i miei thread
 * 
 * @param filePath path del file
 * @return int 1 se è un file conforme 0 altrimenti
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
 * @brief salva nell'array di stringhe tutti i file che trova
 *  esplorando la cartella, l'esplorazione consiste in una BFS sull'albero delle sottocartelle della cartella passata
 * 
 * @param dirName nome della cartella da esplorare
 * @param saveFile string array
 * @param index posizione dello string array in cui scrivere
 */
void findFileDir(const string dirName, string* saveFile, int index)
{
    //alloco l'array di stringhe che conterrà poi tutti i nodi dell'abero delle directory
    string * dirTree = malloc(sizeof(string) * 100);
    int treeI = 0; //indice dell'array di stringhe
    struct stat buf;
    if (stat(dirName, &buf) == -1){
        perror("MAIN: facendo la stat() nella funzione findFileDir");
        fprintf(stderr, "Errore nella directory %s\n", dirName);
        exit(EXIT_FAILURE);
    }
    DIR *d;
    if ((d = opendir(dirName)) == NULL)
    {   perror("MAIN: facendo la opendir() nella funzione findFileDir");
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
            if (stat(filename, &buf) == -1){
                perror("MAIN: facendo la stat() nella funzione findFileDir");
                fprintf(stderr, "errore in %s", filename);
                exit(EXIT_FAILURE);
            }
            if (S_ISDIR(buf.st_mode))
            {
                if (strlen(filename) > 0 && (filename[strlen(filename) - 1] != '.'))
                {   
                    //se è una cartella e non è '.' o '..'
                    dirTree[treeI] = malloc(sizeof(char) * PATH_LEN);
                    strncpy(dirTree[treeI], filename, PATH_LEN);
                    ++treeI;
                }
            }
            else
            {
                if (isFile(filename))
                {
                    saveFile[index] = malloc(sizeof(char) * PATH_LEN);
                    memset(saveFile[index], '\0', PATH_LEN);
                    //salvo il file nell'array solo se è un file regolare e del tipo che i miei thread possono leggere
                    if(isFile(filename)){
                        strncpy(saveFile[index], filename, PATH_LEN);
                        ++index;
                    }else{
                        free(saveFile[index]);
                    }

                }
            }
        }
        if (errno != 0)
        {
            perror("MAIN: facendo la readdir() nella funzione findFileDir");
            fprintf(stderr, "readdir della cartella %s fallito", dirName);
            closedir(d);
            exit(EXIT_FAILURE);
        }
        for (int k = 0; k < treeI; k++)
        {
            //eseguo la funzione anche sulle altre sottocartelle trovate passando però l'indice corretto a cui scrivere
            findFileDir(dirTree[k], saveFile, index);
            free(dirTree[k]);
        }
        free(dirTree);
        closedir(d);
    }
}

int main (int argc, string argv[]){
    //contenuta in Util.h esegue il controllo del numero di argomenti passati al main
    InputCheck(argc, argv);
    //fileindex contiene la osizione dei file passati al main
    int fileIndex=1;
    int opt=0;
    int argvalue=0;


    int ntread=0;
    int qsize=0;
    int tdelay=0;
    //alloco dir solo se ne ho bisogno, cioè se viene passata l'opzione -d 
    string dir = NULL;

    while((opt = getopt(argc, argv, "n:q:d:t:"))!=-1){
        //switch su opt:
        switch (opt)
        {
        case 'n':
        fileIndex+=2;
            argvalue=StringToNumber(optarg);
            //printf("NUMERO DI THREAD: %d\n", argvalue);
            ntread=checkNthread(argvalue);
            break;
        case 'q':
            fileIndex+=2;
            argvalue=StringToNumber(optarg);
            //printf("LUNGHEZZA DELLA CODA: %d\n", argvalue);
            qsize=checkqSize(argvalue);
            break;
        case 't':
            fileIndex+=2;
            argvalue=StringToNumber(optarg);
            //printf("DELAY: %d\n", argvalue);
            tdelay=checkDelay(argvalue);
            break;
        case 'd':
            fileIndex+=2;
            //printf("DIRECTORY: %s\n", optarg);  
            dir=malloc(sizeof(char)*PATH_LEN);
            memset(dir, '\0',PATH_LEN);  
            strncpy(dir,optarg,strlen(optarg));    
        default:
            //non faccio nulla
            break;
        }
    }//end while optarg

    //array di stringhe che salva il risultato dell'esplorazione della cartella
    string * files=(string*)malloc(sizeof(string)*100);//allco 100 posti tanto poi faccio la realloc
    //esploro la cartella se è stata passata come argomento
    if(dir!=NULL){
        findFileDir(dir, files,0);
    }
    //conto quanti file ho trovato e quanti ce ne sono nel main 
    int numFile=0;
    while(files[numFile]!=NULL){
        numFile++;
    }//se non ho passato cartelle non entra nel while e numfile rimane 0
    //sommo per quanti file sono stati passati al main
    numFile=numFile+(argc-fileIndex);
    //rialloco la memoria ora che conosco il numero giusto di file
    files=realloc(files, sizeof(string)*numFile);
    //inserisco tutti i file nell'array files
    for(int index=0; index<numFile; index++){
        if(files[index]!=NULL){
            continue;
        }else{
            files[index]=(string)malloc(sizeof(char)*PATH_LEN);
            memset(files[index], '\0', PATH_LEN);
            strncpy(files[index], argv[fileIndex], strlen(argv[fileIndex]));
            fileIndex++;
        }
    }
    //chiamo la funzione che fa partire la threadpool, il collector e il signal handler
    runMasterThread(ntread,qsize,tdelay,numFile,files);
    //libero la memoria
    for(int i=0; i<numFile; i++){
        free(files[i]);
    }
    free(files);
    if(dir!=NULL){
        free(dir);
    }
    return 0;
}