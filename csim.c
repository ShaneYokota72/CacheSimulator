#include <getopt.h>  // getopt, optarg
#include <stdlib.h>  // exit, atoi, malloc, free
#include <stdio.h>   // printf, fprintf, stderr, fopen, fclose, FILE
#include <limits.h>  // ULONG_MAX
#include <string.h>  // strcmp, strerror
#include <errno.h>   // errno
#include <stdbool.h> // for bool value
#include <limits.h> // for INT_MAX

/* fast base-2 integer logarithm */
#define INT_LOG2(x) (31 - __builtin_clz(x))
#define NOT_POWER2(x) (__builtin_clz(x) + __builtin_ctz(x) != 31)

/* tag_bits = ADDRESS_LENGTH - set_bits - block_bits */
#define ADDRESS_LENGTH 64

/**
 * Print program usage (no need to modify).
 */
static void print_usage() {
    printf("Usage: csim [-hv] -S <num> -K <num> -B <num> -p <policy> -t <file>\n");
    printf("Options:\n");
    printf("  -h           Print this help message.\n");
    printf("  -v           Optional verbose flag.\n");
    printf("  -S <num>     Number of sets.           (must be > 0)\n");
    printf("  -K <num>     Number of lines per set.  (must be > 0)\n");
    printf("  -B <num>     Number of bytes per line. (must be > 0)\n");
    printf("  -p <policy>  Eviction policy. (one of 'FIFO', 'LRU')\n");
    printf("  -t <file>    Trace file.\n\n");
    printf("Examples:\n");
    printf("$ ./csim    -S 16  -K 1 -B 16 -p LRU -t traces/yi2.trace\n");
    printf("$ ./csim -v -S 256 -K 2 -B 16 -p LRU -t traces/yi2.trace\n");
}

/* Parameters set by command-line args (no need to modify) */
int verbose = 0;   // print trace if 1
int S = 0;         // number of sets
int K = 0;         // lines per set
int B = 0;         // bytes per line

typedef enum { FIFO = 1, LRU = 2 } Policy;
Policy policy;     // 0 (undefined) by default

FILE *trace_fp = NULL;

/**
 * Parse input arguments and set verbose, S, K, B, policy, trace_fp.
 */
static void parse_arguments(int argc, char **argv) {
    char c;
    while ((c = getopt(argc, argv, "S:K:B:p:t:vh")) != -1) {
        switch(c) {
            case 'S':
                S = atoi(optarg);
                if (NOT_POWER2(S)) {
                    fprintf(stderr, "ERROR: S must be a power of 2\n");
                    exit(1);
                }
                break;
            case 'K':
                K = atoi(optarg);
                if(K <= 0){
                    fprintf(stderr, "ERROR: K must be a number larget than 0\n");
                    exit(1);
                }
                break;
            case 'B':
                B = atoi(optarg);
                if(NOT_POWER2(B)){
                    fprintf(stderr, "ERROR: B must be a power of 2\n");
                    exit(1);
                }
                break;
            case 'p':
                if (!strcmp(optarg, "FIFO")) {
                    policy = FIFO;
                }
                if(!strcmp(optarg, "LRU")) {
                    policy = LRU;
                }

                if(policy == 0){
                    fprintf(stderr, "ERROR: Policy must be FIFO or LRU\n");
                    exit(1);
                }
                break;
            case 't':
                trace_fp = fopen(optarg, "r");
                if (!trace_fp) {
                    fprintf(stderr, "ERROR: %s: %s\n", optarg, strerror(errno));
                    exit(1);
                } 

                break;
            case 'v':
                verbose = 1;
                break;
            case 'h':
                print_usage();
                exit(0);
            default:
                print_usage();
                exit(1);
        }
    }

    /* Make sure that all required command line args were specified and valid */
    if (S <= 0 || K <= 0 || B <= 0 || policy == 0 || !trace_fp) {
        printf("ERROR: Negative or missing command line arguments\n");
        print_usage();
        if (trace_fp) {
            fclose(trace_fp);
        }
        exit(1);
    }

    /* Other setup if needed */



}

/**
 * Cache data structures
 */
struct Block {
    bool Valid;
    long tag;
    int trackID;
};

// once all the lines are made for each set
struct Set{
    struct Block* blockInSet;
};

struct Cache{
    struct Set* sets;
};

struct Cache* currCache;


/**
 * Allocate cache data structures.
 *
 * This function dynamically allocates (with malloc) data structures for each of
 * the `S` sets and `K` lines per set.
 *
 */
static void allocate_cache() {

    currCache = malloc(sizeof(struct Cache));
    currCache->sets = malloc(S*sizeof(struct Set));
    for(int i=0; i<S; i++){
        currCache->sets[i].blockInSet = malloc(K*sizeof(struct Block));
        for(int j=0; j<K; j++){
            currCache->sets[i].blockInSet[j].Valid = false;
        }
    }

}

/**
 * Deallocate cache data structures.
 *
 * This function deallocates (with free) the cache data structures of each
 * set and line.
 *
 */
static void free_cache() {

    for(int i=0; i<S; i++){
        free(currCache->sets[i].blockInSet);
    }
    free(currCache->sets);
    free(currCache);

}

