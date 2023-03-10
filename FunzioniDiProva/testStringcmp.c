#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main (int argc, char**argv){
    if(strcmp(argv[1],argv[2])==0){
        puts("le due stringhe sono uguali");
    }else{
        if(strcmp(argv[1], argv[2])==-1){
            printf("%s è più grande di %s\n", argv[2],argv[1]);
        }else{
            printf("%s è più grande di %s\n", argv[1],argv[2]);
        }
    }
    return 0;
}