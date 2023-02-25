#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#define BUFFERSIZE 1024



typedef char* string;
typedef struct data{
    long value;
    string path;
}Data;

long getFileSize(FILE *file);
long calcolaFile(FILE *file,string path);
int compare(const void*a, const void* b);

int main (int argc,string argv[]){
    if(argc<2){
        fprintf(stderr, "Usage %s: 'file-path'...", argv[0]);
        return EXIT_FAILURE;
    }
    //inizializzo l'array di file che leggo da input
    FILE**fileArray;
    fileArray=(FILE**)malloc(sizeof(FILE*)*(argc-1));

     //apro i file;
     for(int i=0; i< (argc-1); i++){
        fileArray[i]=fopen(argv[i+1], "rb");
        if(fileArray[i]==NULL){
            perror("fopen()");
            return EXIT_FAILURE;
        }
     }
    
    Data resArray[argc-1];
    for(int i=0;i<(argc-1);i++){
        resArray[i].path=malloc(sizeof(char)*(strlen(argv[i+1])+1));
        strcat(resArray[i].path, argv[i+1]);
        resArray[i].value=calcolaFile(fileArray[i],argv[i+1]);
    }
    qsort(resArray, (argc-1), sizeof(Data), compare);
    for(int x=0; x<(argc-1); x++){
         fprintf(stdout, "%ld\t%s\n", resArray[x].value,resArray[x].path);
    }
    for(int y=0; y<(argc-1);y++){
        fclose(fileArray[y]);
    }
    return 0;
}

long calcolaFile(FILE*file,string path){
    long somma=0;
    string buffer = malloc(sizeof(char)*BUFFERSIZE);
    long dim = getFileSize(file);
    //printf("file size %ld \n",dim);
    dim = dim / sizeof(long);
    //printf("long in file  %ld \n",dim);
    //alloco l'array in cui salvare i valori:
    long* fileLong = (long*)malloc(dim*sizeof(long));
    //leggiamo i valori nel file
    fread(fileLong,sizeof(long),dim,file);

    for(int i=0; i<dim; i++){
        somma=somma+(fileLong[i]*i);
    }
    return somma;
}

int compare (const void* a, const void* b){
    return((*(Data*)a).value - (*(Data*)b).value);
}

long getFileSize(FILE *file) {
    long size;

    fseek(file, 0, SEEK_END);  // si posiziona alla fine del file
    size = ftell(file);        // legge la posizione attuale nel file, che corrisponde alla sua dimensione in byte
    fseek(file, 0, SEEK_SET);  // si riporta all'inizio del file

    return size;
}