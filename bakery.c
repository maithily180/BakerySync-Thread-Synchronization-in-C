// pthread-based Bakery simulation
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>

#define MAXN 1024
#define MAX_CAPACITY 25
#define SOFA_CAPACITY 4
#define NUM_CHEFS 4

typedef struct Customer {
    int id;
    int arrival;
    int index;              // 0..n-1
} Customer;

typedef struct Queue {
    int data[MAXN];
    int head;
    int tail;
    int count;
} Queue;

static Customer *customers[MAXN];
static int num_customers;

// Queues for baking and payment
static Queue bake_q;            // customers who requested cake
static Queue pay_q;             // customers who are ready to pay

// Synchronization
static pthread_mutex_t q_mutex = PTHREAD_MUTEX_INITIALIZER;   // protects queues and register flag
static pthread_cond_t q_cond = PTHREAD_COND_INITIALIZER;      // signals new work
static int cash_register_busy = 0;                             // single register flag

static sem_t sofa_sem;          // sofa capacity
static sem_t inside_sem;        // inside capacity (store capacity)

// Per-customer semaphores
static sem_t cake_ready[MAXN];  // signaled by chef after baking
static sem_t pay_done[MAXN];    // signaled by chef after accepting payment

// Time origin and printing offset
static struct timeval t0;
static int g_start_offset = 0;

static int seconds_since_start(void) {
    struct timeval now; gettimeofday(&now, NULL);
    long sec = now.tv_sec - t0.tv_sec;
    long usec = now.tv_usec - t0.tv_usec;
    if (usec < 0) { sec -= 1; usec += 1000000; }
    return (int)sec;
}

static int now_ts(void) {
    return seconds_since_start() + g_start_offset;
}

static void print_ts(const char *actor, int id, const char *action) {
    printf("%d %s %d %s\n", now_ts(), actor, id, action);
    fflush(stdout);
}

static void q_init(Queue *q) { q->head = q->tail = q->count = 0; }
static void q_push(Queue *q, int v) { q->data[q->tail++] = v; if (q->tail >= MAXN) q->tail = 0; q->count++; }
static int q_pop(Queue *q) { if (q->count == 0) return -1; int v = q->data[q->head++]; if (q->head >= MAXN) q->head = 0; q->count--; return v; }

static void *customer_thread(void *arg) {
    Customer *c = (Customer *)arg;
    int id = c->id;
    // sleep until arrival
    sleep(c->arrival);
    // try to enter store (capacity MAX_CAPACITY)
    sem_wait(&inside_sem);
    print_ts("Customer", id, "enters");

    // sit on sofa (capacity 4)
    sem_wait(&sofa_sem);
    print_ts("Customer", id, "sits");

    // after 1s: request cake
    sleep(1);
    print_ts("Customer", id, "requests cake");
    pthread_mutex_lock(&q_mutex);
    q_push(&bake_q, id);
    pthread_cond_broadcast(&q_cond);
    pthread_mutex_unlock(&q_mutex);

    // wait until cake is ready (chef signals)
    sem_wait(&cake_ready[c->index]);

    // 1s later pay
    sleep(1);
    print_ts("Customer", id, "pays");
    pthread_mutex_lock(&q_mutex);
    q_push(&pay_q, id);
    pthread_cond_broadcast(&q_cond);
    pthread_mutex_unlock(&q_mutex);

    // wait until chef accepts payment
    sem_wait(&pay_done[c->index]);
    print_ts("Customer", id, "leaves");

    // free sofa and store capacity
    sem_post(&sofa_sem);
    sem_post(&inside_sem);
    return NULL;
}

