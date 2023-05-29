#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <pthread.h>
#include <unistd.h>

#define MAX_BUFFER_SIZE 256
#define MAX_CLIENTS 2



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
int client =0;
    while (client < MAX_CLIENTS) {

        // Receive message from clients

        if (msgrcv(shared_data->msqid, &message, sizeof(message), client_id + 1, 0) < 0) {

            error("Error receiving message from client");

        }



        // Process and display the received message

        printf("Client message from PID %d: %s", message.sender_pid, message.mesg_text);



        if (strcmp(message.mesg_text, "Exit\n") == 0) {

            break;

        }



        // Get response from server

        char response[MAX_BUFFER_SIZE];

        printf("\nEnter response: ");

        fgets(response, sizeof(response), stdin);



        // Send response to client

        message.mesg_type = message.sender_pid;

        message.sender_pid = getpid();

        strncpy(message.mesg_text, response, sizeof(message.mesg_text));



        if (msgsnd(shared_data->msqid, &message, sizeof(message), 0) < 0) {

            error("Error sending message to client");

        }
        client++;

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