#include <arpa/inet.h>
#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

pthread_mutex_t mutex;

typedef struct FileDescriptorInfo {
    int file_descriptor;
    int* max_file_descriptor;
    fd_set* read_set;
} FileDescriptorInfo;

void* accept_connection(void* arg) {
    printf("accepting thread id: %p\n", pthread_self());

    FileDescriptorInfo* file_descriptor_info = (FileDescriptorInfo*)arg;

    int communication_file_descriptor = accept(file_descriptor_info->file_descriptor, NULL, NULL);

    FD_SET(communication_file_descriptor, file_descriptor_info->read_set);
    *file_descriptor_info->max_file_descriptor = communication_file_descriptor > *file_descriptor_info->max_file_descriptor
                                                     ? communication_file_descriptor
                                                     : *file_descriptor_info->max_file_descriptor;

    free(file_descriptor_info);

    return NULL;
}

void* communication(void* arg) {
    printf("communication thread id: %p\n", pthread_self());

    FileDescriptorInfo* file_descriptor_info = (FileDescriptorInfo*)arg;

    char buffer[1024] = {0};
    int receive_data_len = recv(file_descriptor_info->file_descriptor, buffer, sizeof(buffer), 0);

    if (receive_data_len > 0) {
        printf("client says before processing: %s\n", buffer);

        for (int i = 0; i < receive_data_len; ++i) {
            buffer[i] = toupper(buffer[i]);
        }

        printf("client says after processing: %s\n", buffer);

        int ret = send(file_descriptor_info->file_descriptor, buffer, receive_data_len, 0);
        if (ret == -1) {
            perror("send");
        }
    } else if (receive_data_len == 0) {
        printf("client disconnected...\n");

        FD_CLR(file_descriptor_info->file_descriptor, file_descriptor_info->read_set);
        close(file_descriptor_info->file_descriptor);
    } else {
        perror("recv");
    }

    free(file_descriptor_info);

    return NULL;
}

int main() {
    pthread_mutex_init(&mutex, NULL);

    int listen_file_descriptor = socket(AF_INET, SOCK_STREAM, 0);

    if (listen_file_descriptor == -1) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in server_address;

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(9999);
    server_address.sin_addr.s_addr = INADDR_ANY;

    int ret = bind(listen_file_descriptor, (struct sockaddr*)&server_address, sizeof(server_address));

    if (ret == -1) {
        perror("bind");
        return -1;
    }

    ret = listen(listen_file_descriptor, 128);

    if (ret == -1) {
        perror("listen");
        return -1;
    }

    fd_set read_set;

    FD_ZERO(&read_set);
    FD_SET(listen_file_descriptor, &read_set);

    int max_file_descriptor = listen_file_descriptor;

    while (1) {
        fd_set temp_set = read_set;

        int ret = select(max_file_descriptor + 1, &temp_set, NULL, NULL, NULL);

        if (FD_ISSET(listen_file_descriptor, &temp_set)) {
            pthread_t tid;
            FileDescriptorInfo* file_descriptor_info = (FileDescriptorInfo*)malloc(sizeof(FileDescriptorInfo));

            file_descriptor_info->file_descriptor = listen_file_descriptor;
            file_descriptor_info->max_file_descriptor = &max_file_descriptor;
            file_descriptor_info->read_set = &read_set;

            pthread_create(&tid, NULL, accept_connection, file_descriptor_info);
            pthread_detach(tid);
        }

        for (int file_descriptor = 0; file_descriptor < max_file_descriptor + 1; ++file_descriptor) {
            if (file_descriptor != listen_file_descriptor && FD_ISSET(file_descriptor, &temp_set)) {
                pthread_t tid;
                FileDescriptorInfo* file_descriptor_info = (FileDescriptorInfo*)malloc(sizeof(FileDescriptorInfo));

                file_descriptor_info->file_descriptor = file_descriptor;
                file_descriptor_info->read_set = &read_set;

                pthread_create(&tid, NULL, communication, file_descriptor_info);
                pthread_detach(tid);
            }
        }
    }

    close(listen_file_descriptor);
    pthread_mutex_destroy(&mutex);

    return 0;
}