/**
 * webserver.c -- A webserver written in C
 * 
 * Test with curl (if you don't have it, install it):
 * 
 *    curl -D - http://localhost:3490/
 *    curl -D - http://localhost:3490/d20
 *    curl -D - http://localhost:3490/date
 * 
 * You can also test the above URLs in your browser! They should work!
 * 
 * Posting Data:
 * 
 *    curl -D - -X POST -H 'Content-Type: text/plain' -d 'Hello, sample data!' http://localhost:3490/save
 * 
 * (Posting data is harder to test from a browser.)
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/file.h>
#include <fcntl.h>
#include "threadpool.h"
#include "net.h"
#include "file.h"
#include "mime.h"
#include "cache.h"

#define PORT "3490"  // the port users will be connecting to

#define SERVER_FILES "./serverfiles"
#define SERVER_ROOT "./serverroot"
#define DEFAULT_PAGE "./serverroot/index.html"
/* 目前只处理两种请求，request和post */
#define REQUEST_NUM 2

typedef struct {
    int fd;
    cache *cache;
} handle_request_args;
/**
 * Send an HTTP response
 *
 * header:       "HTTP/1.1 404 NOT FOUND" or "HTTP/1.1 200 OK", etc.
 * content_type: "text/plain", etc.
 * body:         the data to send.
 * 
 * Return the value from the send() function.
 */
int send_response(int fd, char *header, char *content_type, void *body, int content_length)
{
    const int max_response_size = 262144;
    char response[max_response_size];
    const int max_date_lenth = 40;
    // Build HTTP response and store it in response
    char date[max_date_lenth] = {0};
    time_t t = time(NULL);
    struct tm tm = *localtime((const time_t *)&t);
    (void)sprintf(date, "Date: Month:%d Day:%d %02d:%02d:%02d %d", tm.tm_mon + 1,
                  tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_year + 1);
    char connection[] = "Connection: close";
    char content_len[20] = {0};
    (void)sprintf(content_len, "Content-Length:%d", strlen((const char *)body) + 1);
    int response_length = sprintf(response, "%s\n%s\n%s\n%s\n%s\n\n%s",
        header, date, connection, content_len, content_type, (char *)body);
    if (response_length <= 0) {
        perror("generate response message failed");
        return -1;
    }
    response_length += 1;

    // Send it all!
    int rv = send(fd, response, response_length, 0);

    if (rv < 0) {
        perror("send");
    }

    return rv;
}

int itoa(int num, char *buffer, int butter_len)
{
    int i = 0;
    while (num != 0) {
        char tmp = num % 10;
        if (i >= butter_len - 1) {
            return -1;
        }
        buffer[i++] = tmp + 48;
        num /= 10;
    }
	buffer[i] = '\n';
    for (int j = 0; j < (i >> 1); j++) {
		char k = buffer[j];
		buffer[j] = buffer[i - 1 -j];
		buffer[i - 1 -j] = k;
	}
	return 0;
}

/**
 * Send a /d20 endpoint response
 */
void get_d20(int fd)
{
    // Generate a random number between 1 and 20 inclusive
    srand((unsigned)time(NULL));
    int random_num = rand() % 20 + 1;
    char random_num_str[3] = {0};

    (void)itoa(random_num, random_num_str, 3);
    send_response(fd, "HTTP/1.1 200 OK", "text/plain", random_num_str, sizeof(random_num_str));
    return;
}

/**
 * Send a 404 response
 */
void resp_404(int fd)
{
    char filepath[4096];
    file_data *filedata; 
    char *mime_type;

    // Fetch the 404.html file
    snprintf(filepath, sizeof filepath, "%s/404.html", SERVER_FILES);
    filedata = file_load(filepath);

    if (filedata == NULL) {
        // TODO: make this non-fatal
        fprintf(stderr, "cannot find system 404 file\n");
        exit(3);
    }

    mime_type = mime_type_get(filepath);

    send_response(fd, "HTTP/1.1 404 NOT FOUND", mime_type, filedata->data, filedata->size);

    file_free(filedata);
}

/**
 * Read and return a file from disk or cache
 */