static void *chef_thread(void *arg) {
    long chef_no = (long)arg; // 1..NUM_CHEFS
    while (1) {
        pthread_mutex_lock(&q_mutex);
        // wait for any work
        while (pay_q.count == 0 && bake_q.count == 0) {
            pthread_cond_wait(&q_cond, &q_mutex);
        }

        // prefer payment if register is free and payment queue not empty
        if (pay_q.count > 0 && cash_register_busy == 0) {
            int cid = q_pop(&pay_q);
            cash_register_busy = 1; // lock register
            pthread_mutex_unlock(&q_mutex);

            // accept payment
            printf("%d Chef %ld accepts payment for Customer %d\n", now_ts(), chef_no, cid);
            fflush(stdout);
            sleep(2);

            // signal customer payment done
            // find index: we store per-customer sem by index; find matching
            int idx = -1;
            for (int i = 0; i < num_customers; i++) if (customers[i]->id == cid) { idx = customers[i]->index; break; }
            if (idx != -1) sem_post(&pay_done[idx]);

            pthread_mutex_lock(&q_mutex);
            cash_register_busy = 0;
            pthread_cond_broadcast(&q_cond);
            pthread_mutex_unlock(&q_mutex);
            continue;
        }

        // otherwise bake if there is any
        if (bake_q.count > 0) {
            int cid = q_pop(&bake_q);
            pthread_mutex_unlock(&q_mutex);

            // bake for 2s
            printf("%d Chef %ld bakes for Customer %d\n", now_ts(), chef_no, cid);
            fflush(stdout);
            sleep(2);

            // signal cake ready to that customer
            int idx = -1;
            for (int i = 0; i < num_customers; i++) if (customers[i]->id == cid) { idx = customers[i]->index; break; }
            if (idx != -1) sem_post(&cake_ready[idx]);

            // loop
            continue;
        }

        // if reached here, loop to wait again
        pthread_mutex_unlock(&q_mutex);
    }
    return NULL;
}

int main(void) {
    char line[256];
    while (fgets(line, sizeof(line), stdin)) {
        if (strncmp(line, "<EOF>", 5) == 0) break;
        int ts = 0, id = 0;
        if (sscanf(line, "%d Customer %d", &ts, &id) == 2) {
            Customer *c = (Customer *)calloc(1, sizeof(Customer));
            c->id = id; c->arrival = ts; c->index = num_customers;
            customers[num_customers++] = c;
        }
    }
    if (num_customers == 0) return 0;

    // sort by arrival to compute base time offset (for printing start)
    for (int i = 0; i < num_customers; i++) {
        for (int j = i + 1; j < num_customers; j++) {
            if (customers[j]->arrival < customers[i]->arrival) {
                Customer *tmp = customers[i]; customers[i] = customers[j]; customers[j] = tmp;
                customers[i]->index = i; customers[j]->index = j;
            }
        }
    }
    // normalize time origin to earliest arrival, and keep offset for printing
    int start_offset = customers[0]->arrival;
    g_start_offset = start_offset;
    for (int i = 0; i < num_customers; i++) customers[i]->arrival -= start_offset;

    // init time origin
    gettimeofday(&t0, NULL);

    // init sync primitives
    sem_init(&sofa_sem, 0, SOFA_CAPACITY);
    sem_init(&inside_sem, 0, MAX_CAPACITY);
    for (int i = 0; i < num_customers; i++) {
        sem_init(&cake_ready[i], 0, 0);
        sem_init(&pay_done[i], 0, 0);
    }
    q_init(&bake_q); q_init(&pay_q);

    // spawn chef threads
    pthread_t chefs[NUM_CHEFS];
    for (long i = 1; i <= NUM_CHEFS; i++) {
        pthread_create(&chefs[i - 1], NULL, chef_thread, (void *)i);
    }

    // spawn customer threads
    pthread_t threads[MAXN];
    for (int i = 0; i < num_customers; i++) {
        pthread_create(&threads[i], NULL, customer_thread, (void *)customers[i]);
    }

    // join customers
    for (int i = 0; i < num_customers; i++) pthread_join(threads[i], NULL);

    // all customers done; exit program. Chefs will be force-terminated by process exit.
    // In a more elaborate design, we'd signal chefs to exit; but not required here.
    return 0;
}
