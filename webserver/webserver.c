/*
 * httpd.c: a simple webserver for fos
 *
 * Author: Charles Gruenwald III and David Wentzlaff
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/prctl.h>

//posix network interfaces
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

#ifndef LINUX
#include <env/env.h>
#include <uk_syscall/syscall.h>
#include <utilities/error.h>
#include <messaging/syscall_map.h>
#include <utilities/tsc.h>
#include <init/init.h>
#include <unistd.h>

//fos stuff for spawning
#include <sys/pms/pms.h>
#include <config/config.h>
#endif

#define min(a,b) (((a) < (b)) ? (a) : (b))
#define MAX_CHUNK_SIZE  1400
#define BINARY_CHUNK_SIZE 32768
#define RBUF_SIZE 2048

#define SO_SHARED 0x4000

unsigned long long last_request;
char *root_path = "";

int g_request_count = 0;
int g_connection_count = 0;
int g_id = 0;
int g_webserver_count;
int *g_child_procs;

int open_web_socket();
void sig_handler(int signum);

int fill_webpage(char *data);
bool handle_request(int c_fd, char *data);

#define error(fmt, ...) { fprintf(stderr, "%s:%d ", __FILE__, __LINE__); fprintf(stderr, fmt,## __VA_ARGS__); perror(" err:"); exit(-1);}

#if defined(THREADED_WEBSERVER)
#include <pthread.h>
void *connection_thread(void *);
#endif

//linked list of connections
#include "sglib.h"
typedef struct ConnectionEntry_s
{
    int fd;
    struct ConnectionEntry_s *next;
} ConnectionEntry;

#define add_connection(list, _fd) \
{ \
    ConnectionEntry *new_entry = malloc(sizeof(ConnectionEntry)); \
    new_entry->fd = _fd; \
    SGLIB_LIST_ADD(ConnectionEntry, list, new_entry, next); \
}

#define remove_connection(list, entry) \
{ \
    SGLIB_LIST_DELETE(ConnectionEntry, list, entry, next);  \
    free(entry);  \
}

#define connection_list_map(list, ptr, code){ \
    SGLIB_LIST_MAP_ON_ELEMENTS(ConnectionEntry, list, ptr, next, code); }

bool g_continue;

void spawn_children(int num_children);

int main(int argc, char **argv)
{
    int num_children = 0;
    if(argc == 3)
    {
        num_children = atoi(argv[2]) - 1;
        if(num_children < 0)
            num_children = 0;
    }

    g_webserver_count = num_children + 1;

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGHUP, sig_handler);

    root_path = argv[1];

    char *r_buf = (char*)malloc(RBUF_SIZE);
    ConnectionEntry *connections = NULL;

    if(num_children)
        fprintf(stderr, "fos webserver v0.3. Spawning %d children.\n", num_children);

    spawn_children(num_children); // fork()
    int fd = open_web_socket(); //create socket, bind, listen
    if (listen(fd, 128) < 0) error("error listening");

    if(g_id == 0)
        fprintf(stderr, "Webserver(s) listening.\n");

    g_continue = true;

    while(g_continue)
    {
        fd_set set;
        int max_fd;
loop_again:
        max_fd = 0;
        FD_ZERO(&set);

        connection_list_map(connections, ptr, {
                FD_SET(ptr->fd, &set);
                if(ptr->fd > max_fd) max_fd = ptr->fd; });

        FD_SET(fd, &set);

        if(fd > max_fd) max_fd = fd;

        assert(max_fd > 0);

        select(max_fd + 1, &set, NULL, NULL, NULL);

        // test to see if we have data on an already open connection
        connection_list_map(connections, ptr, {
                if(FD_ISSET(ptr->fd, &set))
                {
                    int read_size = read(ptr->fd, r_buf, RBUF_SIZE);
                    if(read_size > 0) //got data, handle the request
                    {
                        if(handle_request(ptr->fd, r_buf))
                            remove_connection(connections, ptr)

                        if(g_continue)
                            goto loop_again;
                        else
                            goto done;
                    }
                    else              //read of zero, closed connection
                    {
                        //fprintf(stderr, "WS: read==%d connection closing fd=%d\n", read_size, ptr->fd);
                        close(ptr->fd);
                        remove_connection(connections, ptr);
                    }
                }
                });

        // First see if we need to accept an incoming connection
        if(FD_ISSET(fd, &set))
        {
            int c_fd = accept(fd, NULL, NULL); //accept a new client connection
            if(c_fd < 0)
                fprintf(stderr, "couldn't accept.\n");
            else
            {
                g_connection_count++;
                //fprintf(stderr, "WS: new connection fd=%d\n", c_fd);

#if defined(THREADED_WEBSERVER)
                pthread_t th;
                pthread_create(&th, NULL, connection_thread, c_fd);
#else
                add_connection(connections, c_fd);
#endif
            }
        }
    }

done:
/*
    connection_list_map(connections, ptr, {
            close(ptr->fd);
    });
    */

