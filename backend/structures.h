#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

// --- CONSTANTS ---
#define MAX_TRANSACTIONS 1000
#define MAX_CUSTOMERS 100
#define TIME_LOCK_THRESHOLD 10000.0 
#define TIME_LOCK_DURATION 30       
#define AGING_FACTOR 0.5            

typedef enum {
    ROLE_CUSTOMER,
    ROLE_ADMIN
} UserRole;

typedef enum {
    URGENCY_NORMAL = 0,
    URGENCY_EMI = 50,
    URGENCY_MEDICAL = 100
} UrgencyLevel;

typedef enum {
    TIER_BASIC = 0,
    TIER_PREMIUM = 20,
    TIER_VIP = 50,
    TIER_ELITE = 80 // Added ELITE as requested
} CustomerTier;

typedef enum {
    STATUS_WAITING,     
    STATUS_LOCKED,      
    STATUS_PROCESSING,  
    STATUS_DONE,        
    STATUS_CANCELLED    
} TxStatus;

// --- DATA STRUCTURES ---

typedef struct {
    int account_number; // Acts as ID
    char name[50];
    int pin;
    double balance;
    CustomerTier tier;
} Customer;

typedef struct {
    int id;
    int sender_id;
    int receiver_id;
    double amount;
    UrgencyLevel urgency;
    CustomerTier tier; // Snapshot of sender's tier at time of tx
    int risk_score; 

    time_t arrival_time;
    time_t unlock_time; 

    double base_priority;
    double effective_priority; 

    TxStatus status;
} Transaction;

typedef struct {
    Transaction* data[MAX_TRANSACTIONS];
    int size;
    int capacity;
    int (*compare)(const void* a, const void* b); 
} Heap;

typedef struct {
    Transaction all_transactions[MAX_TRANSACTIONS];
    int tx_count;

    Customer all_customers[MAX_CUSTOMERS];
    int customer_count;
    
    Heap* priority_queue;  
    Heap* time_lock_queue; 

    // Statistics
    int processed_count;
    int cancelled_count;
    double total_wait_time;
} GlobalState;

extern GlobalState state;

#endif
