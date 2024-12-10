#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <sys/select.h>
#include "estruturas.h"

#define MANAGER_FIFO "MANAGER_FIFO"
#define CLIENT_FIFO "CLIENT_FIFO%d"
char CLIENT_FIFO_FINAL[100];

void handler_sigalrm(int s) {
   unlink(CLIENT_FIFO_FINAL);
   printf("\nCliente encerrado\n");
   exit(1);
}

typedef struct{
   int tipo, pid, resultado, duracao;
   char username[20], topico[20], mensagem[300], msg_devolucao[50]; 
}TUDOJUNTO;

int main(int argc, char *argv[]){
   if (argc != 2){
      printf("\nNumero de parametros invalido!\nSintaxe a utilizar: './feed username'\n");
      return 0;
   }

   TUDOJUNTO contentor;
   contentor.tipo = 1;
   contentor.pid = getpid();
   strcpy(contentor.username, argv[1]);

   struct sigaction sa;
   sa.sa_handler = handler_sigalrm;
   sa.sa_flags = SA_RESTART;
   sigaction(SIGINT, &sa, NULL);

   struct timeval tv;
   fd_set read_fds;
   int nfd;
   
   //PREENCHE CLIENT_FIFO
   sprintf(CLIENT_FIFO_FINAL, CLIENT_FIFO, contentor.pid);
   printf("\nDEBUG: %s\n", CLIENT_FIFO_FINAL);
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
   close(fd_envia);

   //CLIENTE ABRE O SEU NAMED PIPE PARA LEITURA
   int fd_recebe = open(CLIENT_FIFO_FINAL, O_RDWR);
   if (fd_recebe == -1) {
      printf("Erro ao abrir o FIFO do cliente\n");
      return 0;
   }

   int flag = 0;

   do{
      FD_ZERO(&read_fds);
      FD_SET(0, &read_fds);
      FD_SET(fd_recebe, &read_fds);
      nfd = select(fd_recebe+1, &read_fds, NULL, NULL, NULL);
      
      contentor.pid = getpid();
      strcpy(contentor.username, argv[1]);

      if (FD_ISSET(0, &read_fds)) {
         int fd_envia = open(MANAGER_FIFO, O_WRONLY);
         if (fd_envia == -1) {
            printf("Erro ao abrir o FIFO do servidor");
            return 1;
         }
         char buffer[350]; 
         if (fgets(buffer, sizeof(buffer), stdin)) {
            // Remove o '\n' do final da string (se houver)
            buffer[strcspn(buffer, "\n")] = 0;

            if (sscanf(buffer, "msg %19s %d %[^\n]s", contentor.topico, &contentor.duracao, contentor.mensagem) == 3) {
               contentor.tipo = 2; // Enviar mensagem para tópico
            } 
            else if (strcmp(buffer, "topics") == 0) {
               contentor.tipo = 3; // Solicitação de listagem de tópicos
            } 
            else if (sscanf(buffer, "subscribe %19s", contentor.topico) == 1) {
               contentor.pid = getpid();
               contentor.tipo = 4; // Subscrever a um tópico
            } 
            else if (sscanf(buffer, "unsubscribe %19s", contentor.topico) == 1) {
               contentor.pid = getpid();
               contentor.tipo = 5; // Deixar de subscrever um tópico
            } 
            else if (strcmp(buffer, "exit") == 0) {
               contentor.tipo = 6; //fechar 
            } 
            else {
               printf("Comando inválido. Tente novamente.\n");
               continue;
            }

            if (write(fd_envia, &contentor, sizeof(contentor)) == -1) {
               printf("Erro ao escrever no FIFO do servidor\n");
               close(fd_envia);
               unlink(CLIENT_FIFO_FINAL);
               return 2;
            }
         }
      }

      //Escuta named pipe 
      else if(FD_ISSET(fd_recebe, &read_fds)){
         int size = read(fd_recebe, &contentor, sizeof(contentor));
         if (size > 0) {
            if (contentor.tipo == 1){
               printf("\n%s", contentor.msg_devolucao); 
               if(contentor.resultado == 0){ //Nao deu para registar
                  close(fd_recebe); 
                  close(fd_envia);  
                  unlink(CLIENT_FIFO_FINAL);
                  return 0;
               }
            }
            else if (contentor.tipo == 2){
               printf("\nNova mensagem! user:[%s] topico:[%s] msg:[%s]\n", contentor.username, contentor.topico, contentor.mensagem);
               if(contentor.resultado == 0){
                  close(fd_recebe); 
                  close(fd_envia);  
                  unlink(CLIENT_FIFO_FINAL);
                  return 0;
               }
            } 
            else if (contentor.tipo == 4){
               printf("\n%s\n\n", contentor.msg_devolucao);
               fflush(stdout);
            }
            else if(contentor.tipo == 6){
               printf("\nA encerrar cliente...\n");
               fflush(stdout);
               flag = 1;
               sleep(2);
            }
            else if(contentor.tipo == 7){ //outro cliente desconectou se
               printf("%s", contentor.msg_devolucao);
               fflush(stdout);
            }
            else if (contentor.tipo == 8){
               write(fd_envia, &contentor, sizeof(contentor));
            }
         }
      }


}while(flag == 0);

   close(fd_recebe); 
   close(fd_envia);  
   unlink(CLIENT_FIFO_FINAL);  
   return 0;
}

