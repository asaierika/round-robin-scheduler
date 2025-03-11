#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <math.h>
#include "queue.c"

#define MAX_LINE_LENGTH 100
#define MEMORY_SIZE 2048
#define PAGE_NUM 512
#define IS_VIRTUAL 1
#define NOT_VIRTUAL 0

typedef enum
{
    UNREADY,
    READY,
    RUNNING,
    FINISHED
} ProcessStatus;

typedef struct Process
{
    char pid;
    char *name;                        // Process Name
    int arrival_time;                  // Arrival time
    int remaining_time;                // Remaining execution time
    ProcessStatus status;              // Status of the process
    int memory_executed_proc_arr_size; // Required memory usage in KB
    int memory_start_address;          // Indicate the start adress of the memory; -1 if the memory is not allocated
    int frames[PAGE_NUM];
    int total_page;
    int sum_remaining_time;
    int total_time;
} Process;

typedef struct
{
    int memory_blocks[MEMORY_SIZE]; // An array of blocks in the memory with 0 representing empty blocks and 1 representing allocated blocks
} ContiguousMemory;

char *filename = NULL;
char *memory_strategy = NULL;
int quantum_time = -1;
int current_time = 0;
int process_count = 0;
int memory_usage = 0;
int memory_array[MEMORY_SIZE] = {0};
int page_array[PAGE_NUM] = {0}; // page total number
int total_turnovertime;
int total_process;
int make_span;
int total_turnover;
struct Queue *unready_processes;
struct Queue *ready_processes;
double overhead_array[100];

ContiguousMemory contiguousMemory;
Process **executed_proc_arr;    // Array that monitors the order in which processes are executed
int executed_proc_arr_capacity; // Maximum capacity of the  array
int executed_proc_arr_size;     // Number of elements in the array
int executed_proc_arr_front;    // Index of the front of the array
int executed_proc_arr_rear;     // Index of the rear of the array

int tryAllocateMemory(Process *process, struct Queue *queue_ready);
int tryFreeMemory(Process *process);

void initialiseExecutedProcArr(int executed_proc_arr_capacity);
void addToExecutedProcArr(Process *process);
void removeFnishedProcFromArr();
Process *getEvictProcess();

int initialiseContiguousMemory();
int alllocateContiguousMemory(int executed_proc_arr_size);
void freeContiguousMemory(int start_address, int executed_proc_arr_size);

int allocatePagedMemory(struct Process *process, struct Queue *queue_ready, int virtual_status);
void freePagedMemory(Process *process);
int allocateVirtualMemory(Process *pProcess, struct Queue *pQueue);
int sumPageLeft();
void evictPage(int num_frame, int virtual_status);
int fitAllPage(Process *process, int frame_needed);
void printMemoryFrames(Process *process, int num_page);
int getPagedMemoryUsage();

int singleTurnoverTime(int service_time, int current_time);
void calculateTurnoverTime(int end_time, int start_time);
void calculateAverageTurnover(int total_turnovertime, int total_process);
void calculateMakeSpan(int time);
double calculateTimeOverhead(int turnaround_time, int service_time);
void printOverhead();

int parse(int argc, char *argv[]);
int readInput();
int schedule();
void cleanUp();

int main(int argc, char *argv[])
{
    parse(argc, argv);
    readInput();
    initialiseExecutedProcArr(process_count + 1); // initialise the array that tracks the order of the executed process
    schedule();
    cleanUp();
    return 0;
}

int tryAllocateMemory(Process *process, struct Queue *queue_ready)
{
    // Memory has already been allocated for the process
    if (process->memory_start_address != -1)
        return 1;

    // Allocation is not needed
    if (strcmp(memory_strategy, "infinite") == 0)
    {
        return 1;
    }

    if (strcmp(memory_strategy, "first-fit") == 0)
    {
        int start_address = alllocateContiguousMemory(process->memory_executed_proc_arr_size);
        if (start_address != -1)
        {
            process->memory_start_address = start_address;
            return 1;
        }
        return 0;
    }

    if (strcmp(memory_strategy, "paged") == 0)
    {
        if (process->total_page == 0)
        {
            // If no pages have been allocated yet
            allocatePagedMemory(process, queue_ready, NOT_VIRTUAL);
        }
        return 1; // Assuming allocatePagedMemory is correctly defined elsewhere
    }
    if (strcmp(memory_strategy, "virtual") == 0 && process->total_page == 0)
    {
        // If no pages have been allocated yet
        allocateVirtualMemory(process, queue_ready);
    }

    return 1;
}

