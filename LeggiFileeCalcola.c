#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>




typedef char* string;
typedef struct data{
    long value;
    string path;
}Data;

long calcolaFile(FILE *file);
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
        fileArray[i]=fopen(argv[i+1], "r");
        if(fileArray[i]==NULL){
            perror("fopen()");
            return EXIT_FAILURE;
        }
     }
    
    Data resArray[argc-1];
    for(int i=0;i<(argc-1);i++){
        resArray[i].path=malloc(sizeof(char)*(strlen(argv[i+1])+1));
        strcat(resArray[i].path, argv[i+1]);
        resArray[i].value=calcolaFile(fileArray[i]);
    }
    qsort(resArray, (argc-1), sizeof(Data), compare);
    for(int x=0; x<(argc-1); x++){
        fprintf(stdout, "%ld\t%s\n", resArray[x].value,resArray[x].path);
    }
    for(int y=0; y<(argc-1);y++){
        fclose(fileArray[y]);
    }
    if(fileArray!=NULL){
        free(fileArray);
    }
    return 0;
}

long calcolaFile(FILE*file){
    long value [100];
    for(int i=0; i<100; i++){
        value[i]=-1;
    }
    string s = malloc(sizeof(char)*100);
    string e=NULL;
    int i=0;
    while(fgets(s,100,file)!=NULL){
        e=NULL;
        string temp =malloc(sizeof(char)*(strlen(s)+1));
        if(strstr(s,"\n")){
            strncpy(temp,s,strlen(s)-1);
        }
        value[i]=strtol(temp,&e,0);
        if(*e==(char)0 && e!=NULL){
            value[i]=i*value[i];
        }else{
            fprintf(stderr, "ERRORE nel file");
            exit(EXIT_FAILURE);
        }
        i++;
        free(temp);
        memset(s,'\0', 100);
    }

    long somma = 0;
    i=0;
    while(value[i]!=-1){
        somma=somma+value[i];
        i++;
    }
   if(s!=NULL){
    free(s);
   }
   return somma;
}

int compare (const void* a, const void* b){
    return((*(Data*)a).value - (*(Data*)b).value);
}