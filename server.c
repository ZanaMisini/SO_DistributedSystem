#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <pthread.h>

#define MAX_BUFFER_SIZE 256
#define MAX_CLIENTS 5

typedef struct {
    long mesg_type;
    pid_t sender_pid;
    char mesg_text[MAX_BUFFER_SIZE];
} message_t;

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int msqid;
    int connected_clients;
    int client_counter;
} shared_data_t;

void error(const char *msg) {
    perror(msg);
    exit(1);
}

void* clientThread(void* arg) {
    shared_data_t* shared_data = (shared_data_t*)arg;
    message_t message;

    // Send connection status to server
    pthread_mutex_lock(&shared_data->mutex);
    int client_id = shared_data->client_counter++;
    while (shared_data->connected_clients >= MAX_CLIENTS) {
        pthread_cond_wait(&shared_data->cond, &shared_data->mutex);
    }
    shared_data->connected_clients++;
    pthread_mutex_unlock(&shared_data->mutex);

    // Client loop
    while (1) {
        // Receive message from clients
        if (msgrcv(shared_data->msqid, &message, sizeof(message), client_id + 1, 0) < 0) {
            error("Error receiving message from client");
        }

        // Process and display the received message
        printf("Client message from PID %d: %s", message.sender_pid, message.mesg_text);

        if (strcmp(message.mesg_text, "Exit\n") == 0) {
            break;
        }
    }

    // Decrement connected_clients count and signal server
    pthread_mutex_lock(&shared_data->mutex);
    shared_data->connected_clients--;
    pthread_cond_signal(&shared_data->cond);
    pthread_mutex_unlock(&shared_data->mutex);

    // Destroy message queue if all clients have disconnected
    if (shared_data->connected_clients == 0) {
        if (msgctl(shared_data->msqid, IPC_RMID, NULL) < 0) {
            error("Error removing message queue");
        }
    }

    pthread_exit(NULL);
}

int main() {
    int msqid;
    key_t key = ftok(".", 's');
    shared_data_t shared_data;
    pthread_t clientThreadHandles[MAX_CLIENTS];

    // Connect to message queue
    msqid = msgget(key, 0666 | IPC_CREAT);
    if (msqid < 0) {
        error("Error connecting to message queue");
    }

    // Initialize mutex and condition variable
    if (pthread_mutex_init(&shared_data.mutex, NULL) != 0) {
        error("Error initializing mutex");
    }
    if (pthread_cond_init(&shared_data.cond, NULL) != 0) {
        error("Error initializing condition variable");
    }

    shared_data.msqid = msqid;
    shared_data.connected_clients = 0;
    shared_data.client_counter = 0;

    // Create client threads
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (pthread_create(&clientThreadHandles[i], NULL, clientThread, (void*)&shared_data) != 0) {
            error("Error creating client thread");
        }
    }

    // Wait for the client threads to finish
    for (int i = 0; i < MAX_CLIENTS; i++) {
        pthread_join(clientThreadHandles[i], NULL);
    }

    // Destroy mutex and condition variable
    pthread_mutex_destroy(&shared_data.mutex);
    pthread_cond_destroy(&shared_data.cond);

    return 0;
}
