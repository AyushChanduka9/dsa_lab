#include "structures.h"

// Initialize a heap
Heap* create_heap(int capacity, int (*compare)(const void* a, const void* b)) {
    Heap* h = (Heap*)malloc(sizeof(Heap));
    h->size = 0;
    h->capacity = capacity;
    h->compare = compare;
    // data is array of pointers, already allocated as part of struct if fixed, 
    // but here we used define MAX_TRANSACTIONS in struct? 
    // Wait, in structures.h: Transaction* data[MAX_TRANSACTIONS]; 
    // So no dynamic allocation needed for the array itself.
    return h;
}

void swap(Transaction** a, Transaction** b) {
    Transaction* temp = *a;
    *a = *b;
    *b = temp;
}

// Compare for Priority Queue (Max Heap based on effective_priority)
int compare_priority(const void* a, const void* b) {
    Transaction* t1 = *(Transaction**)a;
    Transaction* t2 = *(Transaction**)b;
    if (t1->effective_priority > t2->effective_priority) return 1;
    if (t1->effective_priority < t2->effective_priority) return -1;
    // Tie-breaker: Arrival time (FIFO)
    if (t1->arrival_time < t2->arrival_time) return 1; 
    return 0;
}

// Compare for Time Lock Queue (Min Heap based on unlock_time)
int compare_timelock(const void* a, const void* b) {
    Transaction* t1 = *(Transaction**)a;
    Transaction* t2 = *(Transaction**)b;
    // We want smallest unlock_time at top
    if (t1->unlock_time < t2->unlock_time) return 1; 
    if (t1->unlock_time > t2->unlock_time) return -1;
    return 0;
}

void heapify_up(Heap* h, int index) {
    int parent = (index - 1) / 2;
    if (index > 0 && h->compare(&h->data[index], &h->data[parent]) > 0) {
        swap(&h->data[index], &h->data[parent]);
        heapify_up(h, parent);
    }
}

void heapify_down(Heap* h, int index) {
    int left = 2 * index + 1;
    int right = 2 * index + 2;
    int largest = index;

    if (left < h->size && h->compare(&h->data[left], &h->data[largest]) > 0)
        largest = left;
    
    if (right < h->size && h->compare(&h->data[right], &h->data[largest]) > 0)
        largest = right;

    if (largest != index) {
        swap(&h->data[index], &h->data[largest]);
        heapify_down(h, largest);
    }
}

void heap_push(Heap* h, Transaction* t) {
    if (h->size >= MAX_TRANSACTIONS) return; // Full
    h->data[h->size] = t;
    heapify_up(h, h->size);
    h->size++;
}

Transaction* heap_pop(Heap* h) {
    if (h->size == 0) return NULL;
    Transaction* root = h->data[0];
    h->data[0] = h->data[h->size - 1];
    h->size--;
    heapify_down(h, 0);
    return root;
}

Transaction* heap_peek(Heap* h) {
    if (h->size == 0) return NULL;
    return h->data[0];
}

// Helper to remove a specific transaction (middle of heap) - O(N) scan, O(logN) fix
// Needed for cancellation
void heap_remove(Heap* h, int tx_id) {
    int i;
    for (i = 0; i < h->size; i++) {
        if (h->data[i]->id == tx_id) break;
    }
    if (i == h->size) return; // Not found

    h->data[i] = h->data[h->size - 1];
    h->size--;
    // We might need to go up or down depending on the value
    // Easiest is to try both or compare with parent
    heapify_down(h, i);
    heapify_up(h, i); 
}
