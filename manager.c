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
/*
typedef struct{
   int pid;
   char username[20];
} login;

typedef struct{
   char msg[50];
   int result;
} login_feedback;
*/
typedef struct{
      int tipo, pid, resultado, duracao;
      char username[20], topico[20], mensagem[300], msg_devolucao[50]; 
   }TUDOJUNTO;

typedef struct {
    int tipo; 
    union {
        login log;
        login_feedback log_feedback;
        dataMSG data_message;
        devolveMSG return_message;
    } content;
} structs_container;

typedef struct {
   char nome_topico[20];           
   int pid_clientes[10];     
   int numClientes;               
} Topico;
int numTopicos = 0;
Topico topicos[20];

void novoLogin(TUDOJUNTO* cont, char usernames[10][20]) {
   int username_exists = 0;
   int free_slot = -1;
   printf("\nUsernames registados: ");
   for (int i = 0; i < 10; i++){
       printf("['%s'] ", usernames[i]);
      if (strcmp(usernames[i], cont->username) == 0) 
         username_exists = 1;
      if (usernames[i][0] == '\0' && free_slot == -1) 
         free_slot = i;
   }

   if (username_exists == 1) {
      strcpy(cont->msg_devolucao, "Username já existe!\n");
      cont->resultado  = 0;
   } 
   else if (free_slot == -1) {
      strcpy(cont->msg_devolucao, "Capacidade máxima de utilizadores atingida!\n");
      cont->resultado = 0;
   } 
   else {
      strcpy(cont->msg_devolucao, "Utilizador adicionado com sucesso\n");
      strcpy(usernames[free_slot], cont->username);
      cont->resultado = 1;
   }  
}


void analisaTopico(TUDOJUNTO* container, Topico* topicos, int* numTopicos) {
    int topic_exists = 0;
    int free_slot = -1;

    // Procura pelo tópico ou um slot livre
    for (int i = 0; i < 20; i++){
        if (strcmp(topicos[i].nome_topico, container->topico) == 0) {
            topic_exists = 1;
            break;
        }
        if (topicos[i].nome_topico[0] == '\0' && free_slot == -1) 
            free_slot = i; 
    }

    if (topic_exists){
        strcpy(container->msg_devolucao, "O tópico já existe");
        container->resultado = 0;
    } 
    else if (free_slot != -1){ //Ha espaço para criar um novo topico
        strcpy(topicos[free_slot].nome_topico, container->topico);
        topicos[free_slot].pid_clientes[0] = container->pid;
        topicos[free_slot].numClientes = 1;
        (*numTopicos)++;
        strcpy(container->msg_devolucao, "Tópico criado com sucesso");
        container->resultado = 1;
    } 
    else{
        strcpy(container->msg_devolucao, "Capacidade máxima de tópicos atingida");
        container->resultado = 0;
    }
}

void subscreveCliente(TUDOJUNTO* container, Topico* topicos) {
   const char* topico_requisitado = container->topico;
   int pid_cliente = container->pid;

   for (int i = 0; i < 20; i++) {
      //Verifica se o tópico corresponde
      if (strcmp(topicos[i].nome_topico, topico_requisitado) == 0) {
         // Verifica se o cliente já está inscrito
         for (int j = 0; j < topicos[i].numClientes; j++) {
            if (topicos[i].pid_clientes[j] == pid_cliente) {
               container->resultado = 1; // Já está inscrito
               return;
            }
         }

         // Adiciona o cliente, se possível
         if (topicos[i].numClientes < 10) {
            topicos[i].pid_clientes[topicos[i].numClientes++] = pid_cliente;
            container->resultado = 1; // Inscrição bem-sucedida
         } 
         else {
            strcpy(container->msg_devolucao, "Máximo de clientes atingido para este tópico");
            container->resultado= 0; // Limite atingido
         }
         return; // Tópico encontrado e tratado
      }
   }

   strcpy(container->msg_devolucao, "Tópico não encontrado");
   container->resultado = 0;
}


void distribuiMensagem(TUDOJUNTO* container, Topico* topicos){
   container->tipo = 2;
   for (int i = 0; i < numTopicos; ++i) {
      if (strcmp(topicos[i].nome_topico, container->topico) == 0) { // Verifica se o tópico existe
         for (int j = 0; j < topicos[i].numClientes; ++j) { // Envia a mensagem para cada cliente associado
            char cliente_fifo[100];
            sprintf(cliente_fifo, CLIENT_FIFO, topicos[i].pid_clientes[j]);
            int fd_envia = open(cliente_fifo, O_WRONLY);

            if (fd_envia != -1) {
                    
               write(fd_envia, container, sizeof(*container)); // Envia a mensagem
               close(fd_envia);
            } 
            else {
               printf("Erro ao abrir FIFO para cliente %d\n", topicos[i].pid_clientes[j]);
            }
         }
         break; // Encontrou o tópico, não precisa continuar a busca
      }
   }  
}






typedef struct{
   int tipo;
   int pid;
   char username[20];
}login_teste;

typedef struct{
   int resultado;
   char msg[50];
}login_feedback_teste;




int main(){
   char usernames[10][20] = {""};
   structs_container container;
   devolveMSG msg_devolvida;
   TUDOJUNTO contentor;

   login_teste log_teste;
   login_feedback_teste log_fedback_teste;

   int fd_recebe, size, fd_envia;

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
   fd_recebe = open(MANAGER_FIFO, O_RDONLY);
   if (fd_recebe == -1) {
      perror("Erro ao abrir FIFO do manager para leitura");
      return 1;
   }

   while(1){
      int size = read(fd_recebe, &contentor, sizeof(contentor));
      if (size > 0){
         if (contentor.tipo == 1){

            novoLogin(&contentor, usernames);
            sprintf(CLIENT_FIFO_FINAL, CLIENT_FIFO, contentor.pid);
           printf("\nUSERNAME: %s", contentor.username);
           printf("\nOpening %s for client %d\n", CLIENT_FIFO_FINAL, contentor.pid);
            int fd_envia = open(CLIENT_FIFO_FINAL, O_WRONLY);
            if (fd_envia == -1) {
               printf("Erro ao abrir FIFO do cliente para escrita\n");
               return 2;
            } 
            write(fd_envia, &contentor, sizeof(contentor));
            close(fd_envia);
            
         }
        
         if (contentor.tipo == 2){ 
            printf("\nNAO DEVIA ENTRAR AQUI");
            
            analisaTopico(&contentor, topicos, &numTopicos);
            subscreveCliente(&contentor, topicos);
            distribuiMensagem(&contentor, topicos);
            
         }
      }
      
   }
   close(fd_recebe);
   unlink(MANAGER_FIFO);
   return 0;
}
