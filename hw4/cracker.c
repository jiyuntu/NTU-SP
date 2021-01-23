#include <openssl/md5.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
char prefix[5000005], goal[5000005], outfile[5000005];
char record[15][1000005];
char ans[15][8][1000005];
int rec,n,m,p;

pthread_t ntid[15];

bool evaluate(char* s, int nn)
{
	char buf[256], mdString[33];
	MD5(s,strlen(s),buf);
	for(int j=0;j<16;j++){
		sprintf(&mdString[j*2],"%02x",(unsigned int)buf[j]);
	}
	for(int j=0;j<nn;j++) if(mdString[j]!=goal[j]){
		return false;
	}
	return true;
}
int dfs1(int d, int len)
{
	//printf("prefix=%s\n",prefix);
	if(d>=len){
		if(evaluate(prefix,1)){
			if(rec<m) strcpy(record[rec++],prefix);
			return 1;
		}
		return 0;
	}
	int l=strlen(prefix);
	int sum=0;
	for(int i=32;i<=126;i++){
		prefix[l]=i; prefix[l+1]='\0';
		sum+=dfs1(d+1,len);
	}
	prefix[l]='\0';
	return sum;
}
bool dfs2(int d, int len, int nn, int id) // may be endless loop QAQ
{
	//printf("record=%s\n",record[id]);
	if(d>=len){
		if(evaluate(record[id],nn)){
			return true;
		}
		return false;
	}
	int l=strlen(record[id]);
	for(int i=32;i<=126;i++){
		record[id][l]=i; record[id][l+1]='\0';
		if(dfs2(d+1,len,nn,id)) return true;
	}
	record[id][l]='\0';
	return false;
}
void *thr_fn(void *arg)
{
	//printf("inthread\n");
	int* id = arg;
	//printf("id=%d\n",*id);
	for(int i=2;i<=n;i++){
		//printf("id=%d\n, record=%s\n",*id,record[*id]);
		strcpy(ans[*id][i-1],record[*id]);
		for(int l=1;!dfs2(0,l,i,*id);l++) {}
	}
	strcpy(ans[*id][n],record[*id]);
	//printf("id=%d finish!\n",id);
	pthread_exit((void *)2);
}
int main(int argc, char* argv[])
{
	strcpy(prefix,argv[1]);
	strcpy(goal,argv[2]);
	strcpy(outfile,argv[5]);
	n=atoi(argv[3]), m=atoi(argv[4]);
	
	freopen(outfile,"w",stdout);

	int cnt=0;
	for(int l=1;cnt<m;l++) cnt+=dfs1(0,l);

	int x[10];
	for(int j=0;j<m;j++){
		x[j]=j;
		pthread_create(&(ntid[j]), NULL, thr_fn, &(x[j]));
	}

	void* tret;
	for(int j=0;j<m;j++) pthread_join(ntid[j],&tret);

	for(int i=0;i<m;i++){
		for(int j=1;j<=n;j++){
			printf("%s\n",ans[i][j]);
		}
		puts("===");
	}
	//for(int i=0;i<m;i++) printf("%s\n",record[i]);
}
