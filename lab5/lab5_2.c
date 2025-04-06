#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#define FIXED_SIZE 1000
#define INITIAL_QUEUE_SIZE 10

typedef struct {
    unsigned char type;
    unsigned short hash;
    unsigned char size;
    unsigned char data[256];
} message_t;

message_t buffer[FIXED_SIZE];
int head = 0;
int tail = 0;
unsigned long producedCount = 0;
unsigned long consumedCount = 0;
int producers = 0;
int consumers = 0;
int current_queue_size = INITIAL_QUEUE_SIZE;

pthread_mutex_t mutex;
pthread_cond_t not_full;
pthread_cond_t not_empty;

unsigned short compute_hash(const message_t *msg) {
    unsigned short result = 0;
    message_t temp = *msg;
    temp.hash = 0;
    result += temp.type;
    result += temp.size;
    for (int i = 0; i < temp.size; i++) {
        result += temp.data[i];
    }
    return result;
}

int verify_hash(const message_t *msg) {
    return compute_hash(msg) == msg->hash;
}

void fill_random_message(message_t *msg) {
    msg->type = (unsigned char)(rand() % 256);
    msg->size = (unsigned char)(rand() % 256);
    for (int i = 0; i < msg->size; i++) {
        msg->data[i] = (unsigned char)(rand() % 256);
    }
    for (int i = msg->size; i < 256; i++) {
        msg->data[i] = 0;
    }
    msg->hash = compute_hash(msg);
}

void* producer(void* arg) {
    (void)arg;
    srand(time(NULL) ^ pthread_self());
    while (1) {
        pthread_mutex_lock(&mutex);
        while ((tail - head + FIXED_SIZE) % FIXED_SIZE >= current_queue_size) {
            pthread_cond_wait(&not_full, &mutex);
        }
        fill_random_message(&buffer[tail]);
        tail = (tail + 1) % FIXED_SIZE;
        producedCount++;
        unsigned long producedNow = producedCount;
        pthread_cond_signal(&not_empty);
        pthread_mutex_unlock(&mutex);
        printf("[Producer %lu] Created message #%lu\n", pthread_self(), producedNow);
        sleep(1);
    }
    return NULL;
}

void* consumer(void* arg) {
    (void)arg;
    while (1) {
        pthread_mutex_lock(&mutex);
        while ((tail - head + FIXED_SIZE) % FIXED_SIZE == 0) {
            pthread_cond_wait(&not_empty, &mutex);
        }
        message_t msg = buffer[head];
        head = (head + 1) % FIXED_SIZE;
        consumedCount++;
        unsigned long consumedNow = consumedCount;
        pthread_cond_signal(&not_full);
        pthread_mutex_unlock(&mutex);
        int ok = verify_hash(&msg);
        printf("[Consumer %lu] Consumed message #%lu (hash_ok=%s)\n",
               pthread_self(), consumedNow, ok ? "YES" : "NO");
        sleep(1);
    }
    return NULL;
}

int main(void) {
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&not_full, NULL);
    pthread_cond_init(&not_empty, NULL);

    pthread_t producer_threads[100];
    int producer_count = 0;
    pthread_t consumer_threads[100];
    int consumer_count = 0;

    printf("Commands(5_2):\n"
           "  p - create producer\n"
           "  c - create consumer\n"
           "  P - kill producer\n"
           "  C - kill consumer\n"
           "  + - increase queue size\n"
           "  - - decrease queue size\n"
           "  s - show status\n"
           "  q - exit\n");

    while (1) {
        int ch = getchar();
        if (ch == EOF || ch == 'q') break;
        if (ch == '\n') continue;

        switch (ch) {
            case 'p':
                if (producer_count < 100) {
                    pthread_create(&producer_threads[producer_count], NULL, producer, NULL);
                    producer_count++;
                    pthread_mutex_lock(&mutex);
                    producers++;
                    pthread_mutex_unlock(&mutex);
                    printf("Created producer %lu\n", producer_threads[producer_count - 1]);
                } else {
                    printf("Reached maximum number of producers\n");
                }
                break;
            case 'c':
                if (consumer_count < 100) {
                    pthread_create(&consumer_threads[consumer_count], NULL, consumer, NULL);
                    consumer_count++;
                    pthread_mutex_lock(&mutex);
                    consumers++;
                    pthread_mutex_unlock(&mutex);
                    printf("Created consumer %lu\n", consumer_threads[consumer_count - 1]);
                } else {
                    printf("Reached maximum number of consumers\n");
                }
                break;
            case 'P':
                if (producer_count > 0) {
                    pthread_t tid = producer_threads[--producer_count];
                    pthread_cancel(tid);
                    pthread_join(tid, NULL);
                    pthread_mutex_lock(&mutex);
                    producers--;
                    pthread_mutex_unlock(&mutex);
                    printf("Killed producer %lu\n", tid);
                } else {
                    printf("No producers to remove\n");
                }
                break;
            case 'C':
                if (consumer_count > 0) {
                    pthread_t tid = consumer_threads[--consumer_count];
                    pthread_cancel(tid);
                    pthread_join(tid, NULL);
                    pthread_mutex_lock(&mutex);
                    consumers--;
                    pthread_mutex_unlock(&mutex);
                    printf("Killed consumer %lu\n", tid);
                } else {
                    printf("No consumers to remove\n");
                }
                break;
            case '+':
                pthread_mutex_lock(&mutex);
                if (current_queue_size < FIXED_SIZE) {
                    current_queue_size++;
                    pthread_cond_signal(&not_full);
                    printf("Queue size increased to %d\n", current_queue_size);
                } else {
                    printf("Queue size reached maximum\n");
                }
                pthread_mutex_unlock(&mutex);
                break;
            case '-':
                pthread_mutex_lock(&mutex);
                if (current_queue_size > 1) {
                    current_queue_size--;
                    printf("Queue size decreased to %d\n", current_queue_size);
                } else {
                    printf("Queue size reached minimum\n");
                }
                pthread_mutex_unlock(&mutex);
                break;
            case 's':
                pthread_mutex_lock(&mutex);
                int used = (tail - head + FIXED_SIZE) % FIXED_SIZE;
                printf("Status:\n"
                       "  Producers: %d\n"
                       "  Consumers: %d\n"
                       "  Produced: %lu\n"
                       "  Consumed: %lu\n"
                       "  Queue size: %d\n"
                       "  Messages in queue: %d\n",
                       producers, consumers, producedCount, consumedCount,
                       current_queue_size, used);
                pthread_mutex_unlock(&mutex);
                break;
            default:
                printf("Unknown command '%c'\n", ch);
        }
    }

    for (int i = 0; i < producer_count; i++) pthread_cancel(producer_threads[i]);
    for (int i = 0; i < consumer_count; i++) pthread_cancel(consumer_threads[i]);
    for (int i = 0; i < producer_count; i++) pthread_join(producer_threads[i], NULL);
    for (int i = 0; i < consumer_count; i++) pthread_join(consumer_threads[i], NULL);

    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&not_full);
    pthread_cond_destroy(&not_empty);
    printf("Program finished\n");
    return 0;
}