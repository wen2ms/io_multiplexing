#include <arpa/inet.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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

    fd_set read_set;

    FD_ZERO(&read_set);
    FD_SET(listen_file_descriptor, &read_set);

    int max_file_descriptor = listen_file_descriptor;

    while (1) {
        fd_set temp_set = read_set;

        int ret = select(max_file_descriptor + 1, &temp_set, NULL, NULL, NULL);

        if (FD_ISSET(listen_file_descriptor, &temp_set)) {
            int communication_file_descriptor = accept(listen_file_descriptor, NULL, NULL);

            FD_SET(communication_file_descriptor, &read_set);
            max_file_descriptor =
                communication_file_descriptor > max_file_descriptor ? communication_file_descriptor : max_file_descriptor;
        }

        for (int file_descriptor = 0; file_descriptor < max_file_descriptor + 1; ++file_descriptor) {
            if (file_descriptor != listen_file_descriptor && FD_ISSET(file_descriptor, &temp_set)) {
                char buffer[1024] = {0};
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

                    FD_CLR(file_descriptor, &read_set);
                    close(file_descriptor);

                    break;
                } else {
                    perror("recv");
                    return -1;
                }
            }
        }
    }

    close(listen_file_descriptor);

    return 0;
}