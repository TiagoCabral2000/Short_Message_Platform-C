#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include "estruturas.h"

#define MANAGER_FIFO "MANAGER_FIFO"
#define CLIENT_FIFO "CLIENT_FIFO%d"
char CLIENT_FIFO_FINAL[100];

typedef struct {
   char nome_topico[20];
   int pid_clientes[10];
   int numClientes;
	char msg_persistentes[5][300];
	int tempo[5];
   char usernames[5][20];
	int numPersistentes;
   int bloqueado;
} Topico;


#define MAX_TOPICOS 20
#define MAX_CLIENTES 10
typedef struct {
   Topico topicos[MAX_TOPICOS];
   int numTopicos;
   char usernames[MAX_CLIENTES][20];
	int pids[20];
	int numCli;
   pthread_mutex_t *m;
   int lock;
   int fd;
} ServerData;

ServerData *global_server_data = NULL; 

void novoLogin(LOGIN* login, ServerData *serverdata);
int analisaTopico(MSG* msg, ServerData *serverdata);
void subscreveCliente(MSG* msg, IDENTIFICADOR* id, SUBSCRIBE* sub, ServerData* serverData);
int guardaPersistentes(MSG* msg, ServerData* ServerData);
void distribuiMensagem(MSG* msg, ServerData* serverData);
void apagaUsername(char username[20], ServerData* serverData, int flag);
void unsubscribe(SUBSCRIBE* sub, ServerData* ServerData);

void guardaPersistentesFicheiro(ServerData* serverData);
void recuperaPersistentesFicheiro(ServerData* serverData);

void mostraTopicos(ServerData* serverData, char nomeTopico[20]);
void encerraTodosClientes(ServerData* serverData);

void handler_sigalrm(int s, siginfo_t *info, void *context) {
   encerraTodosClientes(global_server_data);
    if (global_server_data != NULL) {
        guardaPersistentesFicheiro(global_server_data); // Save the data
    }
    unlink(MANAGER_FIFO);
    printf("\nServidor encerrado\n");
    exit(1);
}

void* processaNamedPipes(void* aux) {
    ServerData* serverData = (ServerData*)aux;

	 int size;
    IDENTIFICADOR id;
    int flag1 = 0, flag2 = 0;

    while (serverData->lock == 0) {
        size = read(serverData->fd, &id, sizeof(id));
        if (size > 0) {
         switch(id.tipo){
            case 1: {
               LOGIN login;
               read(serverData->fd, &login, sizeof(login));
               novoLogin(&login, serverData);
               break;
            } 
            case 2: {
               MSG msg;
               SUBSCRIBE sub = {0};
               read(serverData->fd, &msg, sizeof(msg));
               flag1 = analisaTopico(&msg, serverData);

               if (flag1 == 0){
                  flag2 = guardaPersistentes(&msg, serverData);

                  if (flag2 == 0){
                     subscreveCliente(&msg, &id, &sub, serverData);
                     distribuiMensagem(&msg, serverData);
                  }
               }
               break;
            } 
            case 3: {
               char nome[20] = "\0";
               mostraTopicos(serverData, nome);
               break;
            }
            case 4: {
               MSG msg = {0};
               SUBSCRIBE sub;
               read(serverData->fd, &sub, sizeof(sub));
            	subscreveCliente(&msg, &id, &sub, serverData);
               break;
            }
            case 5:{
               SUBSCRIBE sub;
               read(serverData->fd, &sub, sizeof(sub));
               unsubscribe(&sub, serverData);
               break;
            }
            case 6: {
               LOGIN login;
               read(serverData->fd, &login, sizeof(login));
               int flag = 0;
               apagaUsername(login.username, serverData, flag);
               break;
				}
            case 7: {
               LOGIN login;
               read(serverData->fd, &login, sizeof(login));
               int flag = 1;
               apagaUsername(login.username, serverData, flag);
               break;
            }
				default:
					printf("\nTipo n existe");
			}
      }
   }
    close(serverData->fd);
    unlink(MANAGER_FIFO);
    return NULL;
}

