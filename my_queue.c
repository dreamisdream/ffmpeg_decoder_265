
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct QueueNode
{
    char val[200 * 1000];
    int size;
    struct QueueNode* next;
}QueueNode;

typedef	struct Queue
{
    QueueNode* head;
    QueueNode* tail;
}Queue;

void queue_init(Queue* pq)
{
    pq->head = pq->tail = NULL;
}

void queue_destory(Queue* pq)
{
    if (pq == NULL) {
        printf("queue_destory pq is null\n");
    }
    QueueNode* cur = pq->head;
    while (cur) {
        QueueNode* next = cur->next;
        free(cur);
        cur = next;
    }
    pq->tail = pq->head = NULL;
}

void queue_push(Queue* pq, char* data, int size)
{
    if (pq == NULL) {
        printf("queue_push pq is null\n");
    }

    QueueNode* newNode = (QueueNode*)malloc(sizeof(QueueNode));
    if (NULL == newNode) {
        printf("malloc error\n");
        return;
    }
    memcpy(newNode->val, data, size);
    newNode->size = size;
    newNode->next = NULL;

    if (pq->tail == NULL) {
        pq->head = pq->tail = newNode;
    } else {
        pq->tail->next = newNode;
        pq->tail = newNode;
    }
    //printf("增加节点%d   size:%d   %d \n", queue_size(pq), size, strlen(newNode->val));
}

void queue_pop(Queue* pq)
{
    if (pq == NULL) {
        printf("queue_pop pq is null\n");
        return;
    }
    if (pq->head == NULL && pq->tail == NULL) {
        printf("queue_push pq is null\n");
    }

    if (pq->head->next == NULL) {
        free(pq->head);
        pq->head = pq->tail = NULL;
    } else {
        QueueNode* next = pq->head->next;
        free(pq->head);
        pq->head = next;
    }

}

int queue_empty(Queue* pq)
{
    if (pq == NULL) {
        printf("queue_empty pq is null\n");
        return 0;
    }

    return pq->head == NULL;
}

int queue_front(Queue* pq, char** data, int* size)
{
    if (pq == NULL || pq->head == NULL) {
        printf("queue_front pq or head is null\n");
        return 0;
    }

    if (size <= 0)
        return 0;
    *data = pq->head->val;
    *size = pq->head->size;

    return 1;
}

int queue_size(Queue* pq)
{
    if (pq == NULL) {
        printf("queue_size pq is null\n");
        return 0;
    }

    QueueNode* cur = pq->head;
    int count = 0;
    while (cur) {
        cur = cur->next;
        count++;
    }
    return count;
}
