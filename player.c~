#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>

void flush_sync(FILE *fp, int fd){
    fflush(fp);
    fsync(fd);
    return;
}

int main(int argc, char** argv){
    int ID = atoi(argv[1]);

    int bid_list[21] = {20, 18, 5, 21, 8, 7, 2, 19, 14, 13
	, 9, 1, 6, 10, 16, 11, 4, 12, 15, 17, 3};
    int bid;
    for (int i = 1; i <= 10; i++){
	bid = bid_list[ID + i - 2] * 100;
	printf("%d %d\n", ID, bid);
	flush_sync(stdout, STDOUT_FILENO);
    }

    exit(0);
}
