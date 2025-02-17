#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "queue.h"

int mod(TQueue *queue, int x){
    int N = queue->maxSize;
    return (x % N + N) % N;
}

TQueue* createQueue(int size){
    TQueue *queue = malloc(sizeof(TQueue));
    queue->maxSize = size;
    queue->messages = (void**)malloc(size * sizeof(void*));
    queue->notReceivedCount = (int*)malloc(size * sizeof(int));
    queue->firstMessage = -1;
    queue->newMessageIndex = 0;
    queue->firstSub = NULL;
    queue->subCount = 0;

    pthread_mutex_init(&queue->mx, NULL);
    pthread_cond_init(&queue->cond_read, NULL);
    pthread_cond_init(&queue->cond_write, NULL);

    return queue;
}

void destroyQueue(TQueue *queue){
    pthread_mutex_lock(&queue->mx);

    Subscriber *sub = queue->firstSub;
    Subscriber* previous;

    while(sub){
        previous = sub;
        sub = sub->next;
        free(previous);
    }
    free(queue->messages);
    free(queue->notReceivedCount);
    free(queue);

    pthread_mutex_unlock(&queue->mx);
}

void subscribe(TQueue *queue, pthread_t thread){
    Subscriber *newSub = malloc(sizeof(Subscriber));
    newSub->key = thread;
    newSub->messagesCount = 0;
    newSub->next = NULL;
    
    pthread_mutex_lock(&queue->mx);
    newSub->messageToReadIndex = queue->newMessageIndex;
    
    if(queue->subCount == 0){
        queue->firstSub = newSub;
        queue->subCount++;
        pthread_mutex_unlock(&queue->mx);
        return;
    }
    Subscriber *sub = queue->firstSub;
    while(sub->next){
        sub = sub->next;
    }
    sub->next = newSub;
    queue->subCount++;
    pthread_mutex_unlock(&queue->mx);
}

Subscriber* getSubscriber(TQueue *queue, pthread_t thread){
    pthread_mutex_lock(&queue->mx);

    Subscriber* sub = queue->firstSub;
    while(sub && sub->key != thread){
        sub = sub->next;
    }

    pthread_mutex_unlock(&queue->mx);
    return sub;
}

void unsubscribe(TQueue *queue, pthread_t thread){
    Subscriber *sub = getSubscriber(queue, thread);

    if(sub == NULL)
        return;
    
    pthread_mutex_lock(&queue->mx);

    while(sub->messagesCount > 0){
        queue->notReceivedCount[sub->messageToReadIndex]--;
        sub->messageToReadIndex = mod(queue, sub->messageToReadIndex + 1);
        sub->messagesCount--;
    }
    pthread_cond_broadcast(&queue->cond_write);

    queue->subCount--;

    sub = queue->firstSub;
    Subscriber *previousSub = NULL;

    while(sub && sub->key != thread){
        previousSub = sub;
        sub = sub->next;
    }

    if(sub && sub->key == thread){
        if(previousSub)
            previousSub->next = sub->next;    
        else
            queue->firstSub = sub->next;
        free(sub);
    }
    pthread_mutex_unlock(&queue->mx);
}

void addMsg(TQueue *queue, void *msg){
    pthread_mutex_lock(&queue->mx);

    if(queue->subCount == 0){
        pthread_mutex_unlock(&queue->mx);
        return;
    }

    while(queue->firstMessage == queue->newMessageIndex){
        while(queue->firstMessage != -1 && queue->notReceivedCount[queue->firstMessage] == 0){
            queue->firstMessage = mod(queue, queue->firstMessage + 1);
            if(queue->firstMessage == queue->newMessageIndex){
                queue->firstMessage = -1;            
            }
        }
        if(queue->firstMessage == queue->newMessageIndex)
            pthread_cond_wait(&queue->cond_write, &queue->mx);
    }
    
    if(queue->firstMessage == -1){
        queue->firstMessage = queue->newMessageIndex;
    }
    queue->messages[queue->newMessageIndex] = msg;
    
    queue->notReceivedCount[queue->newMessageIndex] = queue->subCount;
    queue->newMessageIndex = mod(queue, queue->newMessageIndex + 1);

    Subscriber* sub = queue->firstSub;
    while(sub){
        sub->messagesCount++;
        sub = sub->next;
    }

    pthread_mutex_unlock(&queue->mx);
    pthread_cond_broadcast(&queue->cond_read);
}

void* getMsg(TQueue *queue, pthread_t thread){
    Subscriber* sub = getSubscriber(queue, thread);
    if(!sub)
        return NULL;

    pthread_mutex_lock(&queue->mx);
    while(sub->messagesCount == 0){
        pthread_cond_wait(&queue->cond_read, &queue->mx);
    }

    int messageToRead = sub->messageToReadIndex;
    queue->notReceivedCount[messageToRead]--;
    sub->messageToReadIndex = mod(queue, messageToRead + 1);
    sub->messagesCount--;
    void* message = queue->messages[messageToRead];

    pthread_mutex_unlock(&queue->mx);
    pthread_cond_broadcast(&queue->cond_write);
    return message;
}

