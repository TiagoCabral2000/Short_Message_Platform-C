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
int sigint_received = 0;

void handler_sigint(int s) {
    sigint_received = 1; 
}
void handler_sigterm(int s) {
    sigint_received = 1; 
}

void handler_sigfpe(int s) {
    sigint_received = 1;
}

int main(int argc, char *argv[]) {
   if (argc != 2) {
      printf("\nNumero de parametros invalido!\nSintaxe a utilizar: './feed username'\n");
      return 0;
   }

   struct sigaction sa_int, sa_term, sa_fpe;

   //ctrl+c
   sa_int.sa_handler = handler_sigint; 
   sa_int.sa_flags = SA_RESTART;
   sigaction(SIGINT, &sa_int, NULL);

   //kill pid
   sa_term.sa_handler = handler_sigterm;
   sa_term.sa_flags = SA_RESTART;
   sigaction(SIGTERM, &sa_term, NULL);

   //kill -8 pid
   sa_fpe.sa_handler = handler_sigfpe; 
   sa_fpe.sa_flags = SA_RESTART;
   sigaction(SIGFPE, &sa_fpe, NULL);

   fd_set read_fds;
   int nfd;

   IDENTIFICADOR id;
   id.tipo = 1;
   LOGIN login;
   login.pid = getpid();
   strcpy(login.username, argv[1]);

   MSG msg;
   SUBSCRIBE sub;

   sprintf(CLIENT_FIFO_FINAL, CLIENT_FIFO, getpid());
   if (mkfifo(CLIENT_FIFO_FINAL, 0666) == -1) {
      if (errno != EEXIST) {
         printf("Erro ao criar FIFO do cliente\n");
         return 1;
      }
   }

   // ABRE PIPE DO MANAGER PARA ESCRITA
   int fd_envia = open(MANAGER_FIFO, O_WRONLY);
   if (fd_envia == -1) {
      printf("Erro ao abrir o FIFO do servidor\n");
      return 1;
   }

   if (write(fd_envia, &id, sizeof(id)) == -1) {
      printf("Erro ao escrever no FIFO do servidor\n");
      close(fd_envia);
      unlink(CLIENT_FIFO_FINAL);
      return 2;
   }

   if (write(fd_envia, &login, sizeof(login)) == -1) {
      printf("Erro ao escrever no FIFO do servidor\n");
      close(fd_envia);
      unlink(CLIENT_FIFO_FINAL);
      return 2;
   }


   // CLIENTE ABRE O SEU NAMED PIPE PARA LEITURA
   int fd_recebe = open(CLIENT_FIFO_FINAL, O_RDWR);
   if (fd_recebe == -1) {
      printf("Erro ao abrir o FIFO do cliente\n");
      return 0;
   }

   int flag = 0;
   do {
      FD_ZERO(&read_fds);
      FD_SET(0, &read_fds);
      FD_SET(fd_recebe, &read_fds);
      nfd = select(fd_recebe + 1, &read_fds, NULL, NULL, NULL);

      if (sigint_received) {
         printf("Sinal recebido!\n");
         int fd_envia = open(MANAGER_FIFO, O_WRONLY);
         if (fd_envia != -1) {
            id.tipo = 7; // Indicate shutdown
            if (write(fd_envia, &id, sizeof(id)) == -1) {
               printf("Erro ao escrever no FIFO do servidor\n");
            }
            if (write(fd_envia, &login, sizeof(login)) == -1) {
               printf("Erro ao escrever no FIFO do servidor\n");
            }
            close(fd_envia);  
            if (fd_recebe >= 0) close(fd_recebe);
            unlink(CLIENT_FIFO_FINAL);
            printf("A encerrar cliente...\n");
            fflush(stdout);
            sleep(1);
            return 0;
         }  
      }

      if (FD_ISSET(0, &read_fds)) {
         int fd_envia = open(MANAGER_FIFO, O_WRONLY);
         if (fd_envia == -1) {
            printf("Erro ao abrir o FIFO do servidor");
            return 1;
         }
         char buffer[350]; 
         if (fgets(buffer, sizeof(buffer), stdin)) {
            buffer[strcspn(buffer, "\n")] = 0;// Remove o '\n' do final da string se houver

            if (sscanf(buffer, "msg %19s %d %[^\n]s", msg.topico, &msg.duracao, msg.mensagem) == 3) {
               id.tipo = 2;
               msg.pid = getpid();
               strcpy(msg.username, argv[1]);
               if (write(fd_envia, &id, sizeof(id)) == -1) {
                  printf("Erro ao escrever no FIFO do servidor\n");
                  close(fd_envia);
                  unlink(CLIENT_FIFO_FINAL);
                  return 2;
               }
               if (write(fd_envia, &msg, sizeof(msg)) == -1) {
                  printf("Erro ao escrever no FIFO do servidor\n");
                  close(fd_envia);
                  unlink(CLIENT_FIFO_FINAL);
                  return 2;
               }
            } 
            else if (strcmp(buffer, "topics") == 0) {
               id.tipo = 3;
               if (write(fd_envia, &id, sizeof(id)) == -1) {
                  printf("Erro ao escrever no FIFO do servidor\n");
                  close(fd_envia);
                  unlink(CLIENT_FIFO_FINAL);
                  return 2;
               }
            } 
            else if (sscanf(buffer, "subscribe %19s", sub.topico) == 1) {
               sub.pid = getpid();
               id.tipo = 4;
               if (write(fd_envia, &id, sizeof(id)) == -1) {
                  printf("Erro ao escrever no FIFO do servidor\n");
                  close(fd_envia);
                  unlink(CLIENT_FIFO_FINAL);
                  return 2;
               }
               if (write(fd_envia, &sub, sizeof(sub)) == -1) {
                  printf("Erro ao escrever no FIFO do servidor\n");
                  close(fd_envia);
                  unlink(CLIENT_FIFO_FINAL);
                  return 2;
               }
            } 
            else if (sscanf(buffer, "unsubscribe %19s", sub.topico) == 1) {
               id.tipo = 5; 
               sub.pid = getpid();
               if (write(fd_envia, &id, sizeof(id)) == -1) {
                  printf("Erro ao escrever no FIFO do servidor\n");
                  close(fd_envia);
                  unlink(CLIENT_FIFO_FINAL);
                  return 2;
               }
               if (write(fd_envia, &sub, sizeof(sub)) == -1) {
                  printf("Erro ao escrever no FIFO do servidor\n");
                  close(fd_envia);
                  unlink(CLIENT_FIFO_FINAL);
                  return 2;
               }
            } 
            else if (strcmp(buffer, "exit") == 0) {
               id.tipo = 6; 
               if (write(fd_envia, &id, sizeof(id)) == -1) {
                  printf("Erro ao escrever no FIFO do servidor\n");
                  close(fd_envia);
                  unlink(CLIENT_FIFO_FINAL);
                  return 2;
               }
               if (write(fd_envia, &login, sizeof(login)) == -1) { //info necessaria para saber qual remover
                  printf("Erro ao escrever no FIFO do servidor\n");
                  close(fd_envia);
                  unlink(CLIENT_FIFO_FINAL);
                  return 2;
               }
               printf("\nA encerrar cliente...\n");
               fflush(stdout);
               flag = 1;
               sleep(1);
            } 
            else {
               printf("Comando inválido. Tente novamente.\n");
               continue;
            }
         }
      }

      //Escuta named pipe 
      else if(FD_ISSET(fd_recebe, &read_fds)){
         int size = read(fd_recebe, &id, sizeof(id));
         if (size > 0) {
            switch(id.tipo){
               case 1:{
                  FEEDBACK feedback;
                  read(fd_recebe, &feedback, sizeof(feedback));
                  printf("%s", feedback.msg_devolucao); 
                  if(feedback.resultado == 0){ 
                     close(fd_recebe); 
                     close(fd_envia);  
                     unlink(CLIENT_FIFO_FINAL);
                     return 0;
                  }
                  break;
               }
               case 2:{
                  MSG msg;
                  read(fd_recebe, &msg, sizeof(msg));
                  printf("\nNova mensagem! user:[%s] topico:[%s] msg:[%s]", msg.username, msg.topico, msg.mensagem);
                  fflush(stdout);
                  break;
               } 
               case 3: {
                  FEEDBACK feedback;
                  read(fd_recebe, &feedback, sizeof(feedback));
                  printf("\n%s", feedback.msg_devolucao);
                  fflush(stdout);
                  break;
               }
               case 4:{
                  printf("\nManager encerrado. A encerrar cliente...\n");
                  fflush(stdout);
                  flag = 1;
                  sleep(1);
                  break;
               }
               case 5:{
                  printf("\nRemovido pelo administrador. A encerrar cliente...\n");
                  fflush(stdout);
                  flag = 1;
                  sleep(1);
                  break;
               }
            }
         }
         printf("\n");
      }
   } while (flag == 0);

   close(fd_recebe); 
   unlink(CLIENT_FIFO_FINAL);  
   return 0;
}
