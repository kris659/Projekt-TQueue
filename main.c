#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include "queue.h"

#define N 4

pthread_t readers_th[N];
TQueue* queue;

void* writer(void* info) {
    sleep(1);
    int id = *(int*)info;
    int wiad[N*100];
    
    for(int i = 0; i < 100 * N; i++){
        wiad[i] = i;
        printf("WRITE | Writing: %d\n", wiad[i]);
        addMsg(queue, &wiad[i]);
        usleep(100000); 
        
    }
    sleep(1);

    // for(int i = 0; i < N / 2; i++){
    //     printf("Usuwanie | : %d\n", wiad[i]);
    //     removeMsg(queue, &wiad[i]);
    //     // sleep(2);
    //     usleep(100000); 
    // }

    // sleep(5);
    // for(int i = 0; i < N; i++){
    //     wiad[i] = i + 100;
    //     printf("WRITE | Writing: %d\n", wiad[i]);
    //     addMsg(queue, &wiad[i]);
    //     usleep(100000);         
    // }

    // for(int i = 0; i < N * 100; i++){
    //     wiad[i] = i + 100;
    //     printf("WRITE | Writing: %d\n", wiad[i]);
    //     addMsg(queue, &wiad[i]);
    //     usleep(100000);         
    // }
    sleep(100);
    return NULL;
}

void readInt(int reader_id){
    void* msg = getMsg(queue, readers_th[reader_id]);
    if(msg){
        int msgInt = *(int*)msg;
        printf("READER: %d | Read: %d\n", reader_id, msgInt);
    }
    else{
        printf("READ | NULL pointer\n");
    }
}

void* inf_reader(void* info) {
    int id = *(int*)info;
    int msg;
    
    subscribe(queue, readers_th[id]);
    printf("READER 0 | ZASUBSKRYBOWANO\n");
    while(1){
        sleep(1);
        readInt(id);
    }   

    return NULL;
}

void* inf_reader_2(void* info) {
    int id = *(int*)info;
    int msg;
    
    subscribe(queue, readers_th[id]);
    printf("READER 1 | ZASUBSKRYBOWANO\n");
    while(1){
        sleep(2);
        readInt(id);
    }   

    return NULL;
}

void* reader(void* info) {
    int id = *(int*)info;
    int msg;
    
    subscribe(queue, readers_th[id]);
    printf("ZASUBSKRYBOWANO!\n");
    for(int i = 0; i < 2; i++){
        sleep(1);
        readInt(id);
    }
    unsubscribe(queue, readers_th[id]);
    sleep(2);
    subscribe(queue, readers_th[id]);
    printf("ZASUBSKRYBOWANO!\n");
    for(int i = 0; i < 2; i++){
        sleep(1);
        readInt(id);
    }

    return NULL;
}

int main() {
    srand(time(NULL));
    pthread_t th[N];
    int tid[N];

    pthread_t writer_th, reader_th;
    queue = createQueue(4);

    for(int i = 0; i < N; i++){
        tid[i] = i;
    }

    pthread_create(&writer_th, NULL, writer, &tid[0]);
    pthread_create(&readers_th[0], NULL, inf_reader, &tid[0]);
    pthread_create(&readers_th[1], NULL, inf_reader_2, &tid[1]);
    // pthread_create(&readers_th[1], NULL, reader, &tid[1]);

    pthread_join(writer_th, NULL);

    // for(int i = 0; i < N; i++){
    //     tid[i] = i;
    //     pthread_create(&th[i], NULL, worker, &tid[i]);
    // }
    return 0;
}
