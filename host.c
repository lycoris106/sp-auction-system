#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int ID;
    int win_count;
    int rank;
} playerData;

void err_sys(const char *s){
    perror(s);
    exit(1);
}

void flush_sync(FILE *fp, int fd){
    fflush(fp);
    fsync(fd);
    return;
}

playerData* init_data(int player[], int num){
    playerData* ptr = (playerData*)malloc(sizeof(playerData)*num);
    for (int i = 0; i < num; i++){
	ptr[i].ID = player[i];
	ptr[i].win_count = 0;
	ptr[i].rank = 0;
    }
    return ptr;
}

void free_data(playerData *ptr, int num){
    free(ptr);
}


void calculate_rank(playerData* pl, int num){
    int cur_rank = 1;
    int count;
    for (int i = 10; i >= 0; i--){
	count = 0;
	for (int j = 0; j < num; j++){
	    if (pl[j].win_count == i){
		pl[j].rank = cur_rank;
		count++;
	    }
	}
	cur_rank += count;
    }
    return;
}


int main(int argc, char *argv[]){
    
    if (argc != 4)
	err_sys("argc error!");

    int host_id = atoi(argv[1]);
    int key = atoi(argv[2]);
    int depth = atoi(argv[3]);

    pid_t pid1, pid2;
    int pipe1_down[2], pipe1_up[2];
    int pipe2_down[2], pipe2_up[2];

    if (depth <= 1){ //root or second root

	//create child 1
	if (pipe(pipe1_down) < 0)
	    err_sys("pipe1_down create error");
	if (pipe(pipe1_up) < 0)
	    err_sys("pipe1_up create error");

	if ((pid1 = fork()) < 0){
	    err_sys("fork1 error");
	}
	else if (pid1 == 0){
	    close(pipe1_down[1]);
	    close(pipe1_up[0]);

	    dup2(pipe1_down[0], STDIN_FILENO);
	    dup2(pipe1_up[1], STDOUT_FILENO);

	    close(pipe1_down[0]);
	    close(pipe1_up[1]);

	    char new_depth[8];	    
	    sprintf(new_depth, "%d\0", depth+1);
	    execl("./host", "./host", argv[1], argv[2], new_depth, (char*)0);
	}

	close(pipe1_down[0]);
	close(pipe1_up[1]);
	
	//create child 2
	if (pipe(pipe2_down) < 0)
	    err_sys("pipe2_down create error");
	if (pipe(pipe2_up) < 0)
	    err_sys("pipe2_up create error");
	
	if ((pid2 = fork()) < 0)
	    err_sys("fork2 error");
	else if (pid2 == 0){
	    close(pipe2_down[1]);
	    close(pipe2_up[0]);
	    
	    dup2(pipe2_down[0], STDIN_FILENO);
	    dup2(pipe2_up[1], STDOUT_FILENO);

	    close(pipe2_down[0]);
	    close(pipe2_up[1]);

	    //close pipe1
	    close(pipe1_down[1]);
	    close(pipe1_up[0]);

	    char new_depth[8];
	    sprintf(new_depth, "%d\0", depth+1);
	    execl("./host", "./host", argv[1], argv[2], new_depth, (char*)0);
	    
	}

	close(pipe2_down[0]);
	close(pipe2_up[1]);

    }

    //root host
    if (depth == 0){
#ifdef DEBUG
	fprintf(stderr, "depth 0\n");
#endif
	char readFIFOname[16];
	sprintf(readFIFOname, "fifo_%d.tmp\0", host_id);
	
	FILE *readFIFO = fopen(readFIFOname, "r");
#ifdef DEBUG
	fprintf(stderr, "opened readFIFO\n");
#endif
	if (readFIFO == NULL)
	    err_sys("read FIFO error");
	int readfd = fileno(readFIFO);
	FILE *writeFIFO = fopen("fifo_0.tmp", "w");
	if (writeFIFO == NULL)
	    err_sys("write FIFO error");
	int writefd = fileno(writeFIFO);

	
	int playerID[8];


	fscanf(readFIFO, "%d %d %d %d %d %d %d %d", &playerID[0], &playerID[1], &playerID[2], 
		&playerID[3], &playerID[4], &playerID[5], &playerID[6], &playerID[7]);
#ifdef DEBUG
	fprintf(stderr, "finish reading playerIDs\n");
#endif

	//open pipe to children
	FILE *read_pipe1 = fdopen(pipe1_up[0], "r");
	FILE *write_pipe1 = fdopen(pipe1_down[1], "w");
	FILE *read_pipe2 = fdopen(pipe2_up[0], "r");
	FILE *write_pipe2 = fdopen(pipe2_down[1], "w");

	if (!(read_pipe1 && write_pipe1 && read_pipe2 && write_pipe2))
	    err_sys("open pipe error (depth = 0)");

	
	while (playerID[0] != -1){
	    playerData *players = init_data(playerID, 8);

	    fprintf(write_pipe1, "%d %d %d %d\n", playerID[0], playerID[1], playerID[2], playerID[3]);
	    fprintf(write_pipe2, "%d %d %d %d\n", playerID[4], playerID[5], playerID[6], playerID[7]);
#ifdef DEBUG
	    fprintf(stderr, "finish writing playerIDs\n");
#endif

	    flush_sync(write_pipe1, pipe1_down[1]);
	    flush_sync(write_pipe2, pipe2_down[1]);

	    int ID1, ID2, bid1, bid2;
	    for (int i = 0; i < 10; i++){
		fscanf(read_pipe1, "%d %d", &ID1, &bid1);
		fscanf(read_pipe2, "%d %d", &ID2, &bid2);

		int winnerID = (bid1 > bid2)? ID1 : ID2;
#ifdef DEBUG
		fprintf(stderr, "winner = %d\n", winnerID);
#endif

		for (int j = 0; j < 8; j++){
		    if (players[j].ID == winnerID){
			(players[j].win_count)++;
			break;
		    }
		}
	    }
	    
	    calculate_rank(players, 8);
	    
	    fprintf(writeFIFO, "%s\n", argv[2]);
	    for (int i = 0; i < 8; i++){
		fprintf(writeFIFO, "%d %d\n", players[i].ID, players[i].rank);
	    }
	    flush_sync(writeFIFO, writefd);


	    free_data(players, 8);

	    //next competition
	    fscanf(readFIFO, "%d %d %d %d %d %d %d %d", &playerID[0], &playerID[1], &playerID[2], 
		&playerID[3], &playerID[4], &playerID[5], &playerID[6], &playerID[7]);

	}

	//no competition left
	fprintf(write_pipe1, "-1 -1 -1 -1\n");
	fprintf(write_pipe2, "-1 -1 -1 -1\n");
	flush_sync(write_pipe1, pipe1_down[1]);
	flush_sync(write_pipe2, pipe2_down[1]);
	wait(NULL);
	wait(NULL);
	fclose(read_pipe1);
	fclose(write_pipe1);
	fclose(read_pipe1);
	fclose(write_pipe2);
	    
    }

    else if (depth == 1){ // middle host
	FILE *read_pipe1 = fdopen(pipe1_up[0], "r");
	FILE *write_pipe1 = fdopen(pipe1_down[1], "w");
	FILE *read_pipe2 = fdopen(pipe2_up[0], "r");
	FILE *write_pipe2 = fdopen(pipe2_down[1], "w");

	if (!(read_pipe1 && write_pipe1 && read_pipe2 && write_pipe2))
	    err_sys("open pipe error (depth = 1)");
	
	int playerID[4];
	scanf("%d %d %d %d", &playerID[0], &playerID[1], &playerID[2], &playerID[3]);
	
	while (playerID[0] != -1){
	    fprintf(write_pipe1, "%d %d\n", playerID[0], playerID[1]);
	    fprintf(write_pipe2, "%d %d\n", playerID[2], playerID[3]);
	    flush_sync(write_pipe1, pipe1_down[1]);
	    flush_sync(write_pipe2, pipe2_down[1]);

	    int ID1, ID2, bid1, bid2;
	    for (int i = 0; i < 10; i++){
		fscanf(read_pipe1, "%d %d", &ID1, &bid1);
		fscanf(read_pipe2, "%d %d", &ID2, &bid2);
	    

		if (bid1 > bid2)
		    printf("%d %d\n", ID1, bid1);
		else
		    printf("%d %d\n", ID2, bid2);
		flush_sync(stdout, STDOUT_FILENO);

		
	    }
	    //next competition
	    scanf("%d %d %d %d", &playerID[0], &playerID[1], &playerID[2], &playerID[3]);
	}

	//no competition left
	fprintf(write_pipe1, "-1 -1\n");
	fprintf(write_pipe2, "-1 -1\n");
	flush_sync(write_pipe1, pipe1_down[1]);
	flush_sync(write_pipe2, pipe2_down[1]);
	wait(NULL);
	wait(NULL);
	fclose(read_pipe1);
	fclose(write_pipe1);
	fclose(read_pipe2);
	fclose(write_pipe2);

    }
    else{ //leaf host
	int playerID[2];
	scanf("%d %d", &playerID[0], &playerID[1]);
	
	while (playerID[0] != -1){ // player should be forked again after 10 rounds
	    //create pipe
	    //create child 1
	    if (pipe(pipe1_down) < 0)
		err_sys("pipe1_down create error");
	    if (pipe(pipe1_up) < 0)
		err_sys("pipe1_up create error");

	    if ((pid1 = fork()) < 0){
		err_sys("fork1 error");
	    }
	    else if (pid1 == 0){
		close(pipe1_down[1]);
		close(pipe1_up[0]);

		dup2(pipe1_down[0], STDIN_FILENO);
		dup2(pipe1_up[1], STDOUT_FILENO);

		close(pipe1_down[0]);
		close(pipe1_up[1]);

		char ID_str[8];	    
		sprintf(ID_str, "%d\0", playerID[0]);
		execl("./player", "./player", ID_str, (char*)0);
	    }

	    close(pipe1_down[0]);
	    close(pipe1_up[1]);

	    //create child2
	    if (pipe(pipe2_down) < 0)
		err_sys("pipe2_down create error");
	    if (pipe(pipe2_up) < 0)
		err_sys("pipe2_up create error");

	    if ((pid2 = fork()) < 0){
		err_sys("fork2 error");
	    }
	    else if (pid2 == 0){
		close(pipe2_down[1]);
		close(pipe2_up[0]);
		
		dup2(pipe2_down[0], STDIN_FILENO);
		dup2(pipe2_up[1], STDOUT_FILENO);

		close(pipe2_down[0]);
		close(pipe2_up[1]);
		
		//close pipe1
		close(pipe1_down[1]);
		close(pipe1_up[0]);

		char ID_str[8];
		sprintf(ID_str, "%d\0", playerID[1]);
		execl("./player", "./player", ID_str, (char*)0);
	    }
	    close(pipe2_down[0]);
	    close(pipe2_up[1]);

	    FILE *read_pipe1 = fdopen(pipe1_up[0], "r");
	    FILE *write_pipe1 = fdopen(pipe1_down[1], "w");
	    FILE *read_pipe2 = fdopen(pipe2_up[0], "r");
	    FILE *write_pipe2 = fdopen(pipe2_down[1], "w");

	    if (!(read_pipe1 && write_pipe1 && read_pipe2 && write_pipe2))
		err_sys("open pipe error (depth = 1)");

	    int ID1, ID2, bid1, bid2;
	    for (int i = 0; i < 10; i++){
		fscanf(read_pipe1, "%d %d", &ID1, &bid1);
		fscanf(read_pipe2, "%d %d", &ID2, &bid2);

		if (bid1 > bid2)
		    printf("%d %d\n", ID1, bid1);
		else
		    printf("%d %d\n", ID2, bid2);
		flush_sync(stdout, STDOUT_FILENO);
	    }
	    wait(NULL);
	    wait(NULL);
	    fclose(read_pipe1);
	    fclose(write_pipe1);
	    fclose(read_pipe2);
	    fclose(write_pipe2);

	    //next compitition
	    scanf("%d %d", &playerID[0], &playerID[1]);

	}

	//no competition left
    }

    exit(0);

}