int tryFreeMemory(Process *process)
{
    if (strcmp(memory_strategy, "infinite") == 0)
    {
        return 0; // Returns success as freeing memory is not needed
    }

    if (strcmp(memory_strategy, "first-fit") == 0)
    {
        freeContiguousMemory(process->memory_start_address, process->memory_executed_proc_arr_size);
        return 0;
    }
    if (strcmp(memory_strategy, "paged") == 0)
    {
        freePagedMemory(process);
        return 0;
    }
    if (strcmp(memory_strategy, "virtual") == 0)
    {
        freePagedMemory(process);
        return 0;
    }

    return 0;
}

// Initialize process manager with a specified executed_proc_arr_capacity
void initialiseExecutedProcArr(int max_process)
{
    executed_proc_arr_capacity = max_process;
    executed_proc_arr_size = 0;
    executed_proc_arr_front = 0;
    executed_proc_arr_rear = -1;
    executed_proc_arr = (Process **)malloc(executed_proc_arr_capacity * sizeof(Process *));
}

// Add or move the executed process to the end of the array if it's not already in the array
void addToExecutedProcArr(Process *process)
{
    // Check if the process is already in the executed_process array
    for (int i = executed_proc_arr_front; i != (executed_proc_arr_rear + 1) % executed_proc_arr_capacity; i = (i + 1) % executed_proc_arr_capacity)
    {
        if (executed_proc_arr[i]->pid == process->pid)
        {
            // Process found in the executed_proc_arr, remove it
            for (int j = i; j != executed_proc_arr_rear; j = (j + 1) % executed_proc_arr_capacity)
            {
                // Shift elements to the left to fill the gap
                executed_proc_arr[j] = executed_proc_arr[(j + 1) % executed_proc_arr_capacity];
            }
            executed_proc_arr_rear = (executed_proc_arr_rear - 1 + executed_proc_arr_capacity) % executed_proc_arr_capacity; // Update executed_proc_arr_rear index
            executed_proc_arr_size--;                                                                                        // Update executed_proc_arr_size
            break;
        }
    }
    // Add the process to the end of the executed_proc_arr if it's not already in the executed_proc_arr
    if (executed_proc_arr_size >= executed_proc_arr_capacity)
    {
        fprintf(stderr, "executed_proc_arr is full. Cannot add more processes.\n");
        return;
    }
    executed_proc_arr_rear = (executed_proc_arr_rear + 1) % executed_proc_arr_capacity;
    executed_proc_arr[executed_proc_arr_rear] = process;
    executed_proc_arr_size++;
}

void removeFnishedProcFromArr()
{
    for (int i = executed_proc_arr_front; i != (executed_proc_arr_rear + 1) % executed_proc_arr_capacity; i = (i + 1) % executed_proc_arr_capacity)
    {
        if (executed_proc_arr[i]->status == FINISHED)
        {
            // Process found in the executed_proc_arr, remove it
            for (int j = i; j != executed_proc_arr_rear; j = (j + 1) % executed_proc_arr_capacity)
            {
                // Shift elements to the left to fill the gap
                executed_proc_arr[j] = executed_proc_arr[(j + 1) % executed_proc_arr_capacity];
            }
            executed_proc_arr_rear = (executed_proc_arr_rear - 1 + executed_proc_arr_capacity) % executed_proc_arr_capacity; // Update executed_proc_arr_rear index
            executed_proc_arr_size--;                                                                                        // Update executed_proc_arr_size
            break;
        }
    }
}

