#include <stdio.h>
#include <stdlib.h>

// Define a structure to represent a node in the queue
struct Node
{
    struct Process *data;
    struct Node *next;
};

// Define a structure to represent the queue
struct Queue
{
    struct Node *front;
    struct Node *rear;
};

// Function to create a new node
struct Node *createNode(struct Process *data)
{
    struct Node *newNode = (struct Node *)malloc(sizeof(struct Node));
    if (newNode == NULL)
    {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    newNode->data = data;
    newNode->next = NULL;
    return newNode;
}

// Function to initialize the queue
struct Queue *createQueue()
{
    struct Queue *queue = (struct Queue *)malloc(sizeof(struct Queue));
    if (queue == NULL)
    {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    queue->front = queue->rear = NULL;
    return queue;
}

// Function to enqueue a process into the queue
void enqueue(struct Queue *queue, struct Process *data)
{
    struct Node *newNode = createNode(data);
    if (queue->rear == NULL)
    {
        queue->front = queue->rear = newNode;
        return;
    }
    queue->rear->next = newNode;
    queue->rear = newNode;
}

// Function to dequeue a process from the queue
struct Process *dequeue(struct Queue *queue)
{
    if (queue->front == NULL)
        return NULL;
    struct Node *temp = queue->front;
    struct Process *data = temp->data;
    queue->front = queue->front->next;
    if (queue->front == NULL)
        queue->rear = NULL;
    free(temp);
    return data;
}

// Function to check if the queue is empty
int isEmpty(struct Queue *queue)
{
    return queue->front == NULL;
}

// Function to peek at the first element of the queue
struct Process *top(struct Queue *queue)
{
    if (queue->front == NULL)
        return NULL;
    return queue->front->data;
}

struct Node *popNode(struct Queue *queue)
{
    if (queue->front == NULL)
        return NULL;
    return queue->front;
}