void* descontaTempo(void *aux){
   ServerData *serverData = (ServerData *) aux;
   int i, j, k;
   while (serverData->lock == 0){
      sleep(1);
      pthread_mutex_lock(serverData->m);
      for (i = 0; i < serverData->numTopicos; i++) {
         if (serverData->topicos[i].numPersistentes > 0){
            for (j = 0; j < serverData->topicos[i].numPersistentes; j++){
               serverData->topicos[i].tempo[j]--;
               if (serverData->topicos[i].tempo[j] == 0){
                  for (k=j; k < 4; k++){
                     serverData->topicos[i].tempo[k] = serverData->topicos[i].tempo[k+1];
                     strcpy(serverData->topicos[i].msg_persistentes[k], serverData->topicos[i].msg_persistentes[k+1]);
                     strcpy(serverData->topicos[i].usernames[k], serverData->topicos[i].usernames[k+1]);
                  }
                  strcpy(serverData->topicos[i].msg_persistentes[k], "\0");
                  strcpy(serverData->topicos[i].usernames[k], "\0");
                  serverData->topicos[i].tempo[k] = 0;
                  serverData->topicos[i].numPersistentes--;
               }
            }
         }
      }
      pthread_mutex_unlock(serverData->m);
   }
}

void* gereTopicos(void* aux) {
    ServerData* serverData = (ServerData*)aux;
    int i, j;
    while (serverData->lock == 0){
      if (serverData->numTopicos > 0){
         sleep(10);
         for (i = 0; i < serverData->numTopicos; i++) {
            if (serverData->topicos[i].numClientes == 0 && serverData->topicos->numPersistentes == 0){
               pthread_mutex_lock(serverData->m);
               for (int j = 0; j < serverData->numTopicos; j++){
                  serverData->topicos[j] = serverData->topicos[j+1];
               }
               serverData->numTopicos--;
               pthread_mutex_unlock(serverData->m);
            }
         }
      }
    }
}


int main() {
    pthread_mutex_t mutex; 
    pthread_mutex_init (&mutex,NULL); 
    ServerData serverData = {0};
    serverData.lock = 0;
    serverData.m = &mutex;
    pthread_t thr_pipes, thr_tempo, thr_gestaoTopicos;

    for (int i = 0; i < MAX_TOPICOS; i++) {
      for (int j = 0; j < 5; j++) {
         strcpy(serverData.topicos[i].msg_persistentes[j], "\0");
         strcpy(serverData.topicos[i].usernames[j], "\0");
         serverData.topicos[i].tempo[j] = 0;
      }
   }

   recuperaPersistentesFicheiro(&serverData);

   struct sigaction sa;
   sa.sa_sigaction = handler_sigalrm;
   sa.sa_flags = SA_SIGINFO | SA_RESTART;
   sigaction(SIGINT, &sa, NULL);
   global_server_data = &serverData;

    if (mkfifo(MANAGER_FIFO, 0666) == -1) {
        perror("Erro ao criar FIFO do manager");
        return 1;
    }

    int fd_recebe = open(MANAGER_FIFO, O_RDWR);
    if (fd_recebe == -1) {
        perror("Erro ao abrir FIFO do manager para leitura");
        pthread_exit(NULL);
    }

    serverData.fd = fd_recebe;

    if (pthread_create(&thr_pipes, NULL, processaNamedPipes, &serverData) != 0) {
        perror("Erro ao criar thread");
        return 1;
    }
    if (pthread_create(&thr_tempo, NULL, descontaTempo, &serverData) != 0) {
        perror("Erro ao criar thread");
        return 1;
    } 
    if (pthread_create(&thr_gestaoTopicos, NULL, gereTopicos, &serverData) != 0) {
        perror("Erro ao criar thread");
        return 1;
    }

    char buffer[50];
    char username[20];
    char nomeTopico[20];
    while (1) {
        
        if (fgets(buffer, sizeof(buffer), stdin)) {
            // Remove o '\n' do final da string se houver
            buffer[strcspn(buffer, "\n")] = 0;

            if (sscanf(buffer, "remove %19s", username) == 1) {
               int flag = 0;
               apagaUsername(username, &serverData, flag);
            } 
            else if (strcmp(buffer, "topics") == 0) {
               char nome[20] = "\0";
               mostraTopicos(&serverData, nome);
            } 
            else if (sscanf(buffer, "show %19s", nomeTopico) == 1) {
               mostraTopicos(&serverData, nomeTopico);
            } 
            else if (sscanf(buffer, "lock %19s", nomeTopico) == 1) {
               int found = 0;
               for (int i = 0; i < serverData.numTopicos; i++){
                  if (strcmp(serverData.topicos[i].nome_topico, nomeTopico) == 0){
                     found = 1;
                     serverData.topicos[i].bloqueado = 1;
                  }
               }
               if(found == 1){
                  printf("\nTopico %s bloqueado!\n", nomeTopico);
               }
               else{
                  printf("\nTopico %s nao encontrado!\n", nomeTopico);
               }
            } 
            else if (sscanf(buffer, "unlock %19s", nomeTopico) == 1) {
               int found = 0;
               for (int i = 0; i < serverData.numTopicos; i++){
                  if (strcmp(serverData.topicos[i].nome_topico, nomeTopico) == 0){
                     found = 1;
                     serverData.topicos[i].bloqueado = 0;
                  }
               }
               if(found == 1){
                  printf("\nTopico %s desbloqueado!\n", nomeTopico);
               }
               else{
                  printf("\nTopico %s nao encontrado!\n", nomeTopico);
               }
            } 
            else if (strcmp(buffer, "users") == 0) {
               printf("*********************************");
               printf("\n -> Numero clientes ativos = %d", serverData.numCli);
               printf("\n -> Usernames registados: ");
               for (int i = 0; i < 10; i++){
                  printf("[%s] ", serverData.usernames[i]);
               }
               printf("\n");
               printf(" -> PID's registados: ");
               for (int i = 0; i < 10; i++){
                  printf("[%d] ", serverData.pids[i]);
               }
               printf("\n*********************************\n");

            }
            else if (strcmp(buffer, "close") == 0) {
               serverData.lock = 1;           
               encerraTodosClientes(&serverData);
               kill(getpid(), SIGINT);
               break;
            }
            else {
               printf("Comando inválido. Tente novamente.\n");
               continue;
            }
         } 
   }

   pthread_join(thr_pipes, NULL);
   return 0;
}

