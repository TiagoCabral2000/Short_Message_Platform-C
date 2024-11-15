#ifndef TP_SO2425_ESTRUTURAS_H
#define TP_SO2425_ESTRUTURAS_H

typedef struct{
   int pid;
   char username[20];
} login;

typedef struct{
   char msg[50];
   int result;
} login_feedback;

typedef struct{
   int pid;
   char topico[20];
   int duracao;
   char mensagem[300];
} dataMSG;

typedef struct{
   int resultado;
   char mensagem[300];
}devolveMSG;


#endif