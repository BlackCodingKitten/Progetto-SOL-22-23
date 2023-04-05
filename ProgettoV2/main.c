/**
 * @file main.c
 * @author Michela deodati 597983
 * @brief main del progetto farm
 * @version 2.0
 * @date 20-03-2022
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/socket.h>
#include<sys/un.h>


#include "./Masterthread.h"
#include "./Util.h"

#define NTHREAD 4
#define QSIZE 8
#define TIME 0

/**
 * @brief controlla che sia stato passato un numero corretto di argomenti al main altrimenti esce
 * 
 * @param argc numero di argomenti del main
 */
void InputCheck(int argc);

/**
 * @brief controlla il valore passato all'opzione -n e restituisce il valore corretto
 * 
 * @param n valore letto da optarg
 * @return int valore di default se nthread <0, altrimenti nthread
 */
int checkN (int n);

/**
 * @brief controlla il valore passato all'opzione -q e restituisce il valore corretto
 * 
 * @param q dimensione letta da getopt 
 * @return int valore di default se qsize <0, altrimenti qsize
 */
int checkQ (int q);

/**
 * @brief controlla il valore passato all'opzione -t e restituisce il valore corretto
 * 
 * @param t valore letto da optarg
 * @return int valore di default se time è <0, altrimenti time
 */
int checkT (int t);


int main(int argc, string*argv){
    InputCheck(argc);
    //dopo aer controllato ceh siano stati effettivamente passati un numero valido di argomenti al mian blocco i segnali
    //creo il set di segnali mascherati che deve gestire il signal handler 
    sigset_t mask;
    if(sigemptyset(&mask)==-1){
        perror("main.c sigsetmask()");
        return EXIT_FAILURE;
    }

    if(sigaddset(&mask,SIGTERM)==-1){
        perror("main.c sigaddset(SIGTERM)");
        return EXIT_FAILURE;
    }
    if(sigaddset(&mask,SIGINT)==-1){
        perror("main.c sigaddset(SIGINT)");
        return EXIT_FAILURE;
    }
    if(sigaddset(&mask,SIGUSR1)==-1){
        perror("main.c sigaddset(SIGUSR1)");
        return EXIT_FAILURE;
    }
    if(sigaddset(&mask,SIGHUP)==-1){
        perror("main.c sigaddset(SIGHUP)");
        return EXIT_FAILURE;
    }
    //blocco i segnali e per gestirli con il signal handler
    if(pthread_sigmask(SIG_BLOCK, &mask,NULL)!=0){
        //non setta errno
        fprintf(stderr,"main.c pthread_sigmask(): fatal error.\n");
        return EXIT_FAILURE;
    }
    //ignoro SIGPIPE per non terminare quando si chiude una socket
    struct sigaction ignore_sigpipe;
    memset(&ignore_sigpipe, 0,sizeof(ignore_sigpipe));
    ignore_sigpipe.sa_handler=SIG_IGN;
    if(sigaction(SIGPIPE,&ignore_sigpipe,NULL)==-1){
        perror("main .c sigaction( )");
        return EXIT_FAILURE;
    }
    
    //una volta bloccati i segnali analizzo le opzioni passate da input con getopt
    int file_index=1; //contiene l'indice di dove getopt sposta i file mentrwe fa la scansione di argv

    //setto i valori inziali della threadpool al loro default
    int nthread=NTHREAD;
    int qsize=QSIZE;
    int tdelay=TIME;
    string dir_name=NULL;//alloco la directory solo se ne ho bisogno


    int opt=0;
    int argvalue=0;

    while((opt = getopt(argc,argv, "n:q:d:t:"))!=-1){
        switch (opt)
        {
        case 'n':
            file_index +=2;
            argvalue=StringToNumber(optarg);
            nthread=checkN(argvalue);
            break;
        case 'q':
            file_index +=2;
            argvalue=0;
            qsize=checkQ(argvalue);
            break;
        case 't':
            file_index +=2;
            argvalue=0;
            tdelay=checkT(argvalue);
            break;
        case 'd':
            file_index +=2;
            dir_name=(string)malloc(sizeof(char)*(strlen(optarg)+1));
            memset(dir_name,'\0',strlen(optarg)+1);
            strncpy(dir_name, optarg, strlen(optarg)+1);
            break;
        default:
            break;
        }
    }
    //creo la socket farm.sck che mette in comunicazione i workers com il collector
    int fd_conn =0;
    struct sockaddr_un addr;
    strncpy(addr.sun_path,SOCKET_NAME,UNIX_PATH_MAX);
    addr.sun_family=AF_UNIX;
    fd_conn=socket(AF_UNIX, SOCK_STREAM,0);
    if(fd_conn==-1){
        fprintf(stderr, "Errore nella creazione della Server_Socket\n");
        REMOVE_SOCKET();
        return EXIT_FAILURE;
    }
    //eseguo la bind della socket
    if(bind(fd_conn,(struct sockaddr*)&addr, sizeof(addr))!=0){
        //eseguo un tentativo per provare a rimuovere la socket se è già esistente 
        REMOVE_SOCKET();
        if(bind(fd_conn,(struct sockaddr*)&addr,sizeof(addr))!=0){
            perror("bind(farm.sck)");
            REMOVE_SOCKET();
            return EXIT_FAILURE;
        }
    }

    //chiamo run masterthread per creare la threadpool, esguire le task e gestire i segnali 
    int exit_value=0;
    exit_value=runMasterthread(nthread, qsize, tdelay, file_index, argv, argc, dir_name,  &mask, fd_conn);
    if(dir_name!=NULL){
        free(dir_name);
    }
    close(fd_conn);
    return exit_value; //return del valore di runMasterthread();
}

void InputCheck(int argc){
    if(argc<2){
        fprintf(stderr, "Errore,  passare piu' argomenti al main.\nUsage \"farm\":\t-n<nthreds> -q<sizequeue> -t<delaytask> -d<dirname>.\n");
        //esco se non sono stati passati argomenti al main 
        exit(EXIT_FAILURE);
    }
}

int checkN  (int n){
    return (n>0)? n:NTHREAD;
}

int checkQ  (int q){
    return (q>0)? q:QSIZE;
}

int checkT  (int t){
    return(t>0)? t:TIME;
}