void novoLogin(LOGIN* login, ServerData *serverData) {
   FEEDBACK feedback;
   IDENTIFICADOR id;
   id.tipo = 1;
   int fd_envia;
   int username_exists = 0;
   int free_slot = -1;
   for (int i = 0; i < 10; i++){
      if (strcmp(serverData->usernames[i], login->username) == 0) 
         username_exists = 1;
      if (serverData->usernames[i][0] == '\0' && free_slot == -1) 
         free_slot = i;
   }

   if (username_exists == 1) {
      strcpy(feedback.msg_devolucao, "Username já existe!\n");
      feedback.resultado  = 0;
   } 
   else if (free_slot == -1) {
      strcpy(feedback.msg_devolucao, "Capacidade máxima de utilizadores atingida!\n");
      feedback.resultado = 0;
   } 
   else {
      strcpy(feedback.msg_devolucao, "Utilizador adicionado com sucesso\n");
      strcpy(serverData->usernames[free_slot], login->username);
		serverData->pids[free_slot] = login->pid;
		serverData->numCli++;
      feedback.resultado = 1;
   }  

   sprintf(CLIENT_FIFO_FINAL, CLIENT_FIFO, login->pid);

   fd_envia = open(CLIENT_FIFO_FINAL, O_WRONLY);

   if (fd_envia != -1) {
      write(fd_envia, &id, sizeof(id));
      write(fd_envia, &feedback, sizeof(feedback)); // Envia a mensagem
      close(fd_envia);
   } 
   else {
      printf("Erro ao abrir FIFO para cliente %d\n", login->pid);
   }
}


