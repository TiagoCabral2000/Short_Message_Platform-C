#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>

#define MANAGER_FIFO "MANAGER_FIFO"
#define CLIENT_FIFO "CLIENT_FIFO%d"
char CLIENT_FIFO_FINAL[100];

typedef struct{
   int pid;
   char username[20];
} login;

typedef struct{
   char username[20];
   char msg[50];
} login_feedback;

int main(int argc, char *argv[]){
   if (argc != 2){
      printf("\nNumero de parametros invalido!\nSintaxe a utilizar: ./feed username\n");
      return 0;
   }

   login registo;
   login_feedback feedback_registo;
   
   registo.pid = getpid();
   strcpy(registo.username, argv[1]);

   sprintf(CLIENT_FIFO_FINAL, CLIENT_FIFO, registo.pid);
   if(mkfifo(CLIENT_FIFO_FINAL, 0666) == -1){
      if (errno != EEXIST) {
         printf("Erro ao criar FIFO do cliente\n");
         return 1;
      }
   }

   int fd_registo = open(MANAGER_FIFO, O_WRONLY);
   if (fd_registo == -1) {
      printf("Erro ao abrir o FIFO do servidor");
      return 1;
   }

    if (write(fd_registo, &registo, sizeof(registo)) == -1) {
      printf("Erro ao escrever no FIFO do servidor\n");
      close(fd_registo);
      unlink(CLIENT_FIFO_FINAL);
      return 2;
   }

   int fd_recebe_registo = open(CLIENT_FIFO_FINAL, O_RDONLY);
   if (fd_recebe_registo == -1) {
      printf("Erro ao abrir o FIFO do cliente\n");
      return 0;
   }

   int size = read(fd_recebe_registo, &feedback_registo, sizeof(feedback_registo));
   if (size > 0) {
      printf("%s", feedback_registo.msg);
   } 
   else {
      printf("Erro ao ler a resposta do servidor\n");
   }
   close(fd_recebe_registo); 
   close(fd_registo);  
   unlink(CLIENT_FIFO_FINAL);  
   return 0;


}