int getAvailable(TQueue *queue, pthread_t thread){ 
    Subscriber* sub = getSubscriber(queue, thread);
    if(!sub)
        return 0; 
    return sub->messagesCount;
}

void removeMsg(TQueue *queue, void *msg){
    pthread_mutex_lock(&queue->mx);

    int messageIndex = -1;
    for(int i = 0; i < queue->maxSize; i++){
        if(msg == queue->messages[i]){
            messageIndex = i;
            break;
        }
    }

    if(messageIndex == -1 || queue->notReceivedCount[messageIndex] == 0){
        // printf("Message to remove not found!\n");
        pthread_mutex_unlock(&queue->mx);
        return;
    }

    Subscriber *sub = queue->firstSub;
    while(sub){ // Przesunięcie indexu do czytania czytelników którzy odczytali już tą wiadomość
        if(sub->messagesCount == 0){
            sub->messageToReadIndex = mod(queue, sub->messageToReadIndex - 1);
            sub = sub->next;
            continue;
        }

        if(messageIndex < queue->newMessageIndex){
            if(sub->messageToReadIndex > messageIndex && sub->messageToReadIndex < queue->newMessageIndex){
                sub->messageToReadIndex = mod(queue, sub->messageToReadIndex - 1);
            }
            else
                sub->messagesCount--;
        }
        else{
            if(sub->messageToReadIndex > messageIndex || sub->messageToReadIndex < queue->newMessageIndex){
                sub->messageToReadIndex = mod(queue, sub->messageToReadIndex - 1);
            }
            else
                sub->messagesCount--;
        }        
        sub = sub->next;
    }

    // Przesunięcie wiadomości poźniejszych od usuwanej
    messageIndex = mod(queue, messageIndex + 1);
    while(messageIndex != queue->newMessageIndex){
        queue->messages[mod(queue, messageIndex - 1)] = queue->messages[messageIndex];
        messageIndex = mod(queue, messageIndex + 1);
    }
    queue->newMessageIndex = mod(queue, queue->newMessageIndex - 1);
    if(queue->firstMessage == queue->newMessageIndex){
        queue->firstMessage = -1;
    }

    pthread_mutex_unlock(&queue->mx);
    pthread_cond_broadcast(&queue->cond_write);
}

void setSize(TQueue *queue, int size){   
    pthread_mutex_lock(&queue->mx);

    if(size == queue->maxSize){
        return;
    }

    void** messages = (void**)malloc(size * sizeof(void*));
    int* notReceivedCount = (int*)malloc(size * sizeof(int));

    // Sprawdzenie, czy jakieś wiadomości nie zostały już odczytane przez wszystkich
    while(queue->firstMessage != -1 && queue->notReceivedCount[queue->firstMessage] == 0){ 
        queue->firstMessage = mod(queue, queue->firstMessage + 1);
        if(queue->firstMessage == queue->newMessageIndex){
            queue->firstMessage = -1;            
        }
    }

    if(queue->firstMessage != -1){
        int messagesCount = mod(queue, queue->newMessageIndex - queue->firstMessage);
        if(messagesCount == 0)
            messagesCount = queue->maxSize;
        // Usunięcie nadmiarowych wiadomości
        for(int i = 0; i < messagesCount - size; i++){ 
            Subscriber* sub = queue->firstSub;
            while(sub){
                if(sub->messageToReadIndex == queue->firstMessage && sub->messagesCount > 0){
                    sub->messageToReadIndex = mod(queue, sub->messageToReadIndex + 1);
                    sub->messagesCount--;
                }
                sub = sub->next;
            }
            queue->firstMessage = mod(queue, queue->firstMessage + 1);
        }
        
        for(int i = 0; i < messagesCount && i < size; i++){
            // Przepisanie pozostałych wiadomości na początek
            messages[i] = queue->messages[queue->firstMessage];
            notReceivedCount[i] = queue->notReceivedCount[queue->firstMessage];            
            queue->firstMessage = mod(queue, queue->firstMessage + 1);
        }

        queue->newMessageIndex = messagesCount % size;
        queue->firstMessage = 0;

        // Ustawienie sub->messageToReadIndex
        Subscriber* sub = queue->firstSub;
        while(sub){
            if(sub->messagesCount == 0){
                sub->messageToReadIndex = queue->newMessageIndex;
                sub = sub->next;
                break;
            }
            for(int i = 0; i < size; i++){
                if(queue->messages[sub->messageToReadIndex] == messages[i]){
                    sub->messageToReadIndex = i;
                    break;
                }
            }
            sub = sub->next;
        }
    }
    else{
        queue->newMessageIndex = 0;
    }
    
    free(queue->messages);
    free(queue->notReceivedCount);

    queue->maxSize = size;    
    queue->messages = messages;
    queue->notReceivedCount = notReceivedCount;

    pthread_mutex_unlock(&queue->mx);
    pthread_cond_broadcast(&queue->cond_write);
}