void get_file(int fd, cache *cache, char *request_path)
{
    cache_entry *entry = cache_get(cache, request_path);
    char *real_path = request_path;
    if (entry == NULL) {
        file_data *file = file_load(request_path);
        if (file == NULL) {
            file = file_load(DEFAULT_PAGE);
            real_path = DEFAULT_PAGE;
        }
        char *content_type = mime_type_get(real_path);
        cache_put(cache, real_path, content_type, file->data, file->size);
        send_response(fd, "HTTP/1.1 200 OK", content_type, file->data, file->size);
        return;
    }
    send_response(fd, "HTTP/1.1 200 OK", entry->content_type, entry->content,
                  entry->content_length);
    return;
}

/**
 * Search for the end of the HTTP header
 * 
 * "Newlines" in HTTP can be \r\n (carriage return followed by newline) or \n
 * (newline) or \r (carriage return).
 */
char *find_start_of_body(char *header)
{
    ///////////////////
    // IMPLEMENT ME! // (Stretch)
    ///////////////////
}

/**
 * Handle HTTP request and send response
 */

void handle_http_request(void *args)
{
    const int request_buffer_size = 65536; // 64K
    char request[request_buffer_size];
    handle_request_args *args_real = (handle_request_args *)args;
    int fd = args_real->fd;
    cache *cache = args_real->cache;
    // Read request
    int bytes_recvd = recv(fd, request, request_buffer_size - 1, 0);
    printf("recv fd%d bytes_recvd = %d\n", fd, bytes_recvd);
    if (bytes_recvd < 0) {
        perror("recv");
        return;
    }

    char request_type[5] = {0};
    char request_file[200] = {0};
    (void)sscanf(request, "%s %s", request_type, request_file);
    for (int i = 0; i < REQUEST_NUM; i++) {
        if (strncmp(request_type, "GET", strlen(request_type)) == 0) {
            if ((strlen(request_file) == strlen("/d20")) &&
                (strncmp(request_file, "/d20", strlen(request_file)) == 0)) {
                get_d20(fd);
                printf("--------------------------------------\n");
                printf("close fd : %d\n", fd);
                printf("finish task\nrequest type: %s\nrequest file: %s\n", request_type,
                                                                            request_file);
                printf("--------------------------------------\n");
                close(fd);
                return;
            } else {
                get_file(fd, cache, request_file);
                printf("--------------------------------------\n");
                printf("close fd : %d\n", fd);
                printf("finish task\nrequest type: %s\nrequest file: %s\n", request_type,
                                                                            request_file);
                printf("--------------------------------------\n");
                close(fd);
                return;
            }
        }
        if (strncmp(request_type, "POST", strlen(request_type)) == 0) {
            /* POST处理 */
        }
    }
    // Read the first two components of the first line of the request 
 
    // If GET, handle the get endpoints

    //    Check if it's /d20 and handle that special case
    //    Otherwise serve the requested file by calling get_file()


    // (Stretch) If POST, handle the post request
}

/**
 * Main
 */
int main(void)
{
    int newfd;  // listen on sock_fd, new connection on newfd
    struct sockaddr_storage their_addr; // connector's address information
    char s[INET6_ADDRSTRLEN];

    cache *cache = cache_create(10, 0);
    printf("--------------------------------------\n");
    thread_pool *threadpool = create_threadpool(10);
    printf("--------------------------------------\n");
    // Get a listening socket
    int listenfd = get_listener_socket(PORT);

    if (listenfd < 0) {
        fprintf(stderr, "webserver: fatal error getting listening socket\n");
        exit(1);
    }

    printf("webserver: waiting for connections on port %s...\n", PORT);

    // This is the main loop that accepts incoming connections and
    // responds to the request. The main parent process
    // then goes back to waiting for new connections.
    int add_task_res = 0;
    while(1) {
        socklen_t sin_size = sizeof their_addr;

        // Parent process will block on the accept() call until someone
        // makes a new connection:
        newfd = accept(listenfd, (struct sockaddr *)&their_addr, &sin_size);
        if (newfd == -1) {
            perror("accept");
            continue;
        }
        printf("--------------------------------------\n");
        // Print out a message that we got the connection
        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
        printf("fd is %d\nserver: got connection from %s\n", newfd, s);
        
        // newfd is a new socket descriptor for the new connection.
        // listenfd is still listening for new connections.
        tpool_task task;
        task.task_routine = (void *)handle_http_request;
        handle_request_args args = {
            newfd, cache
        };
        task.args = (void *)&args;
        add_task_res = add_task_in_threadpool(threadpool, &task);
    }

    // Unreachable code

    return 0;
}