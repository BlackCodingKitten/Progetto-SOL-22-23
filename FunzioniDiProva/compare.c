#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef char* string;

int compareString(const void * a, const void* b);
int compareNumber(const void*a, const void* b);


int main (int argc, string argv[]){
    
    puts("Stampa in ordine di input");
    for(int i=0; i<argc;i++){
        puts(argv[i]);
    }
    string arg[(argc-1)];
    for(int i=1; i<argc; i++){
        arg[i-1]=argv[i];
    }
    printf("\n");
    fflush(stdout);
    qsort(arg,(argc-1),sizeof(string),compareString );
    puts("Ordinamento in ordine lessicografico");
    for(int i=0; i<argc-1;i++){
        puts(arg[i]);
    }
    printf("\n");
    fflush(stdout);
    qsort(arg, (argc-1),sizeof(string),compareNumber);
    puts("Ordinamento in ordine Numerico crescente");
    for(int i=0; i<argc-1;i++){
        puts(arg[i]);
    }  
    
    return 0;
}


int compareString(const void* a, const void*b){
    return strcmp(*(string*)a, *(string*)b);
}

int compareNumber(const void*a, const void* b){
     string* A= *( string*)a;
     string* B= *( string*)b;
    string e=NULL;
    long nA=strtol(A,&e,0);
    e=NULL;
    long nB=strtol(B,&e,0);
    printf("Comparo %ld con %ld\n", nA,nB);
    return nA>nB;

}