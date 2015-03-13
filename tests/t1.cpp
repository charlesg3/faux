#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

int main(int argc, char **argv)
{
    char *out_data = "#include <stdio.h>\nint main(int argc, char **argv){\nfprintf(stderr, \"hello world\\n\");\nreturn 0;\n}\n";
    char in_data[1024];
    __attribute__(unused) int fd;

    memset(in_data, 0, sizeof(in_data));

    mkdir("/d1234", 0777);

    fd = open("/d1234/f1", O_RDWR | O_CREAT, 0777);
    assert(fd != -1);
    if(write(fd, out_data, strlen(out_data)) < 0)
        fprintf(stderr, "write fail.\n");
    close(fd);

    fd = open("/d1234/f1", O_RDONLY);
    if(read(fd, in_data, sizeof(in_data)) == 0)
        fprintf(stderr, "no data.\n");

    close(fd);

    assert(strcmp(in_data, out_data) == 0);
    return 0;
}
