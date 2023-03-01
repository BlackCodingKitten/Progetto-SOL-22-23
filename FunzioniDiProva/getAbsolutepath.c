#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

int is_regular_file(const char *path)
{
    struct stat path_stat;
    if (stat(path, &path_stat) != 0) {
        return 0;
    }
    return S_ISREG(path_stat.st_mode);
}

char *get_absolute_path(const char *filename) {
    char *absolute_path = NULL;
    char *cwd = NULL;

    if (filename == NULL) {
        return NULL;
    }

    cwd = (char *)malloc(PATH_MAX);
    if (cwd == NULL) {
        return NULL;
    }

    if (getcwd(cwd, PATH_MAX) == NULL) {
        free(cwd);
        return NULL;
    }

    absolute_path = (char *)malloc(PATH_MAX + strlen(filename) + 2);
    if (absolute_path == NULL) {
        free(cwd);
        return NULL;
    }

    snprintf(absolute_path, PATH_MAX + strlen(filename) + 2, "%s/%s", cwd, filename);


    free(cwd);

    if(!is_regular_file(absolute_path)){
        return "NOT REGULAR FILE";
    }
    return absolute_path;
}

int main (int argc,  char* argv[]){
    char** p =(char**)malloc( sizeof((argc-1)*sizeof(char*)));
    for(int i=1; i<argc; i++){
        p[i-1]=malloc(sizeof(char)*PATH_MAX);
        memset(p[i-1],'\0',PATH_MAX);
        char* tmp = get_absolute_path(argv[i]);
        if(tmp==NULL){
            fprintf(stderr, "errore fatale nella verifica del file\n");
            return EXIT_FAILURE;
        }
        if(strstr(tmp,"NOT REGULAR FILE")){
            strcpy(p[i-1], "NOT A REGULAR FILE: ");
            strcat(p[i-1], argv[i]);

            continue;
        }
        strncpy(p[i-1],tmp, strlen(tmp)+1);

    }
    puts("PATH DEI FILE:");

    for(int i=0; i<(argc-1); i++){
        puts(p[i]);
    }

    puts("FINE");
    return 0;
}