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
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
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
int findFileDir (const char* dirName, char** saveFile, int index);

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
    //puts(path);
    free(tmp);

    
    dataFile*array=malloc(sizeof(dataFile)*40);
    char** a = malloc(sizeof(char*)*50);
    for(int i=0; i<50; i++){
        a[i]=NULL;
    }
    int x=0;
    //leggiamo gli argomenti passati a riga di comando
    for(int i=1; i<argc; i++ ){
        if(strstr(argv[i],"file") && strstr(argv[i],".dat")){
            //cerco nella path corrente di lavoro
            strcpy(array[x].filename, argv[i]);
            strcpy(array[x].filepath,searchFile(path,argv[i]));
            ++x;
        }else{
            findFileDir(realpath(argv[i],NULL),a,0);
            int b=0;
            while(a[b]!=NULL){
                strcpy(array[x].filename,strchr(a[b],'/'));
                strcpy(array[x].filepath,a[b]);
                ++b;
                ++x;
            }
            for(int g=0; g<b; g++){
                a[g]=NULL;
            }
        }
    }


    for(int i=0; i<x; i++){
        puts(array[i].filepath);
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
                    return ret;                
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
        perror("stat");
        return 0;
    }
    return S_ISREG(path_stat.st_mode) && strstr(filePath,".dat");
}

int findFileDir (const char* dirName, char** saveFile, int index){
    char** dirTree=malloc(sizeof(char*)*100);
    int treeI=0;
    struct stat buf;
    if(stat(dirName,&buf)==-1){
        perror("facendo la stat");
        fprintf(stderr, "Errore nella directory %s\n", dirName);
        exit(EXIT_FAILURE);
    }
    DIR*d;
    //fprintf(stdout, "SONO NELLA DIRECTORY: %s\n", dirName);

    if((d=opendir(dirName))==NULL){
        perror("opendir");
        fprintf(stderr, "errore nella directory %s\n", dirName);
        exit(EXIT_FAILURE);
    }else{
        struct dirent * f;
        while ((errno=0, f=readdir(d))!=NULL){
            struct stat buf;
            char * filename =malloc(sizeof(char*)*255);
            memset(filename,'\0',255);
            strncpy(filename,dirName, 254);
            strncat(filename,"/", 254);
            strncat(filename,f->d_name,254);

            if(stat(filename,&buf)==-1){
                perror("eseguendo la stat");
                fprintf(stderr, "erre in %s", filename);
                exit(EXIT_FAILURE);
            }
            if(S_ISDIR(buf.st_mode)){
                if(strlen(filename)>0 && (filename[strlen(filename)-1]!='.')){
                    dirTree[treeI]=malloc(sizeof(char)*256);
                    strncpy(dirTree[treeI], filename, 254);
                    ++treeI;
                }
            }else{
                if(strstr(f->d_name,".dat")&& strstr(f->d_name,"file")){
                    //mi assicuro che il file sia giusto
                    //printf("FILE TROVATI: ");
                   //puts(filename);
                  // printf("INDEX:%d\n", index);
                    saveFile[index]=malloc(sizeof(char)*255);
                    //puts("allocato saveFIle");
                    memset(saveFile[index],'\0',255);
                    // puts(filename);
                    strncpy(saveFile[index], filename, 254);
                    ++index;
                    // puts(saveFile[index]);
                    
                }
            }
        }
        if(errno!=0){
            perror("readdir");
            fprintf(stderr, "readdir della cartella %s fallito", dirName);
            closedir(d);
            exit(EXIT_FAILURE);
        }
        for(int k=0; k<treeI; k++){
            findFileDir(dirTree[k], saveFile,index);
        }
    }


}