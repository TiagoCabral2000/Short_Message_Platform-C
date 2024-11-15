#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include "estruturas.h"

#define MANAGER_FIFO "MANAGER_FIFO"
#define CLIENT_FIFO "CLIENT_FIFO%d"
char CLIENT_FIFO_FINAL[100];

typedef struct {
    int tipo; 
    union {
        login log;
        login_feedback log_feedback;
        dataMSG data_message;
        devolveMSG return_message;
    } content;
} structs_container;

typedef struct{
   int tipo, pid, resultado, duracao;
   char username[20], topico[20], mensagem[300], msg_devolucao[50]; 
}TUDOJUNTO;


int main(int argc, char *argv[]){
   if (argc != 2){
      printf("\nNumero de parametros invalido!\nSintaxe a utilizar: './feed username'\n");
      return 0;
   }
//--------------------------------------------------
   structs_container container;
   container.tipo = 1;
   container.content.log.pid = getpid();
   strcpy(container.content.log.username, argv[1]);
//--------------------------------------------------


   TUDOJUNTO contentor;
   contentor.tipo = 1;
   contentor.pid = getpid();
   strcpy(contentor.username, argv[1]);
   
   //PREENCHE CLIENT_FIFO
   sprintf(CLIENT_FIFO_FINAL, CLIENT_FIFO, contentor.pid);
   printf("\nFIFO: %s for client %d\n", CLIENT_FIFO_FINAL, contentor.pid);
   if(mkfifo(CLIENT_FIFO_FINAL, 0666) == -1){
      if (errno != EEXIST) {
         printf("Erro ao criar FIFO do cliente\n");
         return 1;
      }
   }

   //ABRE PIPE DO MANAGER PARA ESCRITA
   int fd_envia = open(MANAGER_FIFO, O_WRONLY);
   if (fd_envia == -1) {
      printf("Erro ao abrir o FIFO do servidor");
      return 1;
   }

   //ENVIA LOGIN PARA O MANAGER
   if (write(fd_envia, &contentor, sizeof(contentor)) == -1) {
      printf("Erro ao escrever no FIFO do servidor\n");
      close(fd_envia);
      unlink(CLIENT_FIFO_FINAL);
      return 2;
   }

   //CLIENTE ABRE O SEU NAMED PIPE PARA LEITURA
   int fd_recebe = open(CLIENT_FIFO_FINAL, O_RDONLY);
   if (fd_recebe == -1) {
      printf("Erro ao abrir o FIFO do cliente\n");
      return 0;
   }

   //RECEBE FEEDBACK DO MANAGER SOBRE A OPERAÇÃO DE LOGIN
   int size = read(fd_recebe, &contentor, sizeof(contentor));
   if (size > 0) {
      printf("%s", contentor.msg_devolucao); //Mostra mensagem devolvida pelo manager
      if(contentor.resultado == 0){ //Nao deu para registar
         close(fd_recebe); 
         close(fd_envia);  
         unlink(CLIENT_FIFO_FINAL);
         return 0;
      }
   } 
   else {
      printf("Erro ao ler a resposta do servidor\n");
   }



   do{
      printf("\n> ");
      if (scanf("%19s %d %[^\n]s", contentor.topico, &contentor.duracao, contentor.mensagem) == 3) {
         contentor.tipo = 2;
         if (write(fd_envia, &contentor, sizeof(contentor)) == -1) {
            printf("Erro ao escrever no FIFO do servidor\n");
            close(fd_envia);
            unlink(CLIENT_FIFO_FINAL);
            return 2;
         }
      } 
      else {
         printf("Erro ao inserir dados. Tente novamente.\n");
      }

      int size = read(fd_recebe, &contentor, sizeof(contentor));
         if (size > 0) {
            if (contentor.tipo == 2){
               printf("\nMensagem recebida -> %s\n", contentor.mensagem);
               if(contentor.resultado == 0){
                  close(fd_recebe); 
                  close(fd_envia);  
                  unlink(CLIENT_FIFO_FINAL);
                  return 0;
               }
            } 
         }
}while(1);









   
   close(fd_recebe); 
   close(fd_envia);  
   unlink(CLIENT_FIFO_FINAL);  
   return 0;
}

