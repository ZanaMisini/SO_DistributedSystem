#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <pthread.h>

#define MAX_BUFFER_SIZE 256

typedef struct {
    long mesg_type;
    pid_t sender_pid;
    char mesg_text[MAX_BUFFER_SIZE];
} message_t;

typedef struct {
    pthread_mutex_t mutex;
    int msqid;
} shared_data_t;

void error(const char *msg) {
    perror(msg);
    exit(1);
}

void* clientThread(void* arg) {
    shared_data_t* shared_data = (shared_data_t*)arg;
    message_t message;

    // Client loop
    while (1) {
        // Send message to server
        printf("\nEnter text: ");
        fgets(message.mesg_text, MAX_BUFFER_SIZE, stdin);

        pthread_mutex_lock(&shared_data->mutex);

        message.mesg_type = 1;
        message.sender_pid = getpid();

        // Send message to server
        if (msgsnd(shared_data->msqid, &message, sizeof(message), 0) < 0) {
            error("Error sending message to server");
        }

        pthread_mutex_unlock(&shared_data->mutex);

        if (strcmp(message.mesg_text, "Exit\n") == 0) {
            break;
        }
    }

    pthread_exit(NULL);
}

int main() {
    int msqid;
    key_t key = ftok(".", 's');
    shared_data_t shared_data;
    pthread_t clientThreadHandle;

    // Connect to message queue
    msqid = msgget(key, 0666 | IPC_CREAT);
    if (msqid < 0) {
        error("Error connecting to message queue");
    }

    // Initialize mutex
    if (pthread_mutex_init(&shared_data.mutex, NULL) != 0) {
        error("Error initializing mutex");
    }

    shared_data.msqid = msqid;

    // Create client thread
    if (pthread_create(&clientThreadHandle, NULL, clientThread, (void*)&shared_data) != 0) {
        error("Error creating client thread");
    }

    // Wait for the client thread to finish
    pthread_join(clientThreadHandle, NULL);

    // Destroy mutex
    pthread_mutex_destroy(&shared_data.mutex);

    return 0;
}
