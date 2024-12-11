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

#define MANAGER_FIFO "MANAGER_FIFO"
#define CLIENT_FIFO "CLIENT_FIFO%d"
char CLIENT_FIFO_FINAL[100];




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

//thread para ir verificando se esta vivo o cliente

// read (fd,&tipo,sizeof(tipo);
// if tipo == 1
//   read (fd,&login,ds)


// typedef struct {
//     int tipo; //1
//     LOGIN login;
// } env1;

// typedef struct {
//     int tipo;
//     MSG msg;
// } env2;

// typedef struct {
//     int tipo;
//    ;
// } env1;

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
int analisaTopico(MSG* msg, Topico* topicos, int* numTopicos);
void subscreveCliente(MSG* msg, IDENTIFICADOR* id, SUBSCRIBE* sub, Topico* topicos, ServerData* serverData);
void guardaPersistentes(MSG* msg, ServerData* ServerData);
void distribuiMensagem(MSG* msg, ServerData* serverData);
void apagaUsername(char username[20], ServerData* serverData, Topico* topicos);
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

    while (serverData->lock == 0) {
        size = read(serverData->fd, &id, sizeof(id));
        if (size > 0) {
         switch(id.tipo){
            case 1: {
               printf("\nTipo == 1\n");
               LOGIN login;
               read(serverData->fd, &login, sizeof(login));
               novoLogin(&login, serverData);
               break;
            } 
            case 2: {
               printf("\nTipo == 2\n");
               MSG msg;
               SUBSCRIBE sub = {0};
               read(serverData->fd, &msg, sizeof(msg));
               int res;
               res = analisaTopico(&msg, serverData->topicos, &serverData->numTopicos);
               if (res == 0){
                  guardaPersistentes(&msg, serverData);
                  subscreveCliente(&msg, &id, &sub, serverData->topicos, serverData);
                  distribuiMensagem(&msg, serverData);
               }
               break;
            } 
            case 3: {
               printf("\nTipo == 3\n");
               char nome[20] = "\0";
               mostraTopicos(serverData, nome);
               break;
            }
            case 4: {
               printf("\nTipo == 4\n");
               MSG msg = {0};
               SUBSCRIBE sub;
               read(serverData->fd, &sub, sizeof(sub));
            	subscreveCliente(&msg, &id, &sub, serverData->topicos, serverData);
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
               apagaUsername(login.username, serverData, serverData->topicos);
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


int main() {
    pthread_mutex_t mutex; 
    pthread_mutex_init (&mutex,NULL); 
    ServerData serverData = {0};
    serverData.lock = 0;
    serverData.m = &mutex;
    pthread_t thr_pipes, thr_tempo, thr_alive;

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
    /*
    if (pthread_create(&thr_alive, NULL, verificaClientes, &serverData) != 0) {
        perror("Erro ao criar thread");
        return 1;
    }*/

    char buffer[50];
    char username[20];
    char nomeTopico[20];
    while (1) {
        
        if (fgets(buffer, sizeof(buffer), stdin)) {
            // Remove o '\n' do final da string (se houver)
            buffer[strcspn(buffer, "\n")] = 0;

            if (sscanf(buffer, "remove %19s", username) == 1) {
               apagaUsername(username, &serverData, serverData.topicos);
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

            else if (strcmp(buffer, "close") == 0) {
               serverData.lock = 1;
               guardaPersistentesFicheiro(&serverData);
               
               encerraTodosClientes(&serverData);
               kill(getpid(), SIGINT);
               break;
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
         } 
         else {
            printf("Comando inválido. Tente novamente.\n");
            continue;
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


int analisaTopico(MSG* msg, Topico* topicos, int* numTopicos) {
    int topic_exists = 0;
    int index;
    int free_slot = -1;
    int res = 0;
    int envia_msg = 0;
    FEEDBACK feedback;
    IDENTIFICADOR id;

    // Procura pelo tópico ou um slot livre
    for (int i = 0; i < 20; i++){
        if (strcmp(topicos[i].nome_topico, msg->topico) == 0) {
            topic_exists = 1;
            index = i;
            break;
        }
        if (topicos[i].nome_topico[0] == '\0' && free_slot == -1) 
            free_slot = i; 
    }

    if (topic_exists == 1){
      if(topicos[index].bloqueado == 1){
         strcpy(feedback.msg_devolucao, "Topico bloqueado pelo administrador!\n");
         envia_msg = 1;
         res = 1;
      }
    } 
    else if (free_slot != -1){ //Ha espaço para criar um novo topico
      strcpy(topicos[free_slot].nome_topico, msg->topico);
      topicos[free_slot].pid_clientes[0] = msg->pid;
      topicos[free_slot].numClientes = 1;
      topicos[free_slot].bloqueado = 0;
      (*numTopicos)++;  
    } 
    else{
      strcpy(feedback.msg_devolucao, "Capacidade máxima de tópicos atingida");
      res = 1;
      envia_msg = 1;
    }

      if(envia_msg == 1){
         id.tipo = 4;
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

void subscreveCliente(MSG* msg, IDENTIFICADOR* id, SUBSCRIBE* sub, Topico* topicos, ServerData* serverData) {
   int found = 0; 
   int alreadySubscribed = 0; 
   int fd_envia;
   int index;
   int PID;
   char utilizador[20];
   FEEDBACK feedback;

   //if para sub e outro if para msg???
   if (id->tipo == 2){
      PID = msg->pid;
      strcpy(utilizador, msg->username);
      for (int i = 0; i < 20; i++) {
      // Verifica se o tópico corresponde
         if (strcmp(topicos[i].nome_topico, msg->topico) == 0) {
            found = 1; 
            index = i;

            // Verifica se o cliente já está inscrito
            for (int j = 0; j < topicos[i].numClientes; j++) {
               if (topicos[i].pid_clientes[j] == msg->pid) {
                  alreadySubscribed = 1; 
                  break;
               }
            }
            if (alreadySubscribed == 1) {
               strcpy(feedback.msg_devolucao, "Já inscrito!");
               break;
            } 
            else{
               if (topicos[index].numClientes < 10) {
                  // Adiciona o cliente, se possível
                  topicos[index].pid_clientes[topicos[index].numClientes] = msg->pid;
                  topicos[index].numClientes++;
                  strcpy(feedback.msg_devolucao, "Subscrito com sucesso!");
               }
               else {
                  strcpy(feedback.msg_devolucao, "Máximo de clientes atingido para este tópico");
               }
            }
         }
      }
   }

   else{
      PID = sub->pid;
      strcpy(utilizador, sub->username);
      for (int i = 0; i < 20; i++) {
      // Verifica se o tópico corresponde
      if (strcmp(topicos[i].nome_topico, sub->topico) == 0) {
         found = 1; 
         index = i;

         // Verifica se o cliente já está inscrito
         for (int j = 0; j < topicos[i].numClientes; j++) {
            if (topicos[i].pid_clientes[j] == sub->pid) {
               alreadySubscribed = 1; 
               break;
            }
         }
         if (alreadySubscribed == 1) {
            strcpy(feedback.msg_devolucao, "Já inscrito!");
            break;
         } 
         else{
            if (topicos[index].numClientes < 10) {
               // Adiciona o cliente, se possível
               topicos[index].pid_clientes[topicos[index].numClientes] = sub->pid;
               topicos[index].numClientes++;
               strcpy(feedback.msg_devolucao, "Subscrito com sucesso!");
            }
            else {
               strcpy(feedback.msg_devolucao, "Máximo de clientes atingido para este tópico");
            }
         }
      }
   }

   }

   if (found == 0){
      strcpy(feedback.msg_devolucao, "Topico nao encontrado");
   }

   if (id->tipo == 4){ //Se o comando foi subscribe
      sprintf(CLIENT_FIFO_FINAL, CLIENT_FIFO, PID);

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


   if (topicos[index].numPersistentes > 0){
      for (int j = 0; j < topicos[index].numPersistentes; j++){
         id->tipo = 2;
         strcpy(msg->mensagem, topicos[index].msg_persistentes[j]);
         strcpy(msg->topico, topicos[index].nome_topico);
         strcpy(msg->username, topicos[index].usernames[j]);

         if(strcmp(msg->username, utilizador) == 0)
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


void guardaPersistentes(MSG* msg, ServerData* ServerData){
    int free_slot = -1;
    int index = -1;

    if (msg->duracao > 0) {
        // Percorrer os tópicos
        for (int i = 0; i < ServerData->numTopicos; i++) {
            if (ServerData->topicos[i].numPersistentes < 5) {
                if (strcmp(ServerData->topicos[i].nome_topico, msg->topico) == 0) {
                    index = i;
                    // Encontrar slot livre
                    for (int j = 0; j < 5; j++) {
                        if (ServerData->topicos[i].msg_persistentes[j][0] == '\0') {
                            free_slot = j;
                            break;
                        }
                    }
                    break;
                }
            }
        }

        if (free_slot == -1 || index == -1) {
            printf("\nSEM ESPACO PARA MSG PERSISTENTE OU TOPICO NAO ENCONTRADO!");
            return;
        }

        pthread_mutex_lock(ServerData->m);
        strcpy(ServerData->topicos[index].msg_persistentes[free_slot], msg->mensagem);
        strcpy(ServerData->topicos[index].usernames[free_slot], msg->username);
        ServerData->topicos[index].tempo[free_slot] = msg->duracao;
        ServerData->topicos[index].numPersistentes++;
        pthread_mutex_unlock(ServerData->m);
    } else {
        printf("\nDURACAO INVALIDA!");
    }
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

void apagaUsername(char username[20], ServerData* serverData, Topico* topicos) {
   int pid = -1;
	int index;
	int fd_envia;
   char nome[20];
   strcpy(nome, username);
   IDENTIFICADOR id;
   FEEDBACK feedback;

   printf("Remover %s...\n", nome);
   fflush(stdout);
    
    for (index = 0; index < MAX_CLIENTES; index++) {
        if (strcmp(serverData->usernames[index], username) == 0) {
            pid = serverData->pids[index];
            strcpy(serverData->usernames[index],"\0");
            serverData->pids[index] = 0;

            
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

    for (int i = 0; i < MAX_TOPICOS; i++) {
        for (int j = 0; j < topicos[i].numClientes; j++) {
            if (topicos[i].pid_clientes[j] == pid) {
                for (int k = j; k < topicos[i].numClientes - 1; k++) {
                    topicos[i].pid_clientes[k] = topicos[i].pid_clientes[k + 1];
                }
                topicos[i].numClientes--;
                break;
            }
        }
    }

    id.tipo = 7;
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
  


   
    for (int i = 0; i < serverData->numCli; i++) {
        if (serverData->pids[i] > 0) {
         id.tipo = 4;
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
      id.tipo = 4;
      strcpy(feedback.msg_devolucao, "Nao estava subscrito no topico!");

   }
   else{
      id.tipo = 4;
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
         id.tipo = 6;
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

        // Processar a linha manualmente
        char *ptr = linha;

        // Extrair <nome do tópico>
        char *espaco = strchr(ptr, ' ');
        if (espaco == NULL) continue; // Linha mal formatada
        *espaco = '\0';
        strncpy(nome_topico, ptr, sizeof(nome_topico));
        ptr = espaco + 1;

        // Extrair <username>
        espaco = strchr(ptr, ' ');
        if (espaco == NULL) continue;
        *espaco = '\0';
        strncpy(username, ptr, sizeof(username));
        ptr = espaco + 1;

        // Extrair <tempo de vida>
        espaco = strchr(ptr, ' ');
        if (espaco == NULL) continue;
        *espaco = '\0';
        tempo_vida = atoi(ptr);
        ptr = espaco + 1;

        // O restante é o <corpo da mensagem>
        strncpy(corpo_mensagem, ptr, sizeof(corpo_mensagem));
        corpo_mensagem[strcspn(corpo_mensagem, "\n")] = '\0'; // Remover o '\n'

        // Adicionar a mensagem ao serverData
        int topico_index = -1;
        for (int i = 0; i < serverData->numTopicos; i++) {
            if (strcmp(serverData->topicos[i].nome_topico, nome_topico) == 0) {
                topico_index = i;
                break;
            }
        }

        // Se o tópico não existir, criar um novo
        if (topico_index == -1 && serverData->numTopicos < MAX_TOPICOS) {
            topico_index = serverData->numTopicos++;
            strncpy(serverData->topicos[topico_index].nome_topico, nome_topico, sizeof(serverData->topicos[topico_index].nome_topico));
            serverData->topicos[topico_index].numPersistentes = 0;
        }

        // Adicionar a mensagem ao tópico encontrado/criado
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


