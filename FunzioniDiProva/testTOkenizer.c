#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int main (int argc, char** argv){
    char A[20];
    char B[20];
 
    for(int i=0;i<strlen(argv[1]); i++ ){
        A[i]=argv[1][i];
    }
    char* e=NULL;
    long n1=strtol(A,&e,0);
    e=NULL;
    for(int i=0;i<strlen(argv[2]); i++ ){
        B[i]=argv[2][i];
    }
    long n2=strtol(B,&e,0);

    if(n2>=n1){
        puts(argv[1]);
        puts(argv[2]);
    }else{
        puts(argv[2]);
        puts(argv[1]);       
    }

    
    return 0;
}