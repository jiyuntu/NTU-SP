#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define maxn 13
#define maxm 11
const int number_of_players[3]={8,4,2};
int win[maxn]; // for root

char buf[512];

void itoa(int x, char* target)
{
	char buf[32];
	int l=0;
	while(x){
		buf[l++]='0'+(x%10);
		x/=10;
	}
	for(int i=0;i<l;i++) target[i]=buf[l-i-1];
	target[l]='\0';
}
int main(int argc, char* argv[])
{
	char host_id[32], key[32];
	strcpy(host_id,argv[1]);
	strcpy(key,argv[2]);
	int depth=atoi(argv[3]);
	char next_depth[32];
	itoa(depth+1,next_depth);

	int players[maxn];

	char write_fifo[]="fifo_0.tmp"; // for root
	char read_fifo[32]; // for root
	sprintf(read_fifo,"fifo_%s.tmp",host_id); // for root

	FILE* readFromFifo;
	FILE* writeToFifo;

	if(depth==0){
		mkfifo(write_fifo,0666);
		mkfifo(read_fifo,0666);
		readFromFifo = fopen(read_fifo,"r");
		writeToFifo = fopen(write_fifo,"w");
	}

	int fd_read[2][2], fd_write[2][2];
	int player_read[2][2];
	int pid0=0, pid1=0;
	FILE* readFromHost0;
	FILE* readFromHost1;
	FILE* writeToHost0;
	FILE* writeToHost1;
	if(depth<2){
		pipe(fd_read[0]);
		pipe(fd_read[1]);
		pipe(fd_write[0]);
		pipe(fd_write[1]);

		readFromHost0 = fdopen(fd_read[0][0],"r");
		readFromHost1 = fdopen(fd_read[1][0],"r");
		writeToHost0 = fdopen(fd_write[0][1],"w");
		writeToHost1 = fdopen(fd_write[1][1],"w");
	}

	if(depth<2){

		if((pid0=fork())>0){

			if((pid1=fork())>0){ //parent

				close(fd_write[0][0]);
				close(fd_read[0][1]);

				close(fd_write[1][0]);
				close(fd_read[1][1]);

			} else if(pid1==0) { //second child

				close(fd_read[0][0]); close(fd_read[0][1]); 
				close(fd_write[0][0]); close(fd_write[0][1]);
				close(fd_write[1][1]); close(fd_read[1][0]);

				if(fd_write[1][0]!=STDIN_FILENO){
					dup2(fd_write[1][0],STDIN_FILENO);
					close(fd_write[1][0]);
				}
				if(fd_read[1][1]!=STDOUT_FILENO){
					dup2(fd_read[1][1],STDOUT_FILENO);
					close(fd_read[1][1]);
				}				
				
				execlp("./host","./host",host_id,key,next_depth,(char *)0);

			}
		
		} else if(pid0==0) { //first childI

			close(fd_read[1][0]); close(fd_read[1][1]);
			close(fd_write[1][0]); close(fd_write[1][1]);
			close(fd_read[0][0]); close(fd_write[0][1]);

			if(fd_write[0][0]!=STDIN_FILENO){
				dup2(fd_write[0][0],STDIN_FILENO);
				close(fd_write[0][0]);
			}
			if(fd_read[0][1]!=STDOUT_FILENO){
				dup2(fd_read[0][1],STDOUT_FILENO);
				close(fd_read[0][1]);
			}
			
			execlp("./host","./host",host_id,key,next_depth,(char *)0);
		}
	}

	int iteration=0;

	while(1){

		iteration++;
		for(int i=0;i<maxn;i++) win[i]=0;

		if(depth==0){ //root host
			for(int i=0;i<number_of_players[depth];i++){
				fscanf(readFromFifo,"%d",&players[i]);
			}
		}

		if(depth>0){
			for(int i=0;i<number_of_players[depth];i++) scanf("%d",&players[i]);
		}

		if(depth<2){
			for(int i=0;i<number_of_players[depth]/2;i++){
				if(i) fprintf(writeToHost0," ");
				fprintf(writeToHost0,"%d",players[i]);
			}
			fprintf(writeToHost0,"\n");
			fflush(writeToHost0);

			for(int i=number_of_players[depth]/2;i<number_of_players[depth];i++){
				if(i>number_of_players[depth]/2) fprintf(writeToHost1, " ");
				fprintf(writeToHost1,"%d",players[i]);
			}
			fprintf(writeToHost1,"\n");
			fflush(writeToHost1);

		}

		if(players[0]==-1) break;

		FILE *player_fp0;
		FILE *player_fp1;
		if(depth==2){ // own pipe
			pipe(player_read[0]);
			pipe(player_read[1]);

			player_fp0=fdopen(player_read[0][0],"r");
			player_fp1=fdopen(player_read[1][0],"r");

			if((pid0=fork())>0){

				if((pid1=fork())>0){
					close(player_read[0][1]); close(player_read[1][1]);
				} else if(pid1==0) { //second child
					close(player_read[0][0]); close(player_read[0][1]);
					close(player_read[1][0]);
					if(player_read[1][1]!=STDOUT_FILENO){
						dup2(player_read[1][1],STDOUT_FILENO);
						close(player_read[1][1]);
					}
					char buf[32];
					itoa(players[1],buf);
					execlp("./player","./player",buf,(char *)0);
				}
			} else if(pid0==0) {
				close(player_read[1][0]); close(player_read[1][1]);
				close(player_read[0][0]);
				if(player_read[0][1]!=STDOUT_FILENO){
					dup2(player_read[0][1],STDOUT_FILENO);
					close(player_read[0][1]);
				}
				char buf[32];
				itoa(players[0],buf);
				execlp("./player","./player",buf,(char *)0);
			}
		}

		int cplayer[2]={0}, bids[2]={0};
		for(int i=0;i<10;i++){
			if(depth<2){
				fscanf(readFromHost0,"%d %d",&cplayer[0],&bids[0]);
				fscanf(readFromHost1,"%d %d",&cplayer[1],&bids[1]);
			} else {
				fscanf(player_fp0,"%d %d",&cplayer[0],&bids[0]);
				fscanf(player_fp1,"%d %d",&cplayer[1],&bids[1]);
			}
			//fclose(fp) when ends a loop???
			if(depth>0){
				if(bids[0]>bids[1]) printf("%d %d\n",cplayer[0],bids[0]);
				else printf("%d %d\n",cplayer[1],bids[1]);
				fflush(stdout);
			} else {
				if(bids[0]>bids[1]){
					win[cplayer[0]]++;
				}
				else{
					win[cplayer[1]]++;
				}
			}
		}

		//if(depth==2) fclose(player_fp0); fclose(player_fp1);

		if(depth==0){
			int rank[maxn];
			int cpy_players[maxn];
			for(int i=0;i<8;i++) cpy_players[i]=players[i];
			for(int i=0;i<8;i++){
				for(int j=1;j<8;j++){
					if(win[cpy_players[j]]>win[cpy_players[j-1]]){
						int tmp=cpy_players[j];
						cpy_players[j]=cpy_players[j-1];
						cpy_players[j-1]=tmp;
					}
				}
			}
			rank[cpy_players[0]]=1;
			for(int i=1;i<8;i++){
				rank[cpy_players[i]]=win[cpy_players[i]]==win[cpy_players[i-1]]?rank[cpy_players[i-1]]:i+1;
			}
			int a=fprintf(writeToFifo,"%s\n",key);
			for(int i=0;i<8;i++){
				fprintf(writeToFifo, "%d %d\n", players[i],rank[players[i]]);
			}
			fflush(writeToFifo);
		} else if(depth==2){
			if(pid0) waitpid(pid0,NULL,0);
			if(pid1) waitpid(pid1,NULL,0);
			close(player_read[0][0]);
			close(player_read[1][0]);
		}
	}
//end the loop

	if(depth<2){
		close(fd_read[0][0]); close(fd_read[1][0]);
		close(fd_write[0][1]); close(fd_write[1][1]);
	}

	if(pid0) waitpid(pid0,NULL,0);
	if(pid1) waitpid(pid1,NULL,0);

	exit(0);
}