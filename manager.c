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

typedef struct {
    char nome_topico[20];
    int pid_clientes[10];
    int numClientes;
	 char msg_persistentes[5][300];
	 int tempo[5];
	 int numPersistentes;
} Topico;

#define MAX_TOPICOS 20
#define MAX_CLIENTES 10

typedef struct {
   Topico topicos[MAX_TOPICOS];
   int numTopicos;
   char usernames[MAX_CLIENTES][20];
	int pids[20];
	int numCli;
} ServerData;

void novoLogin(TUDOJUNTO* cont, ServerData *serverdata);
void analisaTopico(TUDOJUNTO* container, Topico* topicos, int* numTopicos);
void subscreveCliente(TUDOJUNTO* container, Topico* topicos, ServerData* serverData);
void distribuiMensagem(TUDOJUNTO* container, ServerData* serverData);
void apagaUsername(char username[20], ServerData* serverData, Topico* topicos);


void handler_sigalrm(int s) {
    unlink(MANAGER_FIFO);
    printf("\nServidor encerrado\n");
    exit(1);
}

void* processaNamedPipes(void* aux) {
    ServerData* serverData = (ServerData*)aux;
    TUDOJUNTO contentor;
	 int size;

    int fd_recebe = open(MANAGER_FIFO, O_RDWR);
    if (fd_recebe == -1) {
        perror("Erro ao abrir FIFO do manager para leitura");
        pthread_exit(NULL);
    }

    while (1) {
        size = read(fd_recebe, &contentor, sizeof(contentor));
        printf("\n--------Recebi algo de %d---------\n", contentor.pid);
        if (size > 0) {
            if (contentor.tipo == 1) {
               novoLogin(&contentor, serverData);
            } 
            else if (contentor.tipo == 2) {
               analisaTopico(&contentor, serverData->topicos, &serverData->numTopicos);
               subscreveCliente(&contentor, serverData->topicos, serverData);
               distribuiMensagem(&contentor, serverData);
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
					printf("\nANTES DO APAGA_USERNAME\n");
               apagaUsername(contentor.username, serverData, serverData->topicos);
					printf("\nSAI DO APAGA_USERNAME\n");
				}
				else{
					printf("\n\n\nWTF");
				}
        }
    }

    close(fd_recebe);
    unlink(MANAGER_FIFO);
    return NULL;
}

int main() {
    ServerData serverData = {0};
    pthread_t thr_pipes;

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
               printf("\n\n------------------- TOPICOS ------------------- \n");
                for (int i = 0; i < serverData.numTopicos; i++) {
                    printf("Topico %d - '%s' - Clientes: ", i, serverData.topicos[i].nome_topico);
                    for (int j = 0; j < serverData.topicos[i].numClientes; j++) {
                        printf("[%d] ", serverData.topicos[i].pid_clientes[j]);
                    }
                    printf("\n");
                }
                printf("-----------------------------------------------\n");
            } 
            else if (sscanf(buffer, "show %19s", nomeTopico) == 1) {
               
            } 
            else if (sscanf(buffer, "lock %19s", nomeTopico) == 1) {
               
            } 
            else if (strcmp(buffer, "close") == 0) {
               kill(getpid(), SIGINT);
               break;
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

// Implementação de novoLogin, analisaTopico, subscreveCliente, distribuiMensagem permanece igual



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

   printf("\nDEBUG - Enviar mensagem para %s\n", CLIENT_FIFO_FINAL);

   fd_envia = open(CLIENT_FIFO_FINAL, O_WRONLY);

   if (fd_envia != -1) {
      write(fd_envia, cont, sizeof(*cont)); // Envia a mensagem
      close(fd_envia);
   } 
   else {
      printf("Erro ao abrir FIFO para cliente %d\n", cont->pid);
   }

   printf("\nUsernames registados: ");
   for (int i = 0; i < 10; i++){
      printf("[%s] ", serverData->usernames[i]);
   }
   printf("\n");
	printf("PID's registados: ");
	for (int i = 0; i < 10; i++){
      printf("[%d] ", serverData->pids[i]);
   }

   fflush(stdout);
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
      printf("O tópico já existe");
    } 
    else if (free_slot != -1){ //Ha espaço para criar um novo topico
      strcpy(topicos[free_slot].nome_topico, container->topico);
      topicos[free_slot].pid_clientes[0] = container->pid;
      topicos[free_slot].numClientes = 1;
      (*numTopicos)++;
        
    } 
    else{
      printf("Capacidade máxima de tópicos atingida");
    }
}

