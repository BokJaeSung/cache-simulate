
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>

#define MISS_PENALTY 200
#define HIT_CYCLE 1

#define MEMORY_SIZE 512
#define WORD_SIZE 4

// argv value
int cache_size;
int block_size;
int way_size;
FILE *trc_fp;

// how many words in one block
int word_num_in_one_block;
// how many sets(lines) int one cache
int set_num_in_one_cache;
// how many blocks in one cache
int block_num_in_one_cache;
// count of memory access
int count_memory_access;

// if blocksize==wordsize, =>1 else =>0
int IsBlocksizeWord;

// LRU value , lowest cache line will evict.
int LRU;

// total values
int total_hit_count;
int total_miss_count;
int total_dirty;

// u_int : unsinged int
typedef unsigned int u_int;

typedef struct one_line
{
    int valid;
    int dirty;
    int tag;
    int *data;
    int lru;
    int addr;
} one_line;

typedef struct memory_array
{
    int valid;
    int *data;
    u_int addr;
} memory_array;

// struct implementation
one_line *cache;
memory_array *memory;

void init_cache(int argc, char *argv[]);

void W_cache(u_int addr, int data);
void R_cache(u_int addr);

void R_memory(u_int addr, one_line *line_ptr);
void W_memory(one_line *line_ptr);

void board();
void free_func();

/**
 * @brief main()
 */
int main(int argc, char *argv[])
{
    init_cache(argc, argv);
    board();
    free_func();
    return 0;
}

/**
 * @brief init_cache()
 * read from argv
 * read from trc File
 */
void init_cache(int argc, char *argv[])
{
    int option;
    while ((option = getopt(argc, argv, "s:b:a:f:")) != -1)
    {
        switch (option)
        {
        case 's':
            cache_size = atoi((char *)++optarg);
            break;
        case 'b':
            block_size = atoi((char *)++optarg);
            break;
        case 'a':
            way_size = atoi((char *)++optarg);
            break;
        case 'f':
            trc_fp = fopen(++optarg, "r");
            break;
        case '?':
            printf("more parameter or less parameter\n");
            break;
        }
    }
    set_num_in_one_cache = cache_size / (block_size * way_size);
    // for example: c_size 64 ,b_size 8, 2-way,  num of block :8 and , num of set= 4(8/2)
    word_num_in_one_block = block_size / WORD_SIZE;
    // for example: b_size 16 , word num in one block is 4.
    block_num_in_one_cache = way_size * set_num_in_one_cache;
    LRU = 0;

    // check block_size==WORD_SIZE,
    if (block_size == WORD_SIZE)
        IsBlocksizeWord = 1;
    else
        IsBlocksizeWord = 0;

    // build cache
    cache = (one_line *)calloc(block_num_in_one_cache, sizeof(one_line));
    for (int block_index = 0; block_index < block_num_in_one_cache; block_index++)
    {
        cache[block_index].data = (int *)calloc(word_num_in_one_block, sizeof(int));
    }

    // build memory
    memory = (memory_array *)calloc(MEMORY_SIZE, sizeof(memory_array));
    for (int memory_index = 0; memory_index < MEMORY_SIZE; memory_index++)
    {
        memory[memory_index].data = (int *)calloc(word_num_in_one_block, sizeof(int));
        memory[memory_index].valid = 0;
    }
    // use u_int for address
    u_int address;
    int W_data;
    char access_type;
    while (fscanf(trc_fp, "%8x %c", &address, &access_type) != EOF)
    {
        LRU++;
        if (access_type == 'W')
        {
            fscanf(trc_fp, "%d", &W_data);
            W_cache(address, W_data);
        }
        else if (access_type == 'R')
        {
            R_cache(address);
        }
        else
        {
            printf("wrong type of access\n");
            exit(1);
        }
    }
}

/**
 * @brief W_cache()
 * R_memory(),W_memory() available in this func.
 */
void W_cache(u_int addr, int data)
{
    int current_way, new_set_block_index;
    u_int set_index, block_index, word_index, addr_tag;
    one_line *line_ptr;

    /**
     * @param set_index
     * we remove block_size for offset of block...maybe word offset or byte offset.
     * so set_index mean "index that addr should go"
     * @param block_index
     * for example if set_index=3, and 4-way, start position(block_index)is 12nd.
     * @param addr_tag
     *
     */
    set_index = (addr / block_size) % set_num_in_one_cache;
    block_index = set_index * way_size;
    addr_tag = ((addr / block_size) / set_num_in_one_cache);
    word_index = ((addr / 4) % word_num_in_one_block);

    new_set_block_index = way_size;
    // way_size
    for (current_way = 0; current_way < way_size; current_way++)
    {
        line_ptr = cache + block_index + current_way;
        if (line_ptr->valid && line_ptr->tag == addr_tag)
        {
            total_hit_count++;
            line_ptr->dirty = 1;
            line_ptr->addr = addr;
            line_ptr->lru = LRU;
            line_ptr->data[word_index] = data;
            return;
        }
        else if (line_ptr->valid == 0 && current_way < new_set_block_index)
        {
            new_set_block_index = current_way;
        }
    }
    total_miss_count++;
    if (new_set_block_index == way_size)
    {
        int oldest_block_lru = INT_MAX;
        for (current_way = 0; current_way < way_size; current_way++)
        {
            line_ptr = cache + block_index + current_way;
            if (line_ptr->lru < oldest_block_lru)
            {
                oldest_block_lru = line_ptr->lru;
                new_set_block_index = current_way;
            }
        }
        line_ptr = cache + block_index + new_set_block_index;

        // dirty bit==1 evcited data to memory
        if (line_ptr->dirty)
        {
            W_memory(line_ptr);
        }

        if (!IsBlocksizeWord)
        {
            R_memory(addr, line_ptr);
        }
    }
    else
    {
        line_ptr = cache + block_index + new_set_block_index;

        if (!IsBlocksizeWord)
        {
            R_memory(addr, line_ptr);
        }
    }

    line_ptr->dirty = 1;
    line_ptr->valid = 1;
    line_ptr->tag = ((addr / block_size) / set_num_in_one_cache);
    line_ptr->data[(addr / 4) % word_num_in_one_block] = data;
    line_ptr->addr = addr;
    line_ptr->lru = LRU;
}
/**
 * @brief R_memory()
 */