// Select and remove the least recently executed process
Process *getEvictProcess()
{
    Process *process_to_evict;
    while (1)
    {
        if (executed_proc_arr_size == 0)
        {
            return NULL; // Queue is empty
        }
        process_to_evict = executed_proc_arr[executed_proc_arr_front];

        if (process_to_evict->total_page <= 0)
        {
            // If no page is available for this process,
            // remove it from the array and move to the next process
            executed_proc_arr_front = (executed_proc_arr_front + 1) % executed_proc_arr_capacity;
            executed_proc_arr_size--;
            continue;
        }
        else
        {
            // If the process have page to be evicted, terminates and returns
            break;
        }
    }
    return process_to_evict;
}

int initialiseContiguousMemory()
{
    for (int i = 0; i < MEMORY_SIZE - 1; i++)
    {
        contiguousMemory.memory_blocks[i] = 0; // Initialise all blocks to be empty
    }

    return 0;
}

int alllocateContiguousMemory(int executed_proc_arr_size)
{
    int start_address = -1;

    for (int i = 0; i < MEMORY_SIZE - executed_proc_arr_size + 1; i++)
    {
        for (int j = i; j < i + executed_proc_arr_size; j++)
        {
            if (contiguousMemory.memory_blocks[j] == 1)
            {
                break;
            }
            if (j == i + executed_proc_arr_size - 1 && contiguousMemory.memory_blocks[j] == 0)
            {
                start_address = i;
                break;
            }
        }

        if (start_address != -1)
            break;
    }

    // Allocate the memory blocks for the process if there exists an available hole
    if (start_address != -1)
    {
        for (int i = start_address; i < start_address + executed_proc_arr_size; i++)
        {
            contiguousMemory.memory_blocks[i] = 1;
        }
        memory_usage += executed_proc_arr_size; // Add to the total memory used count
    }

    return start_address; // Returns -1 if the allocation fails; returns the start address if the allocation is successful
}

void freeContiguousMemory(int start_address, int executed_proc_arr_size)
{
    for (int i = start_address; i < start_address + executed_proc_arr_size; i++)
    {
        contiguousMemory.memory_blocks[i] = 0;
    }

    memory_usage -= executed_proc_arr_size; // Reduce from the total memory used count
}

int allocatePagedMemory(struct Process *process, struct Queue *queue_ready, int virtual_status)
{
    int total_frame = ceil(process->memory_executed_proc_arr_size / 4.0);

    int page_left = sumPageLeft(page_array);
    if (virtual_status == NOT_VIRTUAL)
    {
        process->total_page = total_frame;
        if (page_left < total_frame && virtual_status == NOT_VIRTUAL)
        {
            evictPage(total_frame, virtual_status);
        }
        fitAllPage(process, total_frame);
    }

    if (virtual_status == IS_VIRTUAL)
    {
        int frame_needed = total_frame - process->total_page;
        if (page_left + process->total_page < 4)
        {
            if (frame_needed > 4)
            {
                frame_needed = 4;
            }

            int evict_frames = frame_needed - page_left - process->total_page;
            evictPage(evict_frames, IS_VIRTUAL);
        }
        int counter = 0;
        for (int i = 0; i < PAGE_NUM; i++)
        {
            if (page_array[i] == 0)
            {
                process->frames[counter] = i;
                process->total_page += 1;
                page_array[i] = 1;
                counter++;
            }
            if (counter == frame_needed)
            {
                break;
            }
        }
    }

    return 0;
}

void freePagedMemory(Process *process)
{
    printf("%d,EVICTED,evicted-frames=[", current_time);
    int first = 1;

    for (int i = 0; i < process->total_page; i++)
    {
        int pageIndex = process->frames[i];
        if (pageIndex != -1)
        { // Assuming -1 indicates an unused slot in the frames array
            if (!first)
                printf(",");
            printf("%d", pageIndex);
            first = 0;
            page_array[pageIndex] = 0; // Mark the page as free
            process->frames[i] = -1;
        }
    }
    process->total_page = 0; // Reset the total_page count for the process
    printf("]\n");
}

