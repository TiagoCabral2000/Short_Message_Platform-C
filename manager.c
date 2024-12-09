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
    int tipo, pid, resultado, duracao;
    char username[20], topico[20], mensagem[300], msg_devolucao[50];
} TUDOJUNTO;
TUDOJUNTO contentor;

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
} ServerData;

void novoLogin(TUDOJUNTO* cont, ServerData *serverdata);
int analisaTopico(TUDOJUNTO* container, Topico* topicos, int* numTopicos);
void subscreveCliente(TUDOJUNTO* container, Topico* topicos, ServerData* serverData);
void guardaPersistentes(TUDOJUNTO* container, ServerData* ServerData);
void distribuiMensagem(TUDOJUNTO* container, ServerData* serverData);
void apagaUsername(char username[20], ServerData* serverData, Topico* topicos);


void handler_sigalrm(int s) {
    unlink(MANAGER_FIFO);
    printf("\nServidor encerrado\n");
    exit(1);
}

void* processaNamedPipes(void* aux) {
    ServerData* serverData = (ServerData*)aux;
    //TUDOJUNTO contentor;
	 int size;

    int fd_recebe = open(MANAGER_FIFO, O_RDWR);
    if (fd_recebe == -1) {
        perror("Erro ao abrir FIFO do manager para leitura");
        pthread_exit(NULL);
    }

    while (serverData->lock == 0) {
        size = read(fd_recebe, &contentor, sizeof(contentor));
        
        if (size > 0) {
            if (contentor.tipo == 1) {
               novoLogin(&contentor, serverData);
            } 
            else if (contentor.tipo == 2) {
               int res;
               res = analisaTopico(&contentor, serverData->topicos, &serverData->numTopicos);
               if (res == 0){
                  subscreveCliente(&contentor, serverData->topicos, serverData);
                  guardaPersistentes(&contentor, serverData);
                  distribuiMensagem(&contentor, serverData);
               }
               else{printf("\nErro");}
            } 
            else if (contentor.tipo == 3) {
               printf("\n\n------------------- TOPICOS ------------------- \n");
                for (int i = 0; i < serverData->numTopicos; i++) {
                    printf("Topico %d - '%s' - Clientes: ", i, serverData->topicos[i].nome_topico);
                    for (int j = 0; j < serverData->topicos[i].numClientes; j++) {
                        printf("[%d] ", serverData->topicos[i].pid_clientes[j]);
                    }
                    printf("\n");
                }
                printf("-----------------------------------------------\n");
            }
            else if (contentor.tipo == 4){
            	subscreveCliente(&contentor, serverData->topicos, serverData);
            }
            else if(contentor.tipo == 5){
               printf("\nUNSUBSCRIBE ainda nao implementado");
            }
            else if(contentor.tipo == 6){
               apagaUsername(contentor.username, serverData, serverData->topicos);
				}
				else{
					printf("\nTipo n existe");
				}
        }
    }

    close(fd_recebe);
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
                  for (k=j; k < serverData->topicos[i].numPersistentes; k++){
                     serverData->topicos[i].tempo[k] = serverData->topicos[i].tempo[k+1];
                     strcpy(serverData->topicos[i].msg_persistentes[k], serverData->topicos[i].msg_persistentes[k+1]);
                  }
                  strcpy(serverData->topicos[i].msg_persistentes[k], "\0");
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
    pthread_t thr_pipes, thr_tempo;

    struct sigaction sa;
    sa.sa_handler = handler_sigalrm;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);

    if (mkfifo(MANAGER_FIFO, 0666) == -1 && errno != EEXIST) {
        perror("Erro ao criar FIFO do manager");
        return 1;
    }

    if (pthread_create(&thr_pipes, NULL, processaNamedPipes, &serverData) != 0) {
        perror("Erro ao criar thread");
        return 1;
    }
    if (pthread_create(&thr_tempo, NULL, descontaTempo, &serverData) != 0) {
        perror("Erro ao criar thread");
        return 1;
    }

    char buffer[50];
    char username[20];
    char nomeTopico[20];
    while (1) {
        
        if (fgets(buffer, sizeof(buffer), stdin)) {
            // Remove o '\n' do final da string (se houver)
            buffer[strcspn(buffer, "\n")] = 0;

            if (sscanf(buffer, "remove %19s", username) == 1) {
               apagaUsername(username, &serverData, serverData.topicos); // Enviar mensagem para tópico
            } 
            else if (strcmp(buffer, "topics") == 0) {
               printf("\n------------------- TOPICOS ------------------- \n");
                for (int i = 0; i < serverData.numTopicos; i++) {
                    printf("Topico %d - '%s' - Clientes: ", i, serverData.topicos[i].nome_topico);
                    for (int j = 0; j < serverData.topicos[i].numClientes; j++) {
                        printf("[%d] ", serverData.topicos[i].pid_clientes[j]);
                    }
                    if (serverData.topicos[i].bloqueado == 0){printf("\nEstado: desbloquado");}
                    else{printf("\nEstado: bloqueado");}
                    printf("\nNumero de msg persistentes no topico = %d\n", serverData.topicos[i].numPersistentes);
                    for (int k = 0; k <5; k++){
                     printf("msg %d: {%s}, tempo = {%d}\n", k+1,serverData.topicos[i].msg_persistentes[k], serverData.topicos[i].tempo[k]);
                    }
                    printf("\n");
                }
                printf("-----------------------------------------------\n");
            } 
            else if (sscanf(buffer, "show %19s", nomeTopico) == 1) {
               
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
            continue; // Espera uma nova entrada válida
         }

   }

   pthread_join(thr_pipes, NULL);
   return 0;
}

