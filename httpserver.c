// Amrita Pasupuleti
// CSE 130, asgn 4: multi-threaded http server
// httpserver.c
// creates a multi-threaded http server with get and put functions

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <regex.h>
#include <errno.h>
#include <sys/socket.h>
#include <pthread.h>

#include "asgn2_helper_funcs.h"
#include "debug.h"
#include "protocol.h"
#include "queue.h"
#include "hashtable.h"
#include "rwlock.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFF_SIZE 4096

#define PUT_200 "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nOK\n"
#define PUT_201 "HTTP/1.1 201 Created\r\nContent-Length: 8\r\n\r\nCreated\n"
#define GET_200 "HTTP/1.1 200 OK\r\nContent-Length: "

#define RES_400 "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n"
#define RES_403 "HTTP/1.1 403 Forbidden\r\nContent-Length: 12\r\n\r\nForbidden\n"
#define RES_404 "HTTP/1.1 404 Not Found\r\nContent-Length: 10\r\n\r\nNot Found\n"
#define RES_500                                                                                    \
    "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 22\r\n\r\nInternal Server Error\n"
#define RES_501 "HTTP/1.1 501 Not Implemented\r\nContent-Length: 16\r\n\r\nNot Implemented\n"
#define RES_505                                                                                    \
    "HTTP/1.1 505 Version Not Supported\r\nContent-Length: 22\r\n\r\nVersion Not Supported\n"

typedef struct Client {
    char method[8];
    char uri[64];
    char version[10];
    char *message;
    int content_length;
    int msg_length;
    int req_id;
    int bytes_read;
} Client;

typedef struct Params {
    regex_t req_line;
    regex_t header_field;
} Params;

queue_t *q;
pthread_mutex_t mutex;
hash_table_t *hashes;

void parse_headers(Client *C, char *pos, Params *re) {
    int offset = 0;
    regmatch_t header_match[4];
    for (;;) {
        if (regexec(&re->header_field, pos + offset, 4, header_match, 0) != 0) {
            break;
        }

        // get key
        char key[header_match[2].rm_eo - header_match[2].rm_so + 1];
        strncpy(key, pos + offset + header_match[2].rm_so,
            header_match[2].rm_eo - header_match[2].rm_so);
        key[header_match[2].rm_eo - header_match[2].rm_so] = '\0';

        // get value
        char value[header_match[3].rm_eo - header_match[3].rm_so + 1];
        strncpy(value, pos + offset + header_match[3].rm_so,
            header_match[3].rm_eo - header_match[3].rm_so);
        value[header_match[3].rm_eo - header_match[3].rm_so] = '\0';

        if (strncmp(key, "Request-Id", 10) == 0) {
            C->req_id = atoi(value);
        } else if (strncmp(key, "Content-Length", 14) == 0) {
            C->content_length = atoi(value);
        }
        // increment
        offset += header_match[0].rm_eo;
    }
}

void parse_input(Client *C, char *buffer, int connfd, Params *re) {
    C->bytes_read = read_until(connfd, buffer, MAX_HEADER_LENGTH, "\r\n\r\n");
    C->content_length = -1;
    C->req_id = 0;

    regmatch_t matches[10];
    regexec(&re->req_line, buffer, 10, matches, 0);

    // method
    strncpy(C->method, buffer + matches[1].rm_so, matches[1].rm_eo - matches[1].rm_so);
    C->method[matches[1].rm_eo - matches[1].rm_so] = '\0';
    // uri
    strncpy(C->uri, buffer + matches[2].rm_so, matches[2].rm_eo - matches[2].rm_so);
    C->uri[matches[2].rm_eo - matches[2].rm_so] = '\0';
    // version
    strncpy(C->version, buffer + matches[3].rm_so, matches[3].rm_eo - matches[3].rm_so);
    C->version[matches[3].rm_eo - matches[3].rm_so] = '\0';
    // check for correct method and version
    if ((strncmp(C->version, HTTP_VERSION_REGEX, 3) != 0)
        || ((strncmp(C->method, "GET", 3) != 0 && strncmp(C->method, "PUT", 3) != 0))) {
        write_n_bytes(connfd, RES_400, strlen(RES_400));
        fprintf(stderr, "%s,/%s,400,%d\n", C->method, C->uri, 0);
    }

    // parse headers
    char headers[matches[4].rm_eo - matches[4].rm_so + 1];
    strncpy(headers, buffer + matches[4].rm_so, matches[4].rm_eo - matches[4].rm_so);
    headers[matches[4].rm_eo - matches[4].rm_so] = '\0';
    parse_headers(C, headers, re);

    // get message
    C->msg_length = C->bytes_read - matches[8].rm_so;
    C->message = (char *) malloc(C->msg_length + 1);
    strncpy(C->message, buffer + matches[8].rm_so, C->msg_length);
    C->message[C->msg_length] = '\0';
}

off_t get_file_size(int fd) {
    struct stat fileStat;
    fstat(fd, &fileStat);
    off_t file_size = fileStat.st_size;
    return file_size;
}