int allocateVirtualMemory(Process *pProcess, struct Queue *pQueue)
{
    int total_frame = ceil(pProcess->memory_executed_proc_arr_size / 4.0);

    int page_left = sumPageLeft(page_array);
    if (total_frame > page_left)
    {
        allocatePagedMemory(pProcess, pQueue, IS_VIRTUAL);
        return 0;
    }
    if (total_frame >= 4 && total_frame <= page_left)
    {
        allocatePagedMemory(pProcess, pQueue, IS_VIRTUAL);
    }
    return 0;
}

int sumPageLeft()
{
    int count = 0;
    for (int i = 0; i < PAGE_NUM; i++)
    {
        if (page_array[i] == 0)
        {
            count++;
        }
    }
    return count;
}

void evictPage(int num_frame, int virtual_status)
{
    printf("%d,EVICTED,evicted-frames=[", current_time);
    int first = 1;
    int initial_free_pages = sumPageLeft();

    int num_evict = 0;
    while (initial_free_pages < num_frame)
    {
        struct Process *top_process = getEvictProcess();

        if (top_process == NULL)
        {
            continue;
        }

        if (top_process->status == FINISHED)
        {
            continue;
        }

        if (virtual_status == IS_VIRTUAL)
        {
            num_evict = num_frame;
        }
        if (virtual_status == NOT_VIRTUAL)
        {
            num_evict = top_process->total_page;
        }
        int evicted_frames = 0;

        for (int i = 0; i < sizeof(top_process->frames); i++)
        {
            int index = top_process->frames[i];

            if (index == -1 || page_array[index] != 1)
                continue;

            if (!first)
            {
                printf(",");
            }

            printf("%d", index);
            first = 0;
            page_array[index] = 0;       // Mark the page as free
            top_process->frames[i] = -1; // Clear the frame entry
            evicted_frames++;

            if (evicted_frames == num_evict)
                break;
        }
        initial_free_pages = num_frame + initial_free_pages;
        if (virtual_status == IS_VIRTUAL)
        {
            int left_frame = top_process->total_page - num_frame;
            if (left_frame <= 0)
            {
                top_process->total_page = 0;
            }
        }
        if (virtual_status == NOT_VIRTUAL)
        {
            top_process->total_page = 0;
        }
    }

    printf("]\n");
}

int fitAllPage(Process *process, int frame_needed)
{
    int count = 0;
    for (int j = 0; j < PAGE_NUM; j++)
    {
        if (page_array[j] == 0)
        {
            process->frames[count] = j;
            count++;
            page_array[j] = 1;
            if (count == frame_needed)
            {
                break;
            }
        }
    }
    return 0;
}

void printMemoryFrames(Process *process, int num_page)
{
    for (int i = 0; i < num_page; i++)
    {
        int pageIndex = process->frames[i];
        if (pageIndex != -1)
        { // Check if the slot is used
            if (i < num_page - 1)
            {
                printf("%d,", pageIndex); // Print with a comma for all but the last index
                continue;
            }
            else
            {
                printf("%d", pageIndex); // Last index printed without a comma
            }
        }
    }
}

int getPagedMemoryUsage()
{
    int allocated = 0;
    for (int i = 0; i < PAGE_NUM; i++)
    {
        if (page_array[i] == 1)
        {
            allocated++;
        }
    }
    int memory_usage_perc = ceil(100 * (double)allocated / PAGE_NUM);
    return memory_usage_perc;
}

int singleTurnoverTime(int service_time, int current_time)
{
    int total_Single = current_time;
    double turn_over = (double)total_Single / service_time;
    for (int i = 0; i < total_process; ++i)
    {
        if (overhead_array[i] == 0)
        {
            overhead_array[i] = turn_over;
            break;
        }
    }
    return 0;
}

void calculateTurnoverTime(int end_time, int start_time)
{
    total_turnovertime = total_turnovertime + (end_time - start_time);
}

void calculateAverageTurnover(int total_turnovertime, int total_process)
{
    double average_turnover = ceil((double)total_turnovertime / total_process);

    printf("Turnaround time %.0f\n", average_turnover);
}
void calculateMakeSpan(int time)
{
    make_span = make_span + time;
}