void novoLogin(TUDOJUNTO* cont, ServerData *serverData) {
   //cont->tipo = 1;
   int fd_envia;
   int username_exists = 0;
   int free_slot = -1;
   for (int i = 0; i < 10; i++){
      if (strcmp(serverData->usernames[i], cont->username) == 0) 
         username_exists = 1;
      if (serverData->usernames[i][0] == '\0' && free_slot == -1) 
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
      strcpy(serverData->usernames[free_slot], cont->username);
		serverData->pids[free_slot] = cont->pid;
		serverData->numCli++;
      cont->resultado = 1;
   }  

   sprintf(CLIENT_FIFO_FINAL, CLIENT_FIFO, cont->pid);

   printf("\nDEBUG - Enviar pelo named pipe %s\n", CLIENT_FIFO_FINAL);

   fd_envia = open(CLIENT_FIFO_FINAL, O_WRONLY);

   if (fd_envia != -1) {
      write(fd_envia, cont, sizeof(*cont)); // Envia a mensagem
      close(fd_envia);
   } 
   else {
      printf("Erro ao abrir FIFO para cliente %d\n", cont->pid);
   }
}


int analisaTopico(TUDOJUNTO* container, Topico* topicos, int* numTopicos) {
    int topic_exists = 0;
    int index;
    int free_slot = -1;

    // Procura pelo tópico ou um slot livre
    for (int i = 0; i < 20; i++){
        if (strcmp(topicos[i].nome_topico, container->topico) == 0) {
            topic_exists = 1;
            index = i;
            break;
        }
        if (topicos[i].nome_topico[0] == '\0' && free_slot == -1) 
            free_slot = i; 
    }

    if (topic_exists == 1){
      if(topicos[index].bloqueado == 1){
         printf("\nTopico bloqueado!\n");
         return 1;
      }
    } 
    else if (free_slot != -1){ //Ha espaço para criar um novo topico
      strcpy(topicos[free_slot].nome_topico, container->topico);
      topicos[free_slot].pid_clientes[0] = container->pid;
      topicos[free_slot].numClientes = 1;
      topicos[free_slot].bloqueado = 0;
      (*numTopicos)++;  
    } 
    else{
      printf("Capacidade máxima de tópicos atingida");
      return 1;
    }
    return 0;
}

void subscreveCliente(TUDOJUNTO* container, Topico* topicos, ServerData* serverData) {
   int found = 0; 
   int alreadySubscribed = 0; 

   for (int i = 0; i < 20; i++) {
      // Verifica se o tópico corresponde
      if (strcmp(topicos[i].nome_topico, container->topico) == 0) {
         found = 1; 

         // Verifica se o cliente já está inscrito
         for (int j = 0; j < topicos[i].numClientes; j++) {
            if (topicos[i].pid_clientes[j] == container->pid) {
               alreadySubscribed = 1; 
               break;
            }
         }

         if (alreadySubscribed) {
            strcpy(container->msg_devolucao, "Já inscrito!");
         } 
         else if (topicos[i].numClientes < 10) {
            // Adiciona o cliente, se possível
            topicos[i].pid_clientes[topicos[i].numClientes] = container->pid;
            topicos[i].numClientes++;
            strcpy(container->msg_devolucao, "Subscrito com sucesso!");
         } 
         else {
            strcpy(container->msg_devolucao, "Máximo de clientes atingido para este tópico");
         }
         break; 
      }
   }

   if (!found) {
      strcpy(container->msg_devolucao, "Tópico não encontrado");
   }

   if (container->tipo == 4){
      sprintf(CLIENT_FIFO_FINAL, CLIENT_FIFO, container->pid);
      printf("\nDEBUG - Enviar pelo named pipe %s\n", CLIENT_FIFO_FINAL);

      int fd_envia = open(CLIENT_FIFO_FINAL, O_WRONLY);
      if (fd_envia != -1) {
         write(fd_envia, container, sizeof(*container)); // Envia a mensagem
         close(fd_envia);
      } 
      else {
         printf("Erro ao abrir FIFO para cliente %d\n", container->pid);
      }
   } 
}

