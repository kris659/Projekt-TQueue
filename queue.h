#ifndef LCL_QUEUE_H
#define LCL_QUEUE_H

// ==============================================
//
//  Version 1.1, 2025-01-16
//
// ==============================================

#include <pthread.h>

typedef struct Subscriber Subscriber;
struct Subscriber{
		pthread_t key;
		int messageToReadIndex;
		int messagesCount;
		Subscriber* next;
};

struct TQueue {
	int maxSize;

	int firstMessage;
	int newMessageIndex;

	Subscriber* firstSub;
	Subscriber* lastSub;
	int subCount;

	void** messages;
	int* notReceivedCount;

	pthread_mutex_t mx;
	pthread_cond_t cond_read;
	pthread_cond_t cond_write;

};
typedef struct TQueue TQueue;

TQueue* createQueue(int size);

void destroyQueue(TQueue *queue);

void subscribe(TQueue *queue, pthread_t thread);

void unsubscribe(TQueue *queue, pthread_t thread);

void addMsg(TQueue *queue, void *msg);

void* getMsg(TQueue *queue, pthread_t thread);

int getAvailable(TQueue *queue, pthread_t thread);

void removeMsg(TQueue *queue, void *msg);

void setSize(TQueue *queue, int size);

#endif //LCL_QUEUE_H
