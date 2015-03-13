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
    FILE *fp;

    memset(in_data, 0, sizeof(in_data));

    mkdir("/d1", 0777);
    mkdir("/d1", 0777);

    fp = fopen("/d1/f2", "w");
    fprintf(fp, "%s", out_data);
    fclose(fp);

    fp = fopen("/d1/f2", "rw");
    if(fread(in_data, 1, sizeof(in_data), fp) == 0)
        fprintf(stderr, "no data.\n");
    fclose(fp);

    assert(strcmp(in_data, out_data) == 0);
    return 0;
}

