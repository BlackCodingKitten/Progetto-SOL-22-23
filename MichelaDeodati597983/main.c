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
    string tmp = malloc(sizeof(char)*strlen(filePath)+3);
    memset(tmp, '\0', strlen(filePath)+3);
    strcpy(tmp, "./");
    strcat(tmp, filePath);
    if(stat(tmp,&path_stat)!=0){
        perror("stat");
        return 0;
    }
    //controllo che sia effettivamente un file
    if(S_ISREG(path_stat.st_mode)){
        free(tmp);
        return 1;
    }
    free(tmp);
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
            strcpy(filename, dirName);
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
                    strcpy(dirTree[treeI], filename);
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
            free(filename);
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
    //esegue il controllo del numero di argomenti passati al main in maniera quantitativa
    InputCheck(argc, argv);

    //fileindex contiene la posizione dei file passati al main
    int fileIndex=1;
    int opt=0;
    int argvalue=0;
    //setto i valori iniziali della threadpool, così se non sono passati altri valori da linea di comando valgono i valori di default
    int ntread=NTHREAD_DEFAULT;
    int qsize=QUEUE_SIZE_DEFAULT;
    int tdelay=DELAY_DEFAULT;
    //alloco dir solo se ne ho bisogno, cioè se viene passata l'opzione -d 
    string dir = NULL;

    while((opt = getopt(argc, argv, "n:q:d:t:"))!=-1){
        //switch su opt:
        switch (opt)
        {
        case 'n':
        fileIndex+=2;
            //string to number è contenuta in Util.h ed è la funzione che esegue la strtol() con controlli 
            argvalue=StringToNumber(optarg);
            ntread=checkNthread(argvalue);
            break;
        case 'q':
            fileIndex+=2;
            argvalue=StringToNumber(optarg);
            qsize=checkqSize(argvalue);
            break;
        case 't':
            fileIndex+=2;
            argvalue=StringToNumber(optarg);
            tdelay=checkDelay(argvalue);
            break;
        case 'd':
            fileIndex+=2;  
            dir=malloc(sizeof(char)*PATH_LEN);
            memset(dir, '\0',PATH_LEN);  
            strcpy(dir,optarg);    
        default:
            //non faccio nulla
            break;
        }
    }//end while optarg

    //array di stringhe che salva il risultato dell'esplorazione della cartella
    string * files=(string*)malloc(sizeof(string)*100);//allco 100 posti tanto poi faccio la realloc
    for(int s=0; s<100; s++){
        files[s]=NULL;
    }
    //esploro la cartella se è stata passata come argomento
    if(dir!=NULL){
        findFileDir(dir, files,0);
    }
    //conto quanti file ho trovato
    int numFile=0;
    while(files[numFile]!=NULL){
        numFile++;
    }//se non ho passato cartelle con l'opzione -d non entra nel while e numfile rimane 0
    //sommo per quanti file sono stati passati al main
    numFile=numFile+(argc-fileIndex);
    //rialloco la memoria ora che conosco il numero giusto di file
    files=realloc(files, sizeof(string)*numFile);
    //inserisco tutti i file passati al main nell'array files, tramite fileindex conosco esattamente a che posizione di argv[] si trovano
    for(int index=0; index<numFile; index++){
        if(files[index]!=NULL){
            continue;
        }else{
            files[index]=(string)malloc(sizeof(char)*PATH_LEN);
            memset(files[index], '\0', PATH_LEN);
            strcpy(files[index], argv[fileIndex]);
            fileIndex++;
        }
    }

    //chiamo la funzione che fa partire la threadpool, fa la fork e invoca il collector e crea il thread signal handler
    int ext = runMasterThread(ntread,qsize,tdelay,numFile,files);
    //libero la memoria
    for(int i=0; i<numFile; i++){
        free(files[i]);
    }
    free(files);
    if(dir!=NULL){
        free(dir);
    }
    //esco
    printf("ritorno %d\n", ext);
    return ext;
}