/* Counters used to record cache statistics */
int miss_count     = 0;
int hit_count      = 0;
int eviction_count = 0;

int count = 0;

/**
 * Simulate a memory access.
 *
 * If the line is already in the cache, increase `hit_count`; otherwise,
 * increase `miss_count`; increase `eviction_count` if another line must be
 * evicted. This function also updates the metadata used to implement eviction
 * policies (LRU, FIFO).
 *
 */
static void access_data(unsigned long addr) {
    // printf("Access to %016lx\n", addr);

    // get set index
    unsigned int mask = (1 << INT_LOG2(S)) - 1;
    long setIndex = (addr >> INT_LOG2(B)) & mask;
    
    // get tag bits
    long tagBits = addr >> (INT_LOG2(S) + INT_LOG2(B));

    // printf("SI:%ld  tagBits:%ld\n", setIndex, tagBits);

    // simulate cache access
    bool allValid = true; // if end up as true, the cache is full
    bool didHit = false;
    int firstEmptySlot = 0;
    int hitBlockIndex;

    struct Set currSet = currCache->sets[setIndex];
    for(int i=0; i<K; i++){
        struct Block currBlock = currSet.blockInSet[i];
        bool currValid = currBlock.Valid;
        long currTag = currBlock.tag;

        if(allValid && currValid == false){
            firstEmptySlot = i;
            allValid = false;
        }

        if((currTag == tagBits) && (currValid == 1)){
            didHit = true;
            hitBlockIndex = i;
            break;
        }
    }

    //  take care of evict, miss, hit, etc
    if(didHit){

        // update cache data
        // if(policy == 1){} // FIFO do nothing
        if(policy == 2){
            currSet.blockInSet[hitBlockIndex].trackID = count;
        }

        // update global count
        hit_count++;
    } else if (allValid){ // the cache block is full and didnt hit

        // update cache data
        int minTrackID = INT_MAX;
        int setIndexToEvict;
        for(int i=0; i<K; i++){
            if(currSet.blockInSet[i].trackID < minTrackID){
                minTrackID = currSet.blockInSet[i].trackID;
                setIndexToEvict = i;
            }
        }
        currSet.blockInSet[setIndexToEvict].Valid = 1;
        currSet.blockInSet[setIndexToEvict].tag = tagBits;
        currSet.blockInSet[setIndexToEvict].trackID = count;

        // update global count
        miss_count++;
        eviction_count++;
    } else { // the cahce block is not full and didnt hit

        // update cache data
        currSet.blockInSet[firstEmptySlot].Valid = 1;
        currSet.blockInSet[firstEmptySlot].tag = tagBits;
        currSet.blockInSet[firstEmptySlot].trackID = count;

        // update global count
        miss_count++;
    }
        
}

/**
 * Replay the input trace.
 *
 * This function:
 * - reads lines (e.g., using fgets) from the file handle `trace_fp` (a global variable)
 * - skips lines not starting with ` S`, ` L` or ` M`
 * - parses the memory address (unsigned long, in hex) and len (unsigned int, in decimal)
 *   from each input line
 * - calls `access_data(address)` for each access to a cache line
 *
 */
static void replay_trace() {


    char op[50];
    while(fgets(op, 50, trace_fp)){
        if(op[0] != ' '){
            continue;
        }

        char operation;
        unsigned long value; // the hex address in long
        unsigned int len; // the bit its trying to access from the ${value}

        char* temp = strtok(op, " ,");
        for(int i=0; i<3; i++){
            if(i == 0){
                operation = temp[0];
            }

            if(i == 1){
                value = strtoul(temp, NULL, 16);
            }

            if(i == 2){
                len = atoi(temp);
            }

            temp = strtok(NULL, " ,");

        }


        long prevSetVal = -1;
        for(unsigned int i=0; i<len; i++){
            unsigned int mask = (1 << INT_LOG2(B)) - 1;
            long setValue = ((value+i) >> INT_LOG2(B)) & mask;

            if(prevSetVal != setValue){
                access_data(value+i);
                count++;
                prevSetVal = setValue;
            }
        }
        if(operation == 'M'){
            prevSetVal = -1;
            for(unsigned int i=0; i<len; i++){
                unsigned int mask = (1 << INT_LOG2(S)) - 1;
                long setValue = ((value+i) >> INT_LOG2(B)) & mask;

                if(prevSetVal != setValue){
                    access_data(value+i);
                    count++;
                    prevSetVal = setValue;
                }
            }
        }
    }
}

/**
 * Print cache statistics (DO NOT MODIFY).
 */
static void print_summary(int hits, int misses, int evictions) {
    printf("hits:%d misses:%d evictions:%d\n", hits, misses, evictions);
}

int main(int argc, char **argv) {
    parse_arguments(argc, argv);  // set global variables used by simulation
    allocate_cache();             // allocate data structures of cache
    replay_trace();               // simulate the trace and update counts
    free_cache();                 // deallocate data structures of cache
    fclose(trace_fp);             // close trace file
    print_summary(hit_count, miss_count, eviction_count);  // print counts
    return 0;
}