void R_memory(u_int addr, one_line *line_ptr)
{
    u_int addr_finding = addr / block_size;
    count_memory_access++;
    for (int memory_index = 0; memory_index < MEMORY_SIZE; memory_index++)
    {
        if (memory[memory_index].valid && memory[memory_index].addr == addr_finding)
        {
            memcpy(line_ptr->data, memory[memory_index].data, sizeof(int) * word_num_in_one_block);
            return;
        }
    }
    memset(line_ptr->data, 0, sizeof(int) * word_num_in_one_block);
}
/**
 * @brief R_cache()
 * R_memory(),W_memory() available in this func.
 */
void R_cache(u_int addr)
{
    int new_set_block_index, current_way;
    u_int set_index, block_index, addr_tag;
    one_line *line_ptr;
    /**
     * @param set_index
     * we remove block_size for offset of block...maybe word offset or byte offset.
     * so set_index mean "index that addr should go"
     * @param block_index
     * for example if set_index=3, and 4-way, start position(block_index)is 12nd.
     * @param addr_tag
     *
     */
    set_index = (addr / block_size) % set_num_in_one_cache;
    block_index = set_index * way_size;
    new_set_block_index = way_size;
    addr_tag = ((addr / block_size) / set_num_in_one_cache);

    for (current_way = 0; current_way < way_size; current_way++)
    {
        line_ptr = cache + block_index + current_way;
        if (line_ptr->valid && line_ptr->tag == addr_tag)
        {
            total_hit_count++;
            line_ptr->lru = LRU;
            return;
        }
        else if (line_ptr->valid == 0 && new_set_block_index > current_way)
        {
            new_set_block_index = current_way;
        }
    }
    total_miss_count++;

    if (new_set_block_index == way_size)
    {
        int oldest_block_lru = INT_MAX;
        for (current_way = 0; current_way < way_size; current_way++)
        {
            line_ptr = cache + block_index + current_way;
            if (line_ptr->lru < oldest_block_lru)
            {
                oldest_block_lru = line_ptr->lru;
                new_set_block_index = current_way;
            }
        }
        line_ptr = cache + block_index + new_set_block_index;
        if (line_ptr->dirty)
        {
            W_memory(line_ptr);
        }
    }
    else // Dont have to evict (cause empty place found.)
    {
        line_ptr = cache + block_index + new_set_block_index;
    }

    line_ptr->dirty = 0;
    line_ptr->valid = 1;
    line_ptr->addr = (addr / block_size);
    line_ptr->tag = ((addr / block_size) / set_num_in_one_cache);
    line_ptr->lru = LRU;

    R_memory(addr, line_ptr);
}
/**
 * @brief W_memory()
 *  save memory (may be evicted cache data)
 */
void W_memory(one_line *line_ptr)
{
    u_int evict_addr = line_ptr->addr / block_size;

    count_memory_access++;
    for (int memory_index = 0; memory_index < MEMORY_SIZE; memory_index++)
    {
        if (memory[memory_index].valid)
        {
            if (memory[memory_index].addr == evict_addr)
            {
                memcpy(memory[memory_index].data, line_ptr->data, sizeof(int) * word_num_in_one_block);
                return;
            }
        }
        else
        {
            memory[memory_index].valid = 1;
            memory[memory_index].addr = evict_addr;
            memcpy(memory[memory_index].data, line_ptr->data, sizeof(int) * word_num_in_one_block);
            return;
        }
    }
}
/**
 * @brief free all element(memory, cache, data..)
 */
void free_func()
{
    for (int index = 0; index < block_size / 4; index++)
        free(cache[index].data);

    for (int index = 0; index < MEMORY_SIZE; index++)
        free(memory[index].data);

    free(cache);
    free(memory);

    fclose(trc_fp);
}
/**
 * @brief print all total_value
 */
void board()
{
    one_line *line_ptr;
    int total_count = total_hit_count + total_miss_count;
    int set_index, current_way, cache_line_offset, word_index;
    double miss_rate, average_memory_access_cycle;

    miss_rate = ((double)total_miss_count / total_count) * 100;
    average_memory_access_cycle = (double)(count_memory_access * MISS_PENALTY + total_count * HIT_CYCLE) / total_count;

    for (set_index = 0; set_index < set_num_in_one_cache; set_index++)
    {
        printf("%d: ", set_index);
        cache_line_offset = set_index * way_size;

        for (current_way = 0; current_way < way_size; current_way++)
        {
            if (current_way)
                printf("   ");

            if ((line_ptr = cache + (cache_line_offset + current_way))->dirty)
                total_dirty++;

            for (word_index = 0; word_index < word_num_in_one_block; word_index++)
                printf("%08X ", (line_ptr->data)[word_index]);

            printf("v:%d d:%d\n", line_ptr->valid, line_ptr->dirty);
        }
    }

    printf("\ntotal number of hits: %d\n", total_hit_count);
    printf("total number of misses: %d\n", total_miss_count);
    printf("miss rate: %.1f%%\n", miss_rate);
    printf("total number of dirty blocks: %d\n", total_dirty);
    printf("average memory access cycle: %.1f\n", average_memory_access_cycle);
}
