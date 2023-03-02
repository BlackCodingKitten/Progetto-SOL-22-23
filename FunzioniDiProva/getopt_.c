#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <getopt.h>

int main (int argc, char**argv){
    int fileIndex=1;
    int opt;
    char*arg=NULL;
    opterr=0;
    printf("inizio la scan degli argomenti.\n");
    while((opt = getopt(argc,argv,"n:q:t:d:"))!=-1){
        puts("stampa dopo la prima scansione?");
        for(int i=1; i<argc; i++){
            printf("%s\t", argv[i]);
        }printf("\n");
        switch (opt)
        {
        case 'n':
        fileIndex+=2;
            printf("NUMERO DI THREAD: %s\n", optarg);
            break;
        case 'q':
        fileIndex+=2;
            printf("LUNGHEZZA DELLA CODA: %s\n", optarg);
            break;
        case 't':
        fileIndex+=2;
            printf("DELAY: %s\n", optarg);
            break;
        case 'd':
        fileIndex+=2;
            printf("DIRECTORY: %s\n", optarg);        
        default:
            printf("sono in default\n");
            break;
        }
    }
    puts("stampa dopo l'uscita da get opt: \n");
    for(int i=1; i<argc; i++){
        printf("%s\t", argv[i]);
    }printf("\n");
    puts("stampo solo i file");
    for(fileIndex; fileIndex<argc; fileIndex++){
        printf("%s\t", argv[fileIndex]);
    }printf("\n");
    return 0;
}