double calculateTimeOverhead(int turnaround_time, int service_time)
{
    for (int i = 0; i < total_process; ++i)
    {
        if (overhead_array[i] == 0)
        {
            overhead_array[i] = (double)turnaround_time / service_time;
        }
    }
    return (double)turnaround_time / service_time;
}

void printOverhead()
{
    double maxOverhead = 0.0;
    double sumOverhead = 0.0;
    int count = 0; // To handle cases where array might have zero or non-positive values.

    // Iterate through each overhead value in the array
    for (int i = 0; i < total_process; ++i)
    {
        if (overhead_array[i] > 0)
        { // Assuming you want to ignore non-positive overhead values
            // Update the maxOverhead if the current element is greater
            if (overhead_array[i] > maxOverhead)
            {
                maxOverhead = overhead_array[i];
            }
            // Add the current element to the sumOverhead
            sumOverhead += overhead_array[i];
            ++count;
        }
    }

    // Calculate the average overhead
    double averageOverhead = (count > 0) ? (sumOverhead / count) : 0.0;

    // Print the maximum and average overhead values
    printf("%.2f ", maxOverhead);
    printf("%.2f\n", averageOverhead);
}

int parse(int argc, char *argv[])
{
    int opt;
    while ((opt = getopt(argc, argv, "f:m:q:")) != -1)
    {
        switch (opt)
        {
        case 'f':
            filename = optarg;
            break;
        case 'm':
            memory_strategy = optarg;
            break;
        case 'q':
            quantum_time = atoi(optarg);
            break;
        default:
            perror("Error reading command line parameters");
            return 1;
        }
    }
    return 0;
}

// Reads each line of the input file and enwraps the parameters
// into a process instance. Enqueue the process.
int readInput()
{
    FILE *input = fopen(filename, "r");
    if (input == NULL)
    {
        perror("Error opening file");
        return 1;
    }

    char line[MAX_LINE_LENGTH];
    unready_processes = createQueue();

    int id = 0;

    while (fgets(line, sizeof(line), input))
    {
        struct Process *new_process = malloc(sizeof(struct Process));
        if (new_process == NULL)
        {
            perror("Memory allocation failed");
            fclose(input); // Close the file before returning
            return 1;
        }

        int arrival_time;
        char name[10];
        int remaining_time;
        int memory_executed_proc_arr_size;

        // Scan the line to extract values
        if (sscanf(line, "%d %9s %d %d", &arrival_time, name, &remaining_time, &memory_executed_proc_arr_size) != 4)
        {
            perror("Error parsing input");
            free(new_process->name); // Free the name field
            free(new_process);       // Free the memory for the newly allocated Process struct
            fclose(input);           // Close the file before returning
            return 1;
        }

        // Allocate memory for the name field and copy the string into it
        new_process->name = strdup(name);
        if (new_process->name == NULL)
        {
            perror("Memory allocation failed");
            free(new_process); // Free the memory for the newly allocated Process struct
            fclose(input);     // Close the file before returning
            return 1;
        }

        // Assign the scanned values to the process
        new_process->pid = id;
        new_process->arrival_time = arrival_time;
        new_process->remaining_time = remaining_time;
        new_process->memory_executed_proc_arr_size = memory_executed_proc_arr_size;
        new_process->status = UNREADY;          // Initialise the status to UNREADY
        new_process->memory_start_address = -1; // Initialse the start address to be -1 as memory is not allocated for the process yet
        new_process->total_page = 0;
        new_process->sum_remaining_time = remaining_time;

        enqueue(unready_processes, new_process);

        process_count += 1; // Increase the total process count by 1
        id++;               // Increase the id by one for the next process
        total_process += 1;
    }
    fclose(input);
    return 0;
}

