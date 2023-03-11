#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 2001112L
#endif
/**
 * @file Util.h
 * @author Michela Deodati 597983
 * @brief contiene funzioni, define e typedef di uso generale
 * @date 15-03-2023
 * 
 */
#if !defined(_UTIL_H)
#define _UTIL_H

#include <sys/socket.h>
#include <stdarg.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>

typedef char* string;

#if !defined(FILE_BUFFER_SIZE)
//il LONG_MAX=9,223,372,036,854,775,807 che sono 19 cifre + \0 = 20
#define FILE_BUFFER_SIZE 20
#endif

#if !defined(SOCKET_NAME)
//definisco il nome ella socket usata per la comunicazione tra collector e workers
#define SOCKET_NAME "./farm.sck"
#endif

#if !defined(UNIX_PATH_MAX)
#define UNIX_PATH_MAX 108
#endif

#if !defined(PATH_LEN)
#define PATH_LEN 255
#endif

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

#define CLOSE_SOCKET(fd_socket)\
    if(close(fd_socket)!=0){\
        perror("close()");\
    } \

/**
 * @function readn
 * @brief evita letture parziali
 * 
 * @param fileDescriptor da dove leggo
 * @param buffer 
 * @param bufferSize 
 * @return int -1 errore(setta errno), 0 durante la lettura leggo EOF, buffersize se la lettura termina con successo
 */
static inline int readn(long fileDescriptor, void*buffer, size_t bufferSize){
    size_t remain = bufferSize;
    int byteRead = 0;
    string buffPtr = (string)buffer;
    while (remain > 0){
        if((byteRead=read((int)fileDescriptor, buffPtr,remain))==-1){
            if(errno=EINTR){
                //nessun errore, è stato lanciato un segnale mentre la system call era in corso
                continue;
            }else{
                //ogni altro errno è errore
                return -1;
            }
        }
        if(byteRead==0){
            //EndOfFile (EOF)
            return 0;
        }
        //decremento i byte rimanenti da leggere
        remain -= byteRead;
        buffPtr += byteRead;
    }
    return byteRead;
}

/**
 * @brief assicurarsi di scrivere tutti i byte
 * 
 * @param fileDescriptor dove scrivo
 * @param buffer cosa scrivo
 * @param bufferSize dimensione di cosa scrivo
 * @return int -1 errore(settato errno), 0 se non ci sono byte da scrivere,il numero di byte scritti se viene eseguito tutto senza errori
 */
static inline int writen(long fileDescriptor, void*buffer, size_t bufferSize){
    size_t remain= bufferSize;
    string bufferPtr = (string)buffer;
    int writtenByte=0;
    while (remain >0){
        if((writtenByte=write((int)fileDescriptor,bufferPtr,remain))==-1){
            if(errno=EINTR){
                continue;
            }else{
                return -1;
            }
        }
        if(writtenByte==0){
            return 0;
        }
        remain -= writtenByte;
        bufferPtr += writtenByte;
    }
    return writtenByte;
}

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
#endif /*_UTIL_H*/

