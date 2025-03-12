#include <arpa/inet.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>

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

        for (int i = 0; i < ready_num; ++i) {
            int file_descriptor = events[i].data.fd;

            if (file_descriptor == listen_file_descriptor) {
                int communication_file_descriptor = accept(listen_file_descriptor, NULL, NULL);

                int flag = fcntl(communication_file_descriptor, F_GETFL);
                flag |= O_NONBLOCK;
                fcntl(communication_file_descriptor, F_SETFL, flag);

                event.events = EPOLLIN | EPOLLET;
                event.data.fd = communication_file_descriptor;
                epoll_ctl(epoll_file_descriptor, EPOLL_CTL_ADD, communication_file_descriptor, &event);
            } else {
                char buffer[5] = {0};

                while (1) {
                    int receive_data_len = recv(file_descriptor, buffer, sizeof(buffer), 0);

                    if (receive_data_len > 0) {
                        printf("client says before processing: %s\n", buffer);
            
                        for (int i = 0; i < receive_data_len; ++i) {
                            buffer[i] = toupper(buffer[i]);
                        }
            
                        printf("client says after processing: %s\n", buffer);
            
                        ret = send(file_descriptor, buffer, receive_data_len, 0);
                        if (ret == -1) {
                            perror("send");
                            return -1;
                        }
                    } else if (receive_data_len == 0) {
                        printf("client disconnected...\n");
    
                        // delete first and then close
                        epoll_ctl(epoll_file_descriptor, EPOLL_CTL_DEL, file_descriptor, NULL);
    
                        close(file_descriptor);
    
                        break;
                    } else {
                        if (errno == EAGAIN) {
                            printf("data received completed...\n");
                            break;
                        } else {
                            perror("recv");

                            return -1;
                        }
                    }
                }
            }
        }
    }

    close(listen_file_descriptor);

    return 0;
}