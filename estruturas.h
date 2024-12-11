#ifndef TP_SO2425_ESTRUTURAS_H
#define TP_SO2425_ESTRUTURAS_H

typedef struct {
    int tipo; 
} IDENTIFICADOR;

typedef struct {
    int pid;
    char username[20];
} LOGIN;

typedef struct {
    int resultado;
    char msg_devolucao[50];
} FEEDBACK;

typedef struct {
    int duracao, pid;
    char topico[20], username[20], mensagem[300];
} MSG;

typedef struct{
   char topico[20], username[20];
   int pid;
} SUBSCRIBE;

#endif