#include <assert.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#define BUF_SIZE 4096

void wget(int sock, char *file, char *output_filename)
{
    /* Form HTTP header */
    char wget_str[1024]; 
    snprintf(wget_str, 1024, "GET %s HTTP/1.1\n"
            "User-Agent: fos wget client\n"
            "Accept: image/png,image/*;q=0.8,*/*;q=0.5\n"
            "Accept-Language: en-us,en;q=0.5\n"
            "Accept-Encoding: gzip,deflate\n"
            "Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.7\n"
            "Keep-Alive: 115\n"
            "Connection: keep-alive\n"
            "Referer: http://127.0.0.1\n", file);

    /* Send request */
    int written = write(sock, wget_str, strlen(wget_str) + 1);
    assert(written == strlen(wget_str) + 1);

    if(!strcmp(file,"/quit")) return;

    /* open the file we are writing to */
    int output_file_fd = output_filename ? open(output_filename, O_CREAT|O_WRONLY|O_TRUNC, 0) : -1;

    char resp[BUF_SIZE * 2];
    memset(resp, 0, BUF_SIZE * 2);

    /* Read in the http header */
    unsigned int total = 0;
    char * header_end;

    while (true)
    {
        int bytes_read = read(sock, resp+total, 4096);
        total += bytes_read;

        if ((header_end = strstr(resp, "\r\n\r\n")))
            break;
    }

    /* Find the packet length */
    int header_len = (header_end + strlen("\r\n\r\n")) - resp;

    char *content_len_str = strstr(resp, "Content-Length: ");
    int content_len;
    char *p = resp;
    while(!content_len_str && p - resp < 1024)
    {
        p += strlen(p) + 1;
        content_len_str = strstr(p, "Content-Length: ");
    }

    content_len_str += strlen("Content-Length: ");
    sscanf(content_len_str, "%d", &content_len);
    char * data_p = strstr(content_len_str, "\r\n\r\n") + strlen("\r\n\r\n");

    /* Write any extra data we got while getting the content length */
    if(output_filename)
        write(output_file_fd, data_p, total - (data_p - resp));

    /* Read in the entire packet, beyond just the header */
    while (total < header_len + content_len) /* header + content + \r\n footer */
    {
        int bytes_read = read(sock, resp, BUF_SIZE);
        if(bytes_read > 0)
        {
            if(output_filename)
                write(output_file_fd, resp, bytes_read);
            total += bytes_read;
        }
        else if(bytes_read == 0)
        {
            printf("read when socket has been closed.\n");
            break;
        }
        else
        {
            printf("got < 0 return from read.\n");
            break;
        }
    }

    if(output_filename)
        close(output_file_fd);
}