void get(char *filename, int connfd, int requestID) {
    pthread_mutex_lock(&mutex);
    rwlock_t *lock = get_hash(hashes, filename);
    pthread_mutex_unlock(&mutex);

    reader_lock(lock);
    int fd = open(filename, O_RDONLY, 0);
    if (fd == -1) {
        if (errno == ENOENT) {
            fprintf(stderr, "GET,/%s,404,%d\n", filename, requestID);
            write_n_bytes(connfd, RES_404, strlen(RES_404));
        } else if (errno == EACCES || errno == EISDIR) {
            fprintf(stderr, "GET,/%s,403,%d\n", filename, requestID);
            write_n_bytes(connfd, RES_403, strlen(RES_403));
        }
    } else {
        // audit log
        fprintf(stderr, "GET,/%s,200,%d\n", filename, requestID);
        // response
        char get_response[2048];
        sprintf(get_response, "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\n\r\n", get_file_size(fd));
        write(connfd, get_response, strlen(get_response));
        if (pass_n_bytes(fd, connfd, get_file_size(fd)) == -1) {
            write_n_bytes(connfd, RES_505, strlen(RES_505));
        }
    }

    close(fd);
    reader_unlock(lock);
}

void put(Client *C, int connfd) {
    pthread_mutex_lock(&mutex);
    rwlock_t *lock = get_hash(hashes, C->uri);
    pthread_mutex_unlock(&mutex);

    writer_lock(lock);
    int fd = open(C->uri, O_WRONLY | O_TRUNC, 0666);
    if (fd == -1) {
        if (errno == EACCES) {
            fprintf(stderr, "PUT,/%s,%d,%d\n", C->uri, 403, C->req_id);
            write_n_bytes(connfd, RES_403, strlen(RES_403));
            close(connfd);
            writer_unlock(lock);
        }
        // code 201, create a file
        else if (errno == ENOENT) {
            fd = open(C->uri, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            write_n_bytes(fd, C->message, C->msg_length);
            pass_n_bytes(connfd, fd, (int) (C->content_length - C->msg_length));
            fprintf(stderr, "PUT,/%s,%d,%d\n", C->uri, 201, C->req_id);
            write_n_bytes(connfd, PUT_201, strlen(PUT_201));
        }
    }
    // code 200, file exists
    else {
        write_n_bytes(fd, C->message, C->msg_length);
        pass_n_bytes(connfd, fd, (int) (C->content_length - C->msg_length));
        fprintf(stderr, "PUT,/%s,%d,%d\n", C->uri, 200, C->req_id);
        write_n_bytes(connfd, PUT_200, strlen(PUT_200));
    }

    close(fd);
    writer_unlock(lock);
}

void *workerThread(void *arg) {
    Params *re = (Params *) arg;
    char buffer[BUFF_SIZE + 1];
    while (1) {
        int connfd;
        queue_pop(q, (void **) &connfd);

        // parse input
        Client C;
        parse_input(&C, buffer, connfd, re);

        // add lock to hash table
        pthread_mutex_lock(&mutex);
        if (get_hash(hashes, C.uri) == NULL) {
            rwlock_t *lock1 = rwlock_new(N_WAY, 4);
            hash_table_insert(hashes, strndup(C.uri, strlen(C.uri)), lock1);
        }
        pthread_mutex_unlock(&mutex);

        // do get or set
        if (strncmp(C.method, "GET", 3) == 0) {
            get(C.uri, connfd, C.req_id);
        } else {
            put(&C, connfd);
        }

        // done - free memory
        memset(buffer, 0, sizeof(buffer));
        free(C.message);
        close(connfd);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        return EXIT_FAILURE;
    }

    int opt = 0;
    int t = 4;
    while ((opt = getopt(argc, argv, "t:")) != -1) {
        switch (opt) {
        case 't': t = atoi(optarg); break;
        default: break;
        }
    }
    int port = atoi(argv[optind]);
    if (port < 1 || port > 65535) {
        return EXIT_FAILURE;
    }
    Listener_Socket sock;
    if (listener_init(&sock, port) == -1) {
        return EXIT_FAILURE;
    }

    Params re;
    regcomp(&(re.req_line),
        REQUEST_LINE_REGEX "("
                           "(" HEADER_FIELD_REGEX ")"
                           "*)\r\n(.*)",
        REG_EXTENDED);
    regcomp(&(re.header_field), "(" HEADER_FIELD_REGEX ")", REG_EXTENDED);

    q = queue_new(t);
    hashes = hash_table_create(t);
    pthread_t threads[t];
    pthread_mutex_init(&mutex, NULL);
    for (int i = 0; i < t; i++) {
        pthread_create(&(threads[i]), NULL, workerThread, (void *) &re);
    }

    while (1) {
        intptr_t connfd = listener_accept(&sock);
        queue_push(q, (void *) connfd);
    }

    // free memory
    regfree(&re.req_line);
    regfree(&re.header_field);
    hash_table_free(hashes);
    return 0;
}