int analisaTopico(MSG* msg, ServerData *serverdata) {
    int topic_exists = 0;
    int index = -1;
    int free_slot = -1;
    int res = -1;
    int envia_msg = 0;
    FEEDBACK feedback;
    IDENTIFICADOR id;

    // Procura pelo tópico ou um slot livre
    for (int i = 0; i < 20; i++){
        if (strcmp(serverdata->topicos[i].nome_topico, msg->topico) == 0) {
            topic_exists = 1;
            res = 0;
            index = i;
            break;
        }
        if (strcmp(serverdata->topicos[i].nome_topico, "\0") == 0 && free_slot == -1) {
            free_slot = i; 
         }
    }

    if (topic_exists == 1){
      if(serverdata->topicos[index].bloqueado == 1){
         strcpy(feedback.msg_devolucao, "Topico bloqueado pelo administrador!\n");
         envia_msg = 1;
         res = 1;
      }
    } 
    else if (free_slot != -1){ //Ha espaço para criar um novo topico
      strcpy(serverdata->topicos[free_slot].nome_topico, msg->topico);
      serverdata->topicos[free_slot].pid_clientes[0] = msg->pid;
      serverdata->topicos[free_slot].numClientes = 1;
      serverdata->topicos[free_slot].bloqueado = 0;
      (serverdata->numTopicos)++; 
      res = 0; 
    } 
    else{
      strcpy(feedback.msg_devolucao, "Capacidade máxima de tópicos atingida");
      res = 1;
      envia_msg = 1;
    }

      if(envia_msg == 1){
         id.tipo = 3;
         sprintf(CLIENT_FIFO_FINAL, CLIENT_FIFO, msg->pid);

         int fd_envia = open(CLIENT_FIFO_FINAL, O_WRONLY);
         if (fd_envia != -1) {
            write(fd_envia, &id, sizeof(id));
            write(fd_envia, &feedback, sizeof(feedback)); // Envia a mensagem
            close(fd_envia);
         } 
         else {
            printf("Erro ao abrir FIFO para cliente %d\n",  msg->pid);
         }
      }
    return res;
}

void subscreveCliente(MSG* msg, IDENTIFICADOR* id, SUBSCRIBE* sub, ServerData* serverData) {
   int found; 
   int alreadySubscribed = -1; 
   int fd_envia;
   int index = -1;
   int PID = -1;
   char utilizador[20];
   int envia_feedback = 0;
   int nova_subscricao = 0;
   FEEDBACK feedback;

   if (id->tipo == 2){
      found = -1;
      PID = msg->pid;
      strcpy(utilizador, msg->username);
      for (int i = 0; i < 20; i++) {
      // Verifica se o tópico corresponde
         if (strcmp(serverData->topicos[i].nome_topico, msg->topico) == 0) {
            found = 1; 
            index = i;

            // Verifica se o cliente já está inscrito
            for (int j = 0; j < serverData->topicos[i].numClientes; j++) {
               if (serverData->topicos[i].pid_clientes[j] == msg->pid) {
                  alreadySubscribed = 1; 
                  break;
               }
            }
            if (alreadySubscribed == -1) {
               if (serverData->topicos[index].numClientes < 10) {
                  serverData->topicos[index].pid_clientes[serverData->topicos[index].numClientes] = msg->pid;
                  serverData->topicos[index].numClientes++;
                  strcpy(feedback.msg_devolucao, "Subscrito com sucesso!");
                  envia_feedback = 1;
                  nova_subscricao = 1;
               }
               else {
                  strcpy(feedback.msg_devolucao, "Máximo de clientes atingido para este tópico");
                  envia_feedback = 1;
               }
            }
            break;
         }
      }
   }

   else if(id->tipo == 4){
      PID = sub->pid;
      strcpy(utilizador, sub->username);
      found = -1;
      for (int i = 0; i < 20; i++) {
         if (strcmp(serverData->topicos[i].nome_topico, sub->topico) == 0) {
            found = 1; 
            index = i;

            for (int j = 0; j < serverData->topicos[i].numClientes; j++) {
               if (serverData->topicos[i].pid_clientes[j] == sub->pid) {
                  alreadySubscribed = 1; 
                  break;
               }
            }
            if (alreadySubscribed == 1) {
               strcpy(feedback.msg_devolucao, "Já inscrito!");
               envia_feedback = 1;
               break;
            } 
            else{
               if (serverData->topicos[index].numClientes < 10) {
                  serverData->topicos[index].pid_clientes[serverData->topicos[index].numClientes] = sub->pid;
                  serverData->topicos[index].numClientes++;
                  strcpy(feedback.msg_devolucao, "Subscrito com sucesso!");
                  envia_feedback = 1;
                  nova_subscricao = 1;
               }
               else {
                  strcpy(feedback.msg_devolucao, "Máximo de clientes atingido para este tópico");
                  envia_feedback = 1;
               }
            }
            break;
         }
      }
   }

   if (found != 1){
      strcpy(feedback.msg_devolucao, "Topico nao encontrado");
      envia_feedback = 1;
   }

   if (envia_feedback == 1){ //Se o comando foi subscribe
      sprintf(CLIENT_FIFO_FINAL, CLIENT_FIFO, PID);
      id->tipo = 3;
      fd_envia = open(CLIENT_FIFO_FINAL, O_WRONLY);
      if (fd_envia != -1) {
         write(fd_envia, id, sizeof(*id)); 
         write(fd_envia, &feedback, sizeof(feedback));
         close(fd_envia);
      } 
      else {
         printf("Erro ao abrir FIFO para cliente %d\n", PID);
      }
   } 

   if (nova_subscricao == 1){
      if (serverData->topicos[index].numPersistentes > 0){
         for (int j = 0; j < serverData->topicos[index].numPersistentes; j++){
            id->tipo = 2;
            strcpy(msg->mensagem, serverData->topicos[index].msg_persistentes[j]);
            strcpy(msg->topico, serverData->topicos[index].nome_topico);
            strcpy(msg->username, serverData->topicos[index].usernames[j]);

             if(strcmp(msg->username, utilizador) == 0) //Nao recebe a propria mensagem duplicada
                continue;
             else{
                     sprintf(CLIENT_FIFO_FINAL, CLIENT_FIFO, PID);
                     fd_envia = open(CLIENT_FIFO_FINAL, O_WRONLY);
                     if (fd_envia != -1) {
                        write(fd_envia, id, sizeof(*id));
                        write(fd_envia, msg, sizeof(*msg)); 
                        close(fd_envia);
                     }
                     else {
                        printf("Erro ao abrir FIFO para cliente %d\n", PID);
                     } 
            }    
         }
      }
   } 
   
}


