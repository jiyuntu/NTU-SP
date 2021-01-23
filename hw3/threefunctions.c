#include "threadutils.h"

void BinarySearch(int thread_id, int init, int maxiter)
{
    ThreadInit(thread_id, init, maxiter);
    //fprintf(stderr,"finish create %d\n",thread_id);
    Current->y=0, Current->z=100;
    Current->i = -1;
    if(setjmp(Current->Environment)==0) longjmp(MAIN,1);
    else ++Current->i;
    
    //fprintf(stderr,"iter=%d\n",Current->i);

    for (; Current->i < Current->N; ++Current->i)
    {
        sleep(1);
        int m=(Current->y+Current->z)/2;
        printf("BinarySearch: %d\n",m);
        if(Current->x==m){
            ThreadExit();
        }
        else if(Current->x>m){
            Current->y=m+1;
        }
        else{
            Current->z=m-1;
        }
        ThreadYield();
    }
    ThreadExit();
}

void BlackholeNumber(int thread_id, int init, int maxiter)
{
    ThreadInit(thread_id, init, maxiter);

    Current->i = -1;
    if(setjmp(Current->Environment)==0) longjmp(MAIN,1);
    else ++Current->i;
    
    for (; Current->i < Current->N; ++Current->i)
    {
        sleep(1);
        
	int a[3],b[3],c[3];
	a[0]=Current->x%10, a[1]=(Current->x/10)%10, a[2]=(Current->x/100);

        for(int i=0;i<3;i++) b[i]=c[i]=a[i];
        for(int i=2;i>=0;i--){
            for(int j=0;j<i;j++){
                if(b[j+1]<b[j]){
                    int tmp=b[j+1];
                    b[j+1]=b[j];
                    b[j]=tmp;
                }
            }
        }
        for(int i=2;i>=0;i--){
            for(int j=0;j<i;j++){
                if(c[j+1]>c[j]){
                    int tmp=c[j+1];
                    c[j+1]=c[j];
                    c[j]=tmp;
                }
            }
        }

        int x=c[0]*100+c[1]*10+c[2], y=b[0]*100+b[1]*10+b[2];
        Current->x=x-y;
        printf("BlackholeNumber: %d\n",Current->x);
        if(Current->x==495) ThreadExit();
        
	ThreadYield();
    }
    ThreadExit();
}

void FibonacciSequence(int thread_id, int init, int maxiter)
{
    ThreadInit(thread_id, init, maxiter);
    
    Current->y=1;
    Current->i = -1;
    if(setjmp(Current->Environment)==0) longjmp(MAIN,1);
    else ++Current->i;

    //fprintf(stderr,"cur i=%d, cur n=%d\n",Current->i,Current->N);
    for (; Current->i < Current->N; ++Current->i)
    {
        sleep(1);
        
        int tmp=Current->x;
        Current->x=Current->y;
        Current->y=Current->y+tmp;
        printf("FibonacciSequence: %d\n",Current->x);

        ThreadYield();
    }
    ThreadExit();
}
