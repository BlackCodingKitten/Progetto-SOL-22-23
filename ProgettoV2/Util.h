#if !defined (_UTIL_H_)
#define _UTIL_H_

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <ctype.h>
 #include <unistd.h>
 #include <errno.h>

#define SOCKET_NAME "farm.sck"
#define BUFFER_SIZE 245
#define UNIX_PATH_MAX 108

typedef char* string;

#define REMOVE_SOCKET()\
   if(unlink(SOCKET_NAME)==0){\
        \
   }else{\
    if(errno==ENOENT){\
        \
    }else{\
        perror("unlink(farm.sck)");\
    }\
   }\

/**
 * @brief controlla che la stringa passata sia un numero valido
 * @return -1 in caso di fallimento, il valore munerico di s se ha successo
 */
static inline int StringToNumber (const string s){
    if(s==NULL){
        return -1;
    }
    if(strlen(s)==0){
        return -1;
    }
    string e=NULL;
    int val =(int)strtol(s,&e,0);
    if(e!=NULL && *e==(char)0){
        return val; //successo
    }
    return -1; //fallimento
}

static inline ssize_t readn(int fd, void *ptr, size_t n) {  
   size_t   nleft;
   ssize_t  nread;
 
   nleft = n;
   while (nleft > 0) {
     if((nread = read(fd, ptr, nleft)) < 0) {
        if (nleft == n) return -1; /* error, return -1 */
        else break; /* error, return amount read so far */
     } else if (nread == 0) break; /* EOF */
     nleft -= nread;
     ptr   += nread;
   }
   return(n - nleft); /* return >= 0 */
}

static inline ssize_t writen(int fd, void *ptr, size_t n) {  
   size_t   nleft;
   ssize_t  nwritten;
 
   nleft = n;
   while (nleft > 0) {
     if((nwritten = write(fd, ptr, nleft)) < 0) {
        if (nleft == n) return -1; /* error, return -1 */
        else break; /* error, return amount written so far */
     } else if (nwritten == 0) break; 
     nleft -= nwritten;
     ptr   += nwritten;
   }
   return(n - nleft); /* return >= 0 */
}

#endif /*_UTIL_H_*/