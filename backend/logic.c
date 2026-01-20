#include "structures.h"
#include "heap.c" 

GlobalState state;
int id_counter = 1000;
int account_counter = 50000; // Start account numbers from 50000

void init_state() {
    state.tx_count = 0;
    state.customer_count = 0;
    state.processed_count = 0;
    state.cancelled_count = 0;
    state.total_wait_time = 0;
    
    state.priority_queue = create_heap(MAX_TRANSACTIONS, compare_priority);
    state.time_lock_queue = create_heap(MAX_TRANSACTIONS, compare_timelock);

    // Default Admin? No, admin role is separate from customers. 
    // Maybe seed one demo customer
}

Customer* create_customer(const char* name, int pin, int tier, double initial_balance) {
    if (state.customer_count >= MAX_CUSTOMERS) return NULL;
    
    Customer* c = &state.all_customers[state.customer_count++];
    c->account_number = account_counter++; // Simple Sequential
    strncpy(c->name, name, 49);
    c->pin = pin;
    c->tier = (CustomerTier)tier;
    c->balance = initial_balance;
    
    return c;
}

Customer* find_customer(int acc_no) {
    for(int i=0; i<state.customer_count; i++) {
        if (state.all_customers[i].account_number == acc_no) {
            return &state.all_customers[i];
        }
    }
    return NULL;
}

double calculate_base_priority(Transaction* t) {
    double p = (double)t->urgency * 2.0 
             + (double)t->tier * 1.5 
             - (double)t->risk_score * 0.5;
    if (p < 0) p = 0;
    return p;
}

// Returns: 0=Success, 1=InvalidSender, 2=InvalidReceiver, 3=AuthFail, 4=InsufficientFunds, 5=Full
int create_transaction(int sender_id, int receiver_id, int pin, double amount, int urgency_lvl) {
    if (state.tx_count >= MAX_TRANSACTIONS) return 5;

    Customer* sender = find_customer(sender_id);
    if (!sender) return 1;

    Customer* receiver = find_customer(receiver_id);
    if (!receiver) return 2;

    if (sender->pin != pin) return 3;
    if (sender->balance < amount) return 4;

    // Create Transaction
    Transaction* t = &state.all_transactions[state.tx_count++];
    t->id = id_counter++;
    t->sender_id = sender_id;
    t->receiver_id = receiver_id;
    t->amount = amount;
    t->urgency = (UrgencyLevel)urgency_lvl;
    t->tier = sender->tier; // Inherit tier from sender
    t->risk_score = 0; // Default
    t->arrival_time = time(NULL);
    t->base_priority = calculate_base_priority(t);
    t->effective_priority = t->base_priority;
    t->status = STATUS_WAITING;

    // Logic: Time Lock if High Value
    if (t->amount >= TIME_LOCK_THRESHOLD) {
        t->status = STATUS_LOCKED;
        t->unlock_time = t->arrival_time + TIME_LOCK_DURATION;
        heap_push(state.time_lock_queue, t);
    } else {
        heap_push(state.priority_queue, t);
    }

    return 0; // OK
}

void update_system_state() {
    time_t now = time(NULL);

    // 1. Time Lock -> PQ
    while (state.time_lock_queue->size > 0) {
        Transaction* top = heap_peek(state.time_lock_queue);
        if (top->unlock_time <= now) {
            heap_pop(state.time_lock_queue); 
            if (top->status != STATUS_CANCELLED) {
                top->status = STATUS_WAITING;
                heap_push(state.priority_queue, top); 
            }
        } else {
            break; 
        }
    }

    // 2. Aging
    for (int i = 0; i < state.priority_queue->size; i++) {
        Transaction* t = state.priority_queue->data[i];
        double wait_seconds = difftime(now, t->arrival_time);
        t->effective_priority = t->base_priority + (wait_seconds * AGING_FACTOR);
    }
    
    // Lazy Re-heapify
    for (int i = (state.priority_queue->size / 2) - 1; i >= 0; i--) {
        heapify_down(state.priority_queue, i);
    }
}

Transaction* process_next_transaction() {
    Transaction* t = heap_pop(state.priority_queue);
    
    // Process the money transfer here!
    if (t) {
        Customer* sender = find_customer(t->sender_id);
        Customer* receiver = find_customer(t->receiver_id);

        if (sender && receiver && sender->balance >= t->amount) {
            sender->balance -= t->amount;
            receiver->balance += t->amount;
            t->status = STATUS_DONE; 
        } else {
            // Failed during processing (e.g. money spent elsewhere while waiting)
            t->status = STATUS_CANCELLED; 
        }

        state.processed_count++;
        state.total_wait_time += difftime(time(NULL), t->arrival_time);
    }
    return t;
}

void cancel_transaction(int id) {
    Transaction* target = NULL;
    for(int i=0; i<state.tx_count; i++) {
        if (state.all_transactions[i].id == id) {
            target = &state.all_transactions[i];
            break;
        }
    }

    if (!target) return;
    if (target->status == STATUS_DONE || target->status == STATUS_CANCELLED) return;

    if (target->status == STATUS_LOCKED) {
        heap_remove(state.time_lock_queue, id);
    } else if (target->status == STATUS_WAITING) {
        heap_remove(state.priority_queue, id);
    }

    target->status = STATUS_CANCELLED;
    state.cancelled_count++;
}

void force_unlock(int id) {
     Transaction* target = NULL;
    for(int i=0; i<state.tx_count; i++) {
        if (state.all_transactions[i].id == id) {
            target = &state.all_transactions[i];
            break;
        }
    }
    if (!target || target->status != STATUS_LOCKED) return;
    
    heap_remove(state.time_lock_queue, id);
    target->status = STATUS_WAITING;
    heap_push(state.priority_queue, target);
}