int guardaPersistentes(MSG* msg, ServerData* ServerData){
    int free_slot = -1;
    int index = -1;
    int envia_feedback = 0;
    int res = 0;
    FEEDBACK feedback;
    IDENTIFICADOR id;

    if (msg->duracao > 0) {
        // Percorrer os tópicos
        for (int i = 0; i < ServerData->numTopicos; i++) {
            if (strcmp(ServerData->topicos[i].nome_topico, msg->topico) == 0) {
               index = i;
               res = 0;
               if (ServerData->topicos[index].numPersistentes == 5) {
                  envia_feedback = 1;
                  res = 1;
                  break;
               }
               else{
                    // Encontrar slot livre
                    for (int j = 0; j < 5; j++) {
                        if (strcmp(ServerData->topicos[i].msg_persistentes[j], "\0") == 0) {
                            free_slot = j;
                            res = 0;
                            break;
                        }
                    }
               }
               break;
            }  
        }

        if (index == -1 || free_slot == -1) {
            res = 1;
        }
        else{
            res = 0;
            pthread_mutex_lock(ServerData->m);
            strcpy(ServerData->topicos[index].msg_persistentes[free_slot], msg->mensagem);
            strcpy(ServerData->topicos[index].usernames[free_slot], msg->username);
            ServerData->topicos[index].tempo[free_slot] = msg->duracao;
            ServerData->topicos[index].numPersistentes++;
            pthread_mutex_unlock(ServerData->m);
        }
    } 

    if (envia_feedback == 1){
      id.tipo = 3;
      strcpy(feedback.msg_devolucao, "Limite de mensagens persistentes atingido!");
      sprintf(CLIENT_FIFO_FINAL, CLIENT_FIFO, msg->pid);

      int fd_envia = open(CLIENT_FIFO_FINAL, O_WRONLY);

            if (fd_envia != -1) {
               write(fd_envia, &id, sizeof(id));
               write(fd_envia, &feedback, sizeof(feedback)); // Envia a mensagem
               close(fd_envia);
            } else {
               printf("Erro ao abrir FIFO para cliente %d\n", msg->pid);
            }

    }
    return res;
}



