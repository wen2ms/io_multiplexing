#include <arpa/inet.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>

typedef struct SocketInfo {
    int file_descriptor;
    int epoll_file_descriptor;
} SocketInfo;

void* accept_connection(void* arg) {
    printf("accept_connection tid: %ld\n", pthread_self());

    SocketInfo* socket_info = (SocketInfo*)arg;

    int communication_file_descriptor = accept(socket_info->file_descriptor, NULL, NULL);

    int flag = fcntl(communication_file_descriptor, F_GETFL);
    flag |= O_NONBLOCK;
    fcntl(communication_file_descriptor, F_SETFL, flag);

    struct epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = communication_file_descriptor;
    epoll_ctl(socket_info->epoll_file_descriptor, EPOLL_CTL_ADD, communication_file_descriptor, &event);

    free(socket_info);
    return NULL;
}

void* communication(void* arg) {
    printf("communication tid: %ld\n", pthread_self());

    SocketInfo* socket_info = (SocketInfo*)arg;

    char buffer[5] = {0};

    char full_package[1024];
    memset(full_package, 0, sizeof(full_package));

    while (1) {
        int receive_data_len = recv(socket_info->file_descriptor, buffer, sizeof(buffer), 0);

        if (receive_data_len > 0) {
            for (int i = 0; i < receive_data_len; ++i) {
                buffer[i] = toupper(buffer[i]);
            }
            strncat(full_package + strlen(full_package), buffer, receive_data_len);

            write(STDOUT_FILENO, buffer, receive_data_len);
        } else if (receive_data_len == 0) {
            printf("client disconnected...\n");

            epoll_ctl(socket_info->epoll_file_descriptor, EPOLL_CTL_DEL, socket_info->file_descriptor, NULL);

            close(socket_info->file_descriptor);

            break;
        } else {
            if (errno == EAGAIN) {
                printf("data received completed...\n");

                send(socket_info->file_descriptor, full_package, strlen(full_package) + 1, 0);

                break;
            } else {
                perror("recv");

                break;
            }
        }
    }

    free(socket_info);
    return NULL;
}

int main() {
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

    int epoll_file_descriptor = epoll_create(1);
    if (epoll_file_descriptor == -1) {
        perror("epoll_create");
        return -1;
    }

    struct epoll_event event;
 
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = listen_file_descriptor;
    epoll_ctl(epoll_file_descriptor, EPOLL_CTL_ADD, listen_file_descriptor, &event);

    struct epoll_event events[1024];
    int size = sizeof(events) / sizeof(events[0]);

    while (1) {
        int ready_num = epoll_wait(epoll_file_descriptor, events, size, -1);
        printf("ready_num = %d\n", ready_num);

        pthread_t tid;
        for (int i = 0; i < ready_num; ++i) {
            int file_descriptor = events[i].data.fd;

            SocketInfo* socket_info = (SocketInfo*)malloc(sizeof(SocketInfo));
            socket_info->epoll_file_descriptor = epoll_file_descriptor;
            socket_info->file_descriptor = file_descriptor;

            if (file_descriptor == listen_file_descriptor) {
                pthread_create(&tid, NULL, accept_connection, socket_info);
                pthread_detach(tid);
            } else {
                pthread_create(&tid, NULL, communication, socket_info);
                pthread_detach(tid);
            }
        }
    }

    close(listen_file_descriptor);

    return 0;
}