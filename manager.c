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


int main(){
   login registo;
   login_feedback devolve_registo;
   char usernames[10][20] = {""};


   int fd_manager, size, fd_login;

   //Criar FIFO do manager
   if (mkfifo(MANAGER_FIFO, 0666) == -1){
      if (errno == EEXIST) {
         printf("\nFIFO já existe|\n");
      } 
      else {
         perror("\nErro ao criar FIFO do manager\n");
         return 1;
      }
   }

   //Abrir FIFO do manager para leitura 
   fd_manager = open(MANAGER_FIFO, O_RDONLY);
   if (fd_manager == -1) {
      perror("Erro ao abrir FIFO do manager para leitura");
      return 1;
   }

   while(1){
      size = read(fd_manager, &registo, sizeof(registo));
      if (size > 0){
         sprintf(CLIENT_FIFO_FINAL, CLIENT_FIFO, registo.pid);
         fd_login = open(CLIENT_FIFO_FINAL, O_WRONLY);
         if (fd_login == -1) {
            printf("Erro ao abrir FIFO do cliente para escrita\n");
            return 1;
         }
         

         int username_exists = 0;
         int free_slot = -1;
         for (int i = 0; i < 10; i++){
            if (strcmp(usernames[i],registo.username) == 0){
               username_exists = 1;
            }
            if (usernames[i][0] == '\0' && free_slot == -1) {
               free_slot = i;
            }
         }

         if (username_exists == 1) {
            strcpy(devolve_registo.msg, "Username já existe!\n");
         } 
         else if (free_slot == -1) {
            strcpy(devolve_registo.msg, "Capacidade máxima de utilizadores atingida!\n");
         } 
         else {
            strcpy(devolve_registo.msg, "Usuário adicionado com sucesso\n");
            strcpy(usernames[free_slot], registo.username);
         }
         strcpy(devolve_registo.username, registo.username);
         write(fd_login, &devolve_registo, sizeof(devolve_registo));
         close(fd_login);
      }
   }
}