void distribuiMensagem(MSG* msg, ServerData* serverData) {
   IDENTIFICADOR id;
   id.tipo = 2;
   int fd_envia;
   for (int i = 0; i < serverData->numTopicos; ++i) {
      if (strcmp(serverData->topicos[i].nome_topico, msg->topico) == 0) {
         for (int j = 0; j < serverData->topicos[i].numClientes; ++j) {
            sprintf(CLIENT_FIFO_FINAL, CLIENT_FIFO, serverData->topicos[i].pid_clientes[j]);

            fd_envia = open(CLIENT_FIFO_FINAL, O_WRONLY);

            if (fd_envia != -1) {
               write(fd_envia, &id, sizeof(id));
               write(fd_envia, msg, sizeof(*msg)); // Envia a mensagem
               close(fd_envia);
            } else {
               printf("Erro ao abrir FIFO para cliente %d\n", serverData->topicos[i].pid_clientes[j]);
            }
         }
         break; // Encontrou o tópico, não precisa continuar
      }
   }
}

void apagaUsername(char username[20], ServerData* serverData, int flag) {
   int pid = -1;
   int found = 0;
	int index;
	int fd_envia;
   char nome[20];
   strcpy(nome, username);
   IDENTIFICADOR id;
   FEEDBACK feedback;
    
    for (index = 0; index < MAX_CLIENTES; index++) {
        if (strcmp(serverData->usernames[index], username) == 0) {
            found = 1;
            printf("Remover %s...\n", nome);
            fflush(stdout);
            pid = serverData->pids[index];
            strcpy(serverData->usernames[index],"\0");
            serverData->pids[index] = 0;

            if (flag == 0){ //so se tiver a terminar ordeiramente
               id.tipo = 5;
               sprintf(CLIENT_FIFO_FINAL, CLIENT_FIFO, pid);
               fd_envia = open(CLIENT_FIFO_FINAL, O_WRONLY);
               if (fd_envia != -1) {
                  if (write(fd_envia, &id, sizeof(id)) == -1) {
                     printf("Falha a escrever ao cliente");
                  }
                  close(fd_envia); 
               }
               else {
                  printf("Falha a abrir o pipe para escrita");
               }
            }

            for (int j = index; j < 9; j++) {
                strcpy(serverData->usernames[j], serverData->usernames[j + 1]);
                serverData->pids[j] = serverData->pids[j + 1];
            }
            strcpy(serverData->usernames[9], "\0");
            serverData->pids[9] = 0;
            serverData->numCli--;
            break;
        }
    }

    if (found == 0){
      printf("\nUsername nao esta registado!\n");
    }
    else{
      for (int i = 0; i < MAX_TOPICOS; i++) {
        for (int j = 0; j < serverData->topicos[i].numClientes; j++) {
            if (serverData->topicos[i].pid_clientes[j] == pid) {
                for (int k = j; k < serverData->topicos[i].numClientes - 1; k++) {
                    serverData->topicos[i].pid_clientes[k] = serverData->topicos[i].pid_clientes[k + 1];
                }
                serverData->topicos[i].numClientes--;
                break;
            }
        }
      }
   
      for (int i = 0; i < serverData->numCli; i++) {
         if (serverData->pids[i] > 0) {
            id.tipo = 3;
               snprintf(feedback.msg_devolucao, sizeof(feedback.msg_devolucao), "\nCliente [%s] desconectado!\n", nome);
               sprintf(CLIENT_FIFO_FINAL, CLIENT_FIFO, serverData->pids[i]);

               fd_envia = open(CLIENT_FIFO_FINAL, O_WRONLY);
               if (fd_envia != -1) {
                  if (write(fd_envia, &id, sizeof(id)) == -1) {
                     printf("Falha a escrever ao cliente");
                  }

                  if (write(fd_envia, &feedback, sizeof(feedback)) == -1) {
                     printf("Falha a escrever ao cliente");
                  }
                  close(fd_envia);
               } 
               else {
                  printf("Falha a abrir o pipe para escrita");
               }
         }
      
      }

    }
}