int schedule()
{
    ready_processes = createQueue();
    Process *current_process = NULL; // Initialize current_process to NULL
    int ready_process_count = 0;

    while (process_count > 0)
    {
        // Check for processes ready to run
        while (!isEmpty(unready_processes) && top(unready_processes)->arrival_time <= current_time)
        {
            Process *new_process = dequeue(unready_processes);
            new_process->status = READY;
            enqueue(ready_processes, new_process);
            ready_process_count++;
        }

        // Update remaining time of current process
        if (current_process != NULL)
        {
            current_process->remaining_time -= quantum_time;

            // Execution of current process completed
            if (current_process->remaining_time <= 0)
            {
                process_count--; // Decrease the count of unfinished processes
                singleTurnoverTime(current_process->sum_remaining_time, current_time - current_process->arrival_time);
                tryFreeMemory(current_process); // Free the memory

                // Print finished process
                printf("%d,%s,process-name=%s,proc-remaining=%d\n",
                       current_time, "FINISHED", current_process->name, ready_process_count);
                calculateTurnoverTime(current_time, current_process->arrival_time);

                current_process->status = FINISHED;
                removeFnishedProcFromArr();
                free(current_process->name); // Free memory allocated for current_process
                free(current_process);       // Free memory allocated for current_process
                current_process = NULL;      // Set current_process to NULL
            }
        }

        // Check if there are any ready processes
        if (isEmpty(ready_processes))
        {
            // No process is ready
            if (current_process == NULL)
            {
                // If there is no current running process,
                // add quantum time to current time and wait
                current_time += quantum_time;
                continue;
            }
            // If there is a current running process,
            // keep it running instead of changing its status to ready
        }
        else
        {
            // If there are RUNNING processes,
            // change the status of the running process to READY
            // and put it at the end of the queue
            if (current_process != NULL)
            {
                current_process->status = READY;
                enqueue(ready_processes, current_process);
                ready_process_count++;
            }

            Process *next_process = NULL;

            // Try allocate memory to the next process until successful
            while (1)
            {
                next_process = dequeue(ready_processes);

                int allocated = tryAllocateMemory(next_process, ready_processes);

                // If the allocation is not successful, put it at the end of the queue
                if (!allocated)
                {
                    enqueue(ready_processes, next_process);
                }
                else
                {
                    break;
                }
            }

            // Run the first process in the ready queue
            // and change its status to RUNNING
            current_process = next_process;
            ready_process_count--;
            current_process->status = RUNNING;
            addToExecutedProcArr(current_process);
            if (current_process->remaining_time > 0)
            {
                // Print running process
                printf("%d,%s,process-name=%s,remaining-time=%d",
                       current_time, "RUNNING", current_process->name, current_process->remaining_time);
                if (strcmp(memory_strategy, "first-fit") == 0)
                {
                    int memory_usage_perc = ceil(100 * (double)memory_usage / MEMORY_SIZE);

                    printf(",mem-usage=%d%%,allocated-at=%d", memory_usage_perc, current_process->memory_start_address);
                }
                if (strcmp(memory_strategy, "paged") == 0)
                {
                    int memory_usage_perc = getPagedMemoryUsage();

                    printf(",mem-usage=%d%%,", memory_usage_perc);
                    printf("mem-frames=[");
                    printMemoryFrames(current_process, current_process->total_page);
                    printf("]");
                }
                if (strcmp(memory_strategy, "virtual") == 0)
                {
                    int memory_usage_perc = getPagedMemoryUsage();

                    printf(",mem-usage=%d%%,", memory_usage_perc);
                    printf("mem-frames=[");
                    printMemoryFrames(current_process, current_process->total_page);
                    printf("]");
                }

                printf("\n");
            }
        }

        current_time += quantum_time; // Update current time
    }

    calculateAverageTurnover(total_turnovertime, total_process);
    printf("Time overhead ");
    printOverhead();
    printf("Makespan %d", (current_time - quantum_time));
    fflush(stdout);
    return 0;
}

void cleanUp()
{
    while (!isEmpty(unready_processes))
    {
        struct Process *process = dequeue(unready_processes);
        free(process->name);
        free(process);
    }

    free(unready_processes);

    while (!isEmpty(ready_processes))
    {
        struct Process *process = dequeue(ready_processes);
        free(process->name);
        free(process);
    }
    free(ready_processes);

    free(executed_proc_arr);
}
