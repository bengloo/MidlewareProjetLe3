#ifndef __REQREP_H__
#define __REQREP_H__
#include "basic_func.h"
#define FMT_REQ "%3hd:%hd:%[^n]"
#define MAX_BUFF 1024
#define OK 1;
#define NOP 0;
typedef char buffer_t[MAX_BUFF];
//typedef char* buffer_t;

typedef enum{
    //Client Maitre vers Serveur
    CREERPARTIE = 1,

    //Client Adversaire vers Serveur
    LISTERPARTIE = 20,

    //Client Adversaire vers Client Maitre
    //JOIN = 30,

    //START = 40

} idRequeteServeur_t;


typedef struct 
{
    idRequeteServeur_t idReq;//0 à 999
    short lgreq;//0 à 1024
    buffer_t msgReq;
} req_t;

typedef struct 
{
    short idRep;//0 à 999
    short lgrep;//0 à 1024
    buffer_t msgRep;
} rep_t;
#endif