void unsubscribe(SUBSCRIBE* sub, ServerData* ServerData){
   int alreadySubscribed = 0; 
   int fd_envia;
   int index;
   FEEDBACK feedback;
   IDENTIFICADOR id;

    for (int i = 0; i < 20; i++) {
      if (strcmp(sub->topico, ServerData->topicos[i].nome_topico) == 0){
         for (int j = 0; j < ServerData->topicos[i].numClientes; j++) {
               if (ServerData->topicos[i].pid_clientes[j] == sub->pid) {
                  alreadySubscribed = 1; 
                  for (int k = j; k < ServerData->topicos[i].numClientes; k++){
                        ServerData->topicos[i].pid_clientes[k] = ServerData->topicos[i].pid_clientes[k+1];
                  }  
                  ServerData->topicos[i].numClientes--;    
               }
         }
         break;
      }
   }
   if (alreadySubscribed == 0){
      id.tipo = 3;
      strcpy(feedback.msg_devolucao, "Nao estava subscrito no topico!");

   }
   else{
      id.tipo = 3;
      strcpy(feedback.msg_devolucao, "Subscricao retirada com sucesso!");
   }

    sprintf(CLIENT_FIFO_FINAL, CLIENT_FIFO, sub->pid);

      fd_envia = open(CLIENT_FIFO_FINAL, O_WRONLY);
      if (fd_envia != -1) {
         if (write(fd_envia, &id, sizeof(id)) == -1) {
            printf("Falha a escrever ao cliente");
         }
         if (write(fd_envia, &feedback, sizeof(feedback)) == -1) {
            printf("Falha a escrever ao cliente");
         }
         close(fd_envia);
      } 
      else {
         printf("Falha a abrir o pipe para escrita");
      }
}

void encerraTodosClientes(ServerData* serverData){
   int fd_envia;
   IDENTIFICADOR id;
   for (int i = 0; i < serverData->numCli; i++) {
        if (serverData->pids[i] > 0) {
         id.tipo = 4;
            sprintf(CLIENT_FIFO_FINAL, CLIENT_FIFO, serverData->pids[i]);

            fd_envia = open(CLIENT_FIFO_FINAL, O_WRONLY);
            if (fd_envia != -1) {
                if (write(fd_envia, &id, sizeof(id)) == -1) {
                    printf("Falha a escrever ao cliente");
                }
                close(fd_envia);
            } 
            else {
                printf("Falha a abrir o pipe para escrita");
            }
        }
    
   }
}

void guardaPersistentesFicheiro(ServerData* serverData) {
    char *nome_ficheiro = getenv("MSG_FICH");
    if (nome_ficheiro == NULL) {
        fprintf(stderr, "Erro: Variável de ambiente MSG_FICH não definida.\n");
        return;
    }

    FILE *f = fopen(nome_ficheiro, "w");
    if (f == NULL) {
        perror("Erro ao abrir o ficheiro para escrita");
        return;
    }

    for (int i = 0; i < serverData->numTopicos; i++) {
      for (int j = 0; j < serverData->topicos[i].numPersistentes; j++){
         if (serverData->topicos[i].tempo[j] > 0) {
            fprintf(f, "%s %s %d %s\n",
                    serverData->topicos[i].nome_topico,
                    serverData->topicos[i].usernames[j],
                    serverData->topicos[i].tempo[j],
                    serverData->topicos[i].msg_persistentes[j]);
        }
      }    
    }
    fclose(f);
    printf("Mensagens persistentes guardadas em %s.\n", nome_ficheiro);
}

