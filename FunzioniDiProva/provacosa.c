#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>



typedef struct dataFile{
    char filename[255];
    char filepath[255];
}dataFile;

char* searchFile (char* dirname, char* filename);
int isFile(const char* filePath);

int main (int argc, char*argv[]){
    //ricavo il percorso corrente del progetto
    char* path=malloc(sizeof(char)*PATH_MAX);
    getcwd(path, PATH_MAX);
    //calcolo la dimensione della stringa senza l'ultima cartella 
    int n=strlen(path)-strlen(strrchr(path,'/'));
    char* tmp=malloc(sizeof(char)*PATH_MAX);
    strcpy(tmp,path);
    memset(path,'\0',strlen(path));
    strncpy(path,tmp,n);
    puts(path);
    free(tmp);
    //adesso in path ho salvato la cartella in cui devo cercare i file per ricavarne la path
    dataFile array[argc-1];
    for(int i=1; i<argc; i++){
        strcpy(array[i-1].filename, argv[i]);
        strcpy(array[i-1].filepath,searchFile(path,argv[i]));
    }
    //stampa
    for(int i=0; i<argc-1; i++){
        printf("%s\t%s\n", array[i].filename, array[i].filepath);
    }

    return 0;
}

char* searchFile (char* dirname, char* filename){
    //apro la cartella chiamata dirname 
    DIR* directory;
    if((directory=opendir(dirname))==NULL){
        perror("Impossibile trovare il percorso del file");
        exit(EXIT_FAILURE);
    }
    //esploro la cartella 
    struct dirent * dirptr;
    while(errno=0, (dirptr=readdir(directory))!=NULL){
        //escludo ./ e ../
        if(dirptr->d_name[0]=='.'){
            continue;
        }
        char* path =(char*)malloc(sizeof(char)*PATH_MAX);
        strcpy(path,dirname);
        strcat(path,"/");
        strcat(path, dirptr->d_name);

        struct stat infodir;
        if(stat(path,&infodir)==-1){
            perror(path);
            exit(EXIT_FAILURE);
        }
        if(strcmp(dirptr->d_name,filename)==0){
            return path;
        }
        if(S_ISDIR(infodir.st_mode)){
            char* ret=malloc(sizeof(char)*255);
            if((ret = searchFile(path,filename))!=NULL){
                if(isFile(ret)){
                    return ret;
                }else{
                    return NULL;
                }
                
            }else{
                free(path);
                continue;
            }
        }
    }
    closedir(directory);
    return NULL;
    
}

int isFile(const char* filePath){
    struct stat path_stat;
    if(stat(filePath,&path_stat)!=0){
        return 0;
    }
    return S_ISREG(path_stat.st_mode);
}