#ifndef LINUX
    initSignal(FOS_MAILBOX_STATUS_OK);
#endif

    return 0;
}

#if defined(THREADED_WEBSERVER)
void *connection_thread(void *p)
{
    char *r_buf = (char*)malloc(1024);
    int fd = (int)p;
    bool connection_alive = true;
    while(connection_alive)
    {
        int read_size = read(fd, r_buf, 2048);
        if(read_size > 0) //got data, handle the request
        {
            printf("handling in thread.\n");
            handle_request(fd, r_buf);
        }
        else              //read of zero, closed connection
            connection_alive = false;
    }
    close(fd);
}
#endif

void spawn_children(int num_children)
{
    g_child_procs = (int *)malloc(sizeof(int) * g_webserver_count);

    int i;
    for(i = 0; i < num_children; i++)
    {
#ifndef LINUX
        const char * httpd_argv [] = {"httpd", root_path, NULL};
        pid_t child_pid;
        pmsExec("/bin/httpd", httpd_argv, NULL, easyEnv());
        pmsGetChildPID(&child_pid);
#else
        pid_t childpid = fork();
        if(childpid == 0)
        {
            prctl(PR_SET_PDEATHSIG, SIGHUP);
            g_id = i + 1;
            return; // Children return, parent keeps spawning
        }
        else
            g_child_procs[i + 1] = childpid;
#endif
    }
}

int open_web_socket()
{
    int sockfd;

    struct addrinfo hints, *res;

    int yes = 1;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) error("error opening socket");
    setsockopt(sockfd, SOL_SOCKET, SO_SHARED, (void *)true, sizeof(int));
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&yes, sizeof(int));

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    getaddrinfo("0.0.0.0", "80", &hints, &res);

    if (bind(sockfd, res->ai_addr, res->ai_addrlen) < 0) error("error binding");

    return sockfd;
}

int send_webpage_generated(int c_fd, float version)
{
    char data[MAX_CHUNK_SIZE + 10];
    struct timeval now;
    gettimeofday(&now, NULL);

    char webpage[4096];
    memset(webpage, 0, 4096);

    sprintf(webpage, "<html><head><title>fos webserver</title>"
            "<link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">"
            "</head>"
            "<body>"
            "<div id=\"outer\"><div id=\"text_logo\">"
            "<pre>\n  __           \n"
            " / _| ___  ___ \n"
            "| |_ / _ \\/ __|\n"
            "|  _| (_) \\__ \\\n"
            "|_|  \\___/|___/  v. 0.3</pre>"
            "</div><div style=\"width:800px;\">"
            "<br />"
            "<p>Thank you for visiting the fos homepage.  This webpage has been served by a webserver executing on fos, a new operating system for cloud and multicore computers.<br /><br /></p>"
            "<hr size=\"1\"> <p>To learn more about fos, check out the <a href=\"http://groups.csail.mit.edu/carbon/fos\">project page</a>."
            "<br /><br />This webpage contains dynamic content as follows:  "
            "<br />The current unix time is: <b>%20lu.%6.6lu</b> seconds since January 1, 1970."
            "<br /><br />You may <a href=\"/list\">view</a> the files on the filesystem.</p>"
            "<hr size=\"1\"><p>Copyright &copy; 2011 MIT CSAIL Carbon Group<br><img src=\"logo-mit.gif\"></p></div>"
            "</div></body></html>", now.tv_sec, now.tv_usec);

    //splat it all together at the end
    snprintf(data, MAX_CHUNK_SIZE-20,
            "HTTP/%1.1f 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: "
            "%ld\r\n\r\n%s",
            version, strlen(webpage), webpage);

    int write_len = write(c_fd, data, strlen(data));
    return write_len;
}

