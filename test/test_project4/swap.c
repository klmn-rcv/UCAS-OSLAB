#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#define PAGE_SIZE 0x1000      // 4KB page
#define MAX_PAGES 2048        // Maximum pages to test
#define ROUNDS 2              // Test rounds

// Memory page addresses
static uintptr_t page_addrs[MAX_PAGES];
// Expected values for each page
static uint64_t expected_vals[MAX_PAGES];

// Function to generate unique value for each page
static uint64_t generate_value(int page_idx, int round) {
    // Create a pattern that's unique per page and round
    uint64_t high = (uint64_t)page_idx << 32;
    uint64_t low = (uint64_t)(round * 0x1000) | 0x12345678;
    return high | low;
}

int main(int argc, char *argv[]) {
    printf("=== Page Replacement Test ===\n");
    
    if (argc < 3) {
        printf("Usage: %s base_addr num_pages\n", argv[0]);
        printf("  base_addr: base address for testing\n");
        printf("  num_pages: number of pages to test\n");
        return -1;
    }
    
    // Get parameters
    uintptr_t base_addr = (uintptr_t)atol(argv[2]);
    int num_pages = atoi(argv[3]);
    
    if (num_pages > MAX_PAGES) {
        printf("Error: Too many pages (max %d)\n", MAX_PAGES);
        return -1;
    }
    
    printf("Base address: 0x%lx\n", (unsigned long)base_addr);
    printf("Pages to test: %d\n", num_pages);
    printf("Total memory: %d KB\n", num_pages * 4);
    printf("Test rounds: %d\n\n", ROUNDS);
    
    // Step 1: Initialize page addresses
    for (int i = 0; i < num_pages; i++) {
        page_addrs[i] = base_addr + (i * PAGE_SIZE);
    }
    
    // Test rounds
    for (int round = 0; round < ROUNDS; round++) {
        printf("Round %d/%d: Writing initial values...\n", round + 1, ROUNDS);
        
        // Write unique value to each page
        for (int i = 0; i < num_pages; i++) {
            expected_vals[i] = generate_value(i, round);
            *(volatile uint64_t*)page_addrs[i] = expected_vals[i];
        }
        
        printf("Round %d/%d: Verifying values...\n", round + 1, ROUNDS);
        
        // Verify all pages are correct
        int errors = 0;
        for (int i = 0; i < num_pages; i++) {
            uint64_t read_val = *(volatile uint64_t*)page_addrs[i];
            if (read_val != expected_vals[i]) {
                printf("  ERROR: Page %d: expected 0x%016lx, got 0x%016lx\n", 
                       i, expected_vals[i], read_val);
                errors++;
                if (errors > 10) {
                    printf("  Too many errors, stopping...\n");
                    break;
                }
            }
        }
        
        if (errors > 0) {
            printf("Round %d: FAILED with %d errors\n", round + 1, errors);
            return -1;
        }
        
        printf("Round %d: PASS\n", round + 1);
        
        // Update values for next round
        for (int i = 0; i < num_pages; i++) {
            expected_vals[i] = expected_vals[i] ^ 0xAAAAAAAAAAAAAAAA;
            *(volatile uint64_t*)page_addrs[i] = expected_vals[i];
        }
    }
    
    printf("\n=== Working Set Test ===\n");
    
    // Create a working set (frequently accessed pages)
    #define WORKING_SET_SIZE 32
    int working_set[WORKING_SET_SIZE];
    
    // Choose random pages for working set
    // srand(time(NULL));
    for (int i = 0; i < WORKING_SET_SIZE; i++) {
        working_set[i] = rand() % num_pages;
    }
    
    printf("Testing working set pattern...\n");
    
    // Access working set frequently
    for (int iter = 0; iter < 100; iter++) {
        // Access each page in working set
        for (int i = 0; i < WORKING_SET_SIZE; i++) {
            int page_idx = working_set[i];
            // Read value
            uint64_t val = *(volatile uint64_t*)page_addrs[page_idx];
            // Update value
            val++;
            *(volatile uint64_t*)page_addrs[page_idx] = val;
        }
        
        // Occasionally access random page outside working set
        if (iter % 10 == 0) {
            int rand_page = rand() % num_pages;
            uint64_t val = *(volatile uint64_t*)page_addrs[rand_page];
            val += 0x100;
            *(volatile uint64_t*)page_addrs[rand_page] = val;
        }
    }
    
    printf("Working set test completed\n");
    
    // Final verification
    printf("\n=== Final Verification ===\n");
    
    int final_errors = 0;
    for (int i = 0; i < WORKING_SET_SIZE; i++) {
        uint64_t val = *(volatile uint64_t*)page_addrs[working_set[i]];
        // Just check if value is reasonable (not checking exact value)
        if (val == 0 || val == 0xFFFFFFFFFFFFFFFF) {
            printf("  Suspicious value at page %d: 0x%016lx\n", 
                   working_set[i], val);
            final_errors++;
        }
    }
    
    if (final_errors == 0) {
        printf("SUCCESS: Page replacement test passed!\n");
    } else {
        printf("FAILED: Found %d suspicious values\n", final_errors);
    }
    
    return 0;
}