void subscreveCliente(TUDOJUNTO* container, Topico* topicos, ServerData* serverData) {
   int found = 0; // Flag to track if the topic is found
   int alreadySubscribed = 0; // Flag to track if the client is already subscribed

   for (int i = 0; i < 20; i++) {
      // Verifica se o tópico corresponde
      if (strcmp(topicos[i].nome_topico, container->topico) == 0) {
         found = 1; // Mark that the topic is found

         // Verifica se o cliente já está inscrito
         for (int j = 0; j < topicos[i].numClientes; j++) {
            if (topicos[i].pid_clientes[j] == container->pid) {
               alreadySubscribed = 1; // Mark as already subscribed
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
         break; // Exit the loop since the topic was found
      }
   }

   if (!found) {
      strcpy(container->msg_devolucao, "Tópico não encontrado");
   }

   if (container->tipo == 4){
      sprintf(CLIENT_FIFO_FINAL, CLIENT_FIFO, container->pid);
      printf("\nDEBUG - Enviar mensagem para %s\n", CLIENT_FIFO_FINAL);

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



void distribuiMensagem(TUDOJUNTO* container, ServerData* serverData) {
   container->tipo = 2;
   int fd_envia;
   for (int i = 0; i < serverData->numTopicos; ++i) {
      if (strcmp(serverData->topicos[i].nome_topico, container->topico) == 0) {
         for (int j = 0; j < serverData->topicos[i].numClientes; ++j) {
            sprintf(CLIENT_FIFO_FINAL, CLIENT_FIFO, serverData->topicos[i].pid_clientes[j]);

            printf("\nDEBUG - Enviar mensagem para %s\n", CLIENT_FIFO_FINAL);

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
	printf("\n\nNUMERO DE CLIENTES = %d", serverData->numCli);
	printf("\nUsernames registados: ");
   for (int i = 0; i < 10; i++){
      printf("[%s] ", serverData->usernames[i]);
   }
   printf("\n");
	printf("PID's registados: ");
	for (int i = 0; i < 10; i++){
      printf("[%d] ", serverData->pids[i]);
   }

	printf("\nPID GUARDADO = %d | POSICAO DO ARRAY = %d |NAME = %s", pid, index, username);
   fflush(stdout);

    // Notify the client 
	 //Com o codigo abaixo, no segundo cliente que fizer exit, recebe valores random preenchidos no container
/* 
   if (pid > 0) {
    TUDOJUNTO container;
    container.tipo = 6;
    sprintf(CLIENT_FIFO_FINAL, CLIENT_FIFO, pid);

    printf("\nCLIENT_FIFO path: %s\n", CLIENT_FIFO_FINAL);

    fd_envia = open(CLIENT_FIFO_FINAL, O_WRONLY | O_NONBLOCK);
    if (fd_envia != -1) {
        printf("\nWriting to client...");
        if (write(fd_envia, &container, sizeof(container)) == -1) {
            perror("Failed to write to client FIFO");
        }
        close(fd_envia);
    } else {
        perror("Failed to open client FIFO");
    }

    // Notify other clients
    for (int i = 0; i < serverData->numCli; i++) {
        if (serverData->pids[i] > 0) {
            container.tipo = 7;
            snprintf(container.msg_devolucao, sizeof(container.msg_devolucao), "\nCliente [%s] desconectado!\n", username);

            sprintf(CLIENT_FIFO_FINAL, CLIENT_FIFO, serverData->pids[i]);
            printf("\nBroadcasting disconnection -- CLIENT_FIFO path: %s\n", CLIENT_FIFO_FINAL);

            fd_envia = open(CLIENT_FIFO_FINAL, O_WRONLY | O_NONBLOCK);
            if (fd_envia != -1) {
                if (write(fd_envia, &container, sizeof(container)) == -1) {
                    perror("Failed to broadcast to client FIFO");
                }
                close(fd_envia);
            } else {
                perror("Failed to open broadcast FIFO");
            }
        }
    }
}
*/

	
}



	//container.tipo = 7;

	//ServerData->pids
	// for (int i = 0; i < serverData->numCli; i++){
	// 	sprintf(CLIENT_FIFO_FINAL, CLIENT_FIFO, serverData->pids[i]);

	// 	printf("\nDEBUG - Enviar mensagem para %s\n", CLIENT_FIFO_FINAL);

	// 	int fd_envia = open(CLIENT_FIFO_FINAL, O_WRONLY);

	// 	if (fd_envia != -1) {
	// 		write(fd_envia, &container, sizeof(container)); // Envia a mensagem
	// 		close(fd_envia);
	// 	} 
	// 	else {
	// 		printf("Erro ao abrir FIFO para cliente %d\n", pid);
	// 	}

	// }