void guardaPersistentes(TUDOJUNTO* container, ServerData* ServerData){
   int free_slot = -1;
   int index = -1;
   if(container->duracao > 0){
      //percorrer o topico para verificar se ainda tenho espaco livre para adicionar mensagem persistente
      for (int i = 0; i < ServerData->numTopicos; i++){
         if(strcmp(ServerData->topicos[i].nome_topico, container->topico) == 0){
            index = i;
            for (int j = 0; j < 5; j++){
               if (strcmp(ServerData->topicos[i].msg_persistentes[j], "\0") == 0){
                  free_slot = j;
                  break;
               }
            }
            if(free_slot == -1 || index == -1){printf("\nSEM ESPACO PARA MSG PERSISTENTE OU TOPICO N ENCONTRAADO!");}
            break;
         }
      }
      pthread_mutex_lock(ServerData->m);
      strcpy(ServerData->topicos[index].msg_persistentes[free_slot], container->mensagem);
      ServerData->topicos[index].tempo[free_slot] = container->duracao;
      ServerData->topicos[index].numPersistentes++;
      pthread_mutex_unlock(ServerData->m);
   }
}


void distribuiMensagem(TUDOJUNTO* container, ServerData* serverData) {
   container->tipo = 2;
   int fd_envia;
   for (int i = 0; i < serverData->numTopicos; ++i) {
      if (strcmp(serverData->topicos[i].nome_topico, container->topico) == 0) {
         for (int j = 0; j < serverData->topicos[i].numClientes; ++j) {
            sprintf(CLIENT_FIFO_FINAL, CLIENT_FIFO, serverData->topicos[i].pid_clientes[j]);

            printf("\nDEBUG - Enviar pelo named pipe %s\n", CLIENT_FIFO_FINAL);

            fd_envia = open(CLIENT_FIFO_FINAL, O_WRONLY);

            if (fd_envia != -1) {
               write(fd_envia, container, sizeof(*container)); // Envia a mensagem
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

   printf("Remover %s...\n", nome);
   fflush(stdout);
    // Find and remove the username
    for (index = 0; index < MAX_CLIENTES; index++) {
        if (strcmp(serverData->usernames[index], username) == 0) {
            pid = serverData->pids[index];
            strcpy(serverData->usernames[index],"\0");
            serverData->pids[index] = 0;

            // Shift the remaining elements
            for (int j = index; j < 9; j++) {
                strcpy(serverData->usernames[j], serverData->usernames[j + 1]);
                serverData->pids[j] = serverData->pids[j + 1];
            }
            serverData->numCli--;
            break;
        }
    }

    // Remove the client from topics
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
	 //Com o codigo abaixo, no segundo cliente que fizer exit, recebe valores random preenchidos no container

if (pid > 0) {
    //TUDOJUNTO container;
    contentor.tipo = 6;
    sprintf(CLIENT_FIFO_FINAL, CLIENT_FIFO, pid);

    fd_envia = open(CLIENT_FIFO_FINAL, O_WRONLY);
    if (fd_envia == -1) {
        printf("Falha ao abrir o named pipe");
        return;
    }

    if (write(fd_envia, &contentor, sizeof(contentor)) == -1) {
        printf("Falha ao escrever no named pipe");
    }
    close(fd_envia);

   contentor.tipo = 7;
    for (int i = 0; i < serverData->numCli; i++) {
        if (serverData->pids[i] > 0) {
            snprintf(contentor.msg_devolucao, sizeof(contentor.msg_devolucao), "\nCliente [%s] desconectado!\n", nome);
            sprintf(CLIENT_FIFO_FINAL, CLIENT_FIFO, serverData->pids[i]);

            fd_envia = open(CLIENT_FIFO_FINAL, O_WRONLY);
            if (fd_envia != -1) {
                if (write(fd_envia, &contentor, sizeof(contentor)) == -1) {
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