int send_header(int c_fd, char* contentType, int len, float version)
{
    char data[1024];
    snprintf(data, 1024, "HTTP/%1.1f 200 OK\r\n" "Content-Type: %s\r\n" "Connection: %s\r\n" "Content-Length: %d\r\n\r\n", version, contentType, version != 1.0 ? "Keep-Alive" : "close", len);
    int total_to_write = strlen(data);
    int offset = 0;
    int total_written = 0;
    int amount_written = 0;
    while(total_to_write)
    {
        amount_written = write(c_fd, data + offset, total_to_write);

        // error writing, retry
        if(amount_written < 0)
            continue;

        total_written += amount_written;
        offset += amount_written;
        total_to_write -= amount_written;
    }

    return total_written;
}

int send_webpage_notfound(int c_fd, char *path, float version)
{
    char data[MAX_CHUNK_SIZE + 10];

    char webpage[4096];
    memset(webpage, 0, 4096);
    sprintf(webpage, "<html><head><title>404: %s Not Found!</title>"
            "<link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">"
            "</head>"
            "<body>"
            "<div id=\"outer\"><h1>404: [%s] Not Found!</h1></center><div style=\"width:800px;\"><hr size=\"1\">"
            "Sorry, the path you requested was not found on this system."
            "<hr size=\"1\"><p>Copyright &copy; 2011 MIT CSAIL Carbon Group<br><img src=\"logo-mit.gif\">"
            "</div></div></body></html>", path, path);

    //splat it all together at the end
    snprintf(data, MAX_CHUNK_SIZE-20, "HTTP/%1.1f 404 NOT FOUND\r\n" "Content-Type: text/html\r\n" "Connection: %s\r\n" "Content-Length: %ld\r\n\r\n%s",
            version,
            version != 1.0 ? "Keep-Alive" : "close",
            strlen(webpage),
            webpage);

    int write_len = write(c_fd, data, strlen(data));
    return write_len;
}

void send_webpage_dir(int c_fd, char *path, float version)
{
    char page [4096];
    memset(page, 0, 4096);
    sprintf(page, "<html><head><title>Directory listing for: %s</title>"
            "<link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">"
            "</head>"
            "<body>"
            "<div id=\"outer\"><h1>Directory: %s</h1></center><div style=\"width:800px;\"><hr size=\"1\">", path, path);

    char *p = page + strlen(page);

    DIR *dir = opendir(path);
    struct dirent *entry;

    sprintf(p, "<table class=\"file_list\">\n<tr><td>Filename</td><td>Size</td></tr>");
    p += strlen(p);

    while((entry = readdir(dir)) != NULL)
    {
        struct stat st;
        char fullpath[1024];
        sprintf(fullpath, strcmp(path, "/") ? "%s/%s" : "%s%s", path, entry->d_name);
        if(stat(fullpath, &st))
            error("statting: %s", fullpath);

        if(!strcmp(entry->d_name, ".")) continue;

        if(S_ISDIR(st.st_mode))
            sprintf(p, "<tr><td><a href=\"%s\">[%s]</a></td><td>%ld</td></tr>\r", fullpath, strcmp(entry->d_name, "..") ? entry->d_name : "parent_directory", st.st_size);
        else
            sprintf(p, "<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>\r", fullpath, entry->d_name, st.st_size);
        p += strlen(p);
    }

    sprintf(p, "</table>\r\n");
    p += strlen(p);

    sprintf(p, "<hr size=\"1\"><p>Copyright &copy; 2011 MIT CSAIL Carbon Group<br><img src=\"logo-mit.gif\"></p></div>");
    p += strlen(p);

    sprintf(p, "</div></div></body></html>");
    p += strlen(p);

    int page_size = strlen(page);

    if(send_header(c_fd, "text/html", page_size, version) < 0)
    {
        close(c_fd);
        return;
    }

    write(c_fd, page, strlen(page));
    return;
}

void send_webpage_file(int c_fd, char *path, float version)
{
    int fd = open(path, 0);
    char strp[MAX_CHUNK_SIZE + 10];
    // find the length of the file
    int file_size = lseek(fd, 0, SEEK_END);

    char *content_type;
    // send out the header
    if(strcmp(path + strlen(path) - strlen(".xxx"), ".gif") == 0)
        content_type = "image/gif";
    else if(strcmp(path + strlen(path) - strlen(".xxx"), ".css") == 0)
        content_type = "text/css";
    else if(strcmp(path + strlen(path) - strlen(".xx"), ".js") == 0)
        content_type = "text/javascript";
    else
        content_type = "text/html";

    if(send_header(c_fd, content_type, file_size, version) < 0)
    {
        close(fd);
        return;
    }

    // send out the rest of the file
    lseek(fd, 0, SEEK_SET);
    int amount_read = 0;
    int total_read = 0;
    int amount_written = 0;
    int total_written = 0;
    int offset = 0;
    bool read_all = false;
    bool written_all = false;
//    printf("Reading file: [\033[22;34m%s\033[0m] (%d bytes)\n", path, file_size);

    //read the first chunk
    amount_read = read(fd, strp, MAX_CHUNK_SIZE);
    total_read = amount_read;
    if(total_read == file_size)
        read_all = true;

    while(read_all == false || written_all == false)
    {
        // write the read data
        amount_written = write(c_fd, strp + offset, amount_read);
        if(amount_written < 0)
            continue;

        total_written += amount_written;

        // handle partial writes
        if(amount_written < amount_read)
        {
            offset += amount_written;
            amount_read -= amount_written;
            continue;
        }

        if(total_written == file_size)
        {
            written_all = true;
            break;
        }

        // fully written the last chunk
        offset = 0;

        if(!read_all)
        {
            amount_read = read(fd, strp, MAX_CHUNK_SIZE);
            total_read += amount_read;
        }

        if(total_read == file_size)
            read_all = true;
    }

    close(fd);
    if(total_read != file_size || total_written != file_size || !read_all || !written_all)
    {
        fprintf(stderr, "error: didn't read or write entire file\n");
        fprintf(stderr, "total_read: %d total_written: %d file_size: %d\n",
                total_read, total_written, file_size);
        fprintf(stderr, "read_all: %d written_all: %d\n", read_all, written_all);
        assert(false);
    }

}

void send_webpage_mem(int c_fd, char *path, float version)
{
    unsigned long long start;
    unsigned long long khz = 2000000;
    unsigned long long ms;
    unsigned long long kbytes_per_sec;
#ifndef LINUX
    start = rdtscll();
#endif

    char *content_type = "application/octect-stream";
    char buf[BINARY_CHUNK_SIZE];

    long long bin_size = atoll(path);

    if(bin_size < 0)
    {
        fprintf(stderr, "oops got negative size. fail.\n");
        return;
    }

    //if(bin_size < BINARY_CHUNK_SIZE - 128)
    if(bin_size < 150)
    {
        snprintf(buf, BINARY_CHUNK_SIZE,
                "HTTP/%1.1f 200 OK\r\n"
                "Content-Type: %s\r\n"
                "Connection: %s\r\n"
                "Content-Length: %lld\r\n\r\n",
                version,
                content_type,
                version != 1.0 ? "Keep-Alive" : "close", bin_size);

        int total_to_write = strlen(buf) + bin_size;
        int offset = 0;
        int amount_written = 0;
        while(total_to_write)
        {
            amount_written = write(c_fd, buf + offset, total_to_write);

            if(amount_written < 0) continue; // error writing, retry

            offset += amount_written;
            total_to_write -= amount_written;
        }
    } else {
        if(send_header(c_fd, content_type, bin_size, version) < 0)
        {
            fprintf(stderr, "couldn't send header.\n");
            close(c_fd);
            return;
        }

        // send out the binary data
        int amount_written = 0;
        int total_written = 0;
        int offset = 0;

        while(total_written < bin_size)
        {
            // write the binary chunk
            int amount_to_write = min(bin_size - total_written, BINARY_CHUNK_SIZE - offset);
            amount_written = write(c_fd, buf + offset, amount_to_write);
            if(amount_written < 0)
                continue;

            total_written += amount_written;

            // handle partial writes
            if(amount_written < amount_to_write)
            {
                offset += amount_written;
                continue;
            }

            // fully written the last chunk
            offset = 0;
        }
    }


#ifndef LINUX
    ms = ((double)(rdtscll()-start));
    kbytes_per_sec = ((double)bin_size * khz) / ms;
    //printf("Sent webpage at: %lld kbps\n", kbytes_per_sec);
/*
    printf("handle mem (%d) request took: %lld ms since last request: %lld ms.\n",
            bin_size,
            ms / CONFIG_HARDWARE_KHZ,
            (start - last_request) / CONFIG_HARDWARE_KHZ);
            */


    last_request = rdtscll();
#endif
}