void recuperaPersistentesFicheiro(ServerData* serverData) {
    char *nome_ficheiro = getenv("MSG_FICH");
    if (nome_ficheiro == NULL) {
        fprintf(stderr, "Erro: Variável de ambiente MSG_FICH não definida.\n");
        kill(getpid(), SIGINT);
        return;
    }

    FILE *f = fopen(nome_ficheiro, "r");
    if (f == NULL) {
        perror("Erro ao abrir o ficheiro para leitura");
        return;
    }

    char linha[350];
    while (fgets(linha, 350, f) != NULL) {
        char nome_topico[20];
        char username[20];
        int tempo_vida;
        char corpo_mensagem[300];

        char *ptr = linha;

        char *espaco = strchr(ptr, ' ');
        if (espaco == NULL) continue; // Linha mal formatada
        *espaco = '\0';
        strncpy(nome_topico, ptr, sizeof(nome_topico));
        ptr = espaco + 1;

        espaco = strchr(ptr, ' ');
        if (espaco == NULL) continue;
        *espaco = '\0';
        strncpy(username, ptr, sizeof(username));
        ptr = espaco + 1;

        espaco = strchr(ptr, ' ');
        if (espaco == NULL) continue;
        *espaco = '\0';
        tempo_vida = atoi(ptr);
        ptr = espaco + 1;

        // O restante é o <corpo da mensagem>
        strncpy(corpo_mensagem, ptr, sizeof(corpo_mensagem));
        corpo_mensagem[strcspn(corpo_mensagem, "\n")] = '\0'; // Remover o '\n'

        int topico_index = -1;
        for (int i = 0; i < serverData->numTopicos; i++) {
            if (strcmp(serverData->topicos[i].nome_topico, nome_topico) == 0) {
                topico_index = i;
                break;
            }
        }

        if (topico_index == -1 && serverData->numTopicos < MAX_TOPICOS) {
            topico_index = serverData->numTopicos++;
            strncpy(serverData->topicos[topico_index].nome_topico, nome_topico, sizeof(serverData->topicos[topico_index].nome_topico));
            serverData->topicos[topico_index].numPersistentes = 0;
        }

        if (topico_index != -1) {
            int mensagem_index = serverData->topicos[topico_index].numPersistentes;
            if (mensagem_index < 300) {
                strncpy(serverData->topicos[topico_index].usernames[mensagem_index], username, sizeof(serverData->topicos[topico_index].usernames[mensagem_index]));
                serverData->topicos[topico_index].tempo[mensagem_index] = tempo_vida;
                strncpy(serverData->topicos[topico_index].msg_persistentes[mensagem_index], corpo_mensagem, sizeof(serverData->topicos[topico_index].msg_persistentes[mensagem_index]));
                serverData->topicos[topico_index].numPersistentes++;
            } else {
                fprintf(stderr, "Número máximo de mensagens persistentes excedido no tópico '%s'.\n", nome_topico);
            }
        }
    }

    fclose(f);
    printf("Mensagens persistentes recuperadas de %s.\n", nome_ficheiro);
}

void mostraTopicos(ServerData* serverData, char nomeTopico[20]){
   if (strcmp(nomeTopico, "\0") == 0){
      printf("\n------------------- TOPICOS ------------------- \n");
      for (int i = 0; i < serverData->numTopicos; i++) {
         printf("Topico %d - '%s'\nClientes subscritos: %d -> ", i, serverData->topicos[i].nome_topico, serverData->topicos[i].numClientes);
         for (int j = 0; j < serverData->topicos[i].numClientes; j++) {
            printf("[%d] ", serverData->topicos[i].pid_clientes[j]);
         }
         if (serverData->topicos[i].bloqueado == 0){printf("\nEstado: desbloquado");}
         else{printf("\nEstado: bloqueado");}
         printf("\nNumero de msg persistentes no topico = %d\n", serverData->topicos[i].numPersistentes);
         for (int k = 0; k <5; k++){
            printf("msg %d: {%s}, username {%s}, tempo = {%d}\n", k+1,serverData->topicos[i].msg_persistentes[k], serverData->topicos[i].usernames[k], serverData->topicos[i].tempo[k]);
         }
         printf("\n");
      }
      printf("-----------------------------------------------\n");
   }
   else{
      for (int i = 0; i < serverData->numTopicos; i++) {
         if (strcmp(serverData->topicos[i].nome_topico, nomeTopico) == 0){
            printf ("\nTOPICO %s:\nClientes subscritos: %d -> ", serverData->topicos[i].nome_topico, serverData->topicos[i].numClientes);
            for (int j = 0; j < serverData->topicos[i].numClientes; j++) {
            printf("[%d] ", serverData->topicos[i].pid_clientes[j]);
         }
         if (serverData->topicos[i].bloqueado == 0){printf("\nEstado: desbloquado");}
         else{printf("\nEstado: bloqueado");}
         printf("\nNumero de msg persistentes no topico = %d\n", serverData->topicos[i].numPersistentes);
         for (int k = 0; k <5; k++){
            printf("msg %d: {%s}, username {%s}, tempo = {%d}\n", k+1,serverData->topicos[i].msg_persistentes[k], serverData->topicos[i].usernames[k], serverData->topicos[i].tempo[k]);
         }
         }
      }

   }
}