void send_webpage_cgi(int c_fd, char *path, char *query_string, float version)
{
    char path_and_args[4096];
    path_and_args[0] = '\0';
    strcat(path_and_args, path);
    strcat(path_and_args, " ");
    strcat(path_and_args, query_string);

    FILE *cgi_child = popen(path_and_args,"r");

    size_t read_count;
    char read_buf[4096];

    read_count = fread(read_buf, 1, 4096, cgi_child);
    pclose(cgi_child);

    char *content_type = "text/html";

    if(send_header(c_fd, content_type, read_count, version) < 0)
    {
        close(c_fd);
        return;
    }

    write(c_fd, read_buf, read_count);

}

bool handle_request(int c_fd, char *data)
{
    g_request_count++;
    int is_cgi_post = 0;
    int is_cgi_get = 0;
    char * queryString = NULL;
    char path[512];

    char requestType[128];
    char requestPath[256];
    float http_version;
    sscanf(data, "%127s %255s", requestType, requestPath);

//    fprintf(stderr, "[%d] request path: %s\n", g_id, requestPath);

    char *http_version_str = data + strlen(requestType) + strlen(requestPath) + 2 + strlen("HTTP/");
    sscanf(http_version_str,"%f", &http_version);

//    fprintf(stderr, "Http version_str: %s\n", http_version_str);

    if(strstr(http_version_str, "Connection: Keep-Alive"))
    {
        http_version = 1.1;
    }

    if (strcasecmp(requestType, "GET") && strcasecmp(requestType, "POST"))
    {
        fprintf(stderr, "not a get or post.\n");
        return true; // we should return a 501 here
    }

    if (strcasecmp(requestType, "POST") == 0)
    {
        is_cgi_post = 1;
        // print out the full header
        printf("found a POST with the following data: %s\n", data);
    }
    if (strcasecmp(requestType, "GET") == 0)
    {
        queryString = requestPath;
        while ((*queryString != '?') && (*queryString != '\0'))
            queryString++;
        if (*queryString == '?')
        {
            is_cgi_get = 1;
            *queryString = '\0';
            queryString++;
        }
    }

    sprintf(path, "%s", requestPath);

    if(!strcmp(requestPath,"/quit"))
    {
        send_webpage_notfound(c_fd, path, http_version);
        g_continue = false;
        return true;
    }

    struct stat st;
    // if there's no file then dynamically generate the content.
    if(strcmp(path,"/") == 0)
    {
        send_webpage_generated(c_fd, http_version);
    }
    else if(strcmp(path, "/list") == 0)
        send_webpage_dir(c_fd, root_path, http_version);
    else if(strncmp(path, "/mem", 4) == 0)
    {
        send_webpage_mem(c_fd, path + 4, http_version);
    }
    else if(strncmp(path, "/cgi-bin/", 9) == 0)
    {
        char actual_path[1024];
        snprintf(actual_path, 1024, "%s%s", root_path, path);
        if(stat(actual_path, &st))
        {
            send_webpage_notfound(c_fd, path, http_version);
        } else
            send_webpage_cgi(c_fd, actual_path, queryString, http_version);
    }
    else
    {
        char actual_path[1024];
        snprintf(actual_path, 1024, "%s%s", root_path, path);
        if(stat(actual_path, &st))
        {
            send_webpage_notfound(c_fd, path, http_version);
        }
        else if(S_ISDIR(st.st_mode))
            send_webpage_dir(c_fd, actual_path, http_version);
        else
            send_webpage_file(c_fd, actual_path, http_version);
    }

    if(http_version == 1.0)
    {
        close(c_fd);
        return true;
    }
    return false;
}

void sig_handler(int signum)
{
    if(g_id == 0)
    {
        int i;
        int status;
        pid_t done;

        for(i = 1; i < g_webserver_count; i++)
            kill(g_child_procs[i], SIGINT);

        for(i = 1; i < g_webserver_count; i++)
        {
            done = wait(&status);
        }


        free(g_child_procs);
    }

    fprintf(stderr, "Webserver [%d] handled %d reqs %d conns, ran on cpu: %d\n", 
            g_id, g_request_count, g_connection_count, sched_getcpu());
    exit(0);
}


