//
// Created by david on 6/28/2023.
//
#include <unistd.h>
#include <cstring>
#include <ostream>
#include <sys/mman.h>
#include <iostream>
#include <cstdlib>
#include <valarray>
#include <ctime>
#include <sys/wait.h>

const int BIG = 100000000;
const int MAX_BLOCK_MEMORY = 128 * 1024;
const int NUM_OF_MAX_BLOCK_SIZE = 32;
const int HEAP_SIZE = MAX_BLOCK_MEMORY * NUM_OF_MAX_BLOCK_SIZE;
const int AMOUNT_OF_BLOCK_TYPES = 11;
const unsigned int MIN_SIZE = 128;

void *heap_bottom;

bool is_first_malloc = true;

int cookies;
int mmap_total_size = 0;
int num_allocated_blocks = 0;
int num_allocated_bytes = 0;
int num_meta_data_bytes = 0;

struct MallocMetadata {
    int cookie;
    size_t size;
    bool is_free;
    MallocMetadata *next;
    MallocMetadata *prev;
};
MallocMetadata *block_array[AMOUNT_OF_BLOCK_TYPES];
MallocMetadata *mapListHead =nullptr;

void validateCookie(MallocMetadata* metadata)
{
    if(metadata->cookie != cookies) exit(0xdeadbeef);
}


void printBlockList()
{
    MallocMetadata* curr;
    int counter;
    std::cout << "=============================================================" << std::endl;
    for(int i=0;i<AMOUNT_OF_BLOCK_TYPES;i++)
    {
        curr = block_array[i];
        counter = 0;
        while(curr)
        {
            counter+=1;
            curr = curr->next;
        }
        std::cout << "Blocks of index: " << i << " Count:" << counter<< std::endl;

    }

    std::cout << "Mapped blocks:" << std::endl;
    // print how many mmap'd blocks
    curr = mapListHead;
    counter = 0;
    while(curr)
    {
        std::cout << "Block with size:" << curr->size << std::endl;
        curr=curr->next;
    }
}
void addNewMapRegion(MallocMetadata* newMemory)
{
    if(mapListHead == nullptr)
    {
        mapListHead = newMemory;
        return;
    }
    newMemory->next = mapListHead;
    mapListHead->prev = newMemory;
    mapListHead = newMemory;
}

void removeBlockFroMemoryList(MallocMetadata *currMemory) {
    validateCookie(currMemory);
    MallocMetadata *temp = currMemory->prev;
    if (temp) {
        if (!currMemory->next) {
            temp->next = nullptr;
            currMemory->next = nullptr;
            currMemory->prev = nullptr;
            return;
        }
        temp->next = currMemory->next;
        (currMemory->next)->prev = temp;
        currMemory->next = nullptr;
        currMemory->prev = nullptr;
    } else {
        if (!currMemory->next) {
            mapListHead = nullptr; //TODO: WARNING IF FIX HERE FIX THERE TOO
            currMemory->next = nullptr;
            currMemory->prev = nullptr;
            return;
        }
        currMemory->next->prev = nullptr;
        mapListHead = currMemory->next;
        currMemory->next = nullptr;
        currMemory->prev = nullptr;
    }
}
void setMetaData(MallocMetadata *metadata, size_t size, bool is_free) {
    metadata->size = size;
    metadata->is_free = is_free;
    metadata->cookie = cookies;
}



MallocMetadata *findBestFit(size_t size,unsigned int *power, int *currIndex) {
    for (int i = 0; i < AMOUNT_OF_BLOCK_TYPES; i++) {
        if (size <= *power * MIN_SIZE) {
            if (block_array[i]) {
                *currIndex = i;
                return block_array[i];
            }
        }
        *power *= 2;
    }
    return nullptr;
}

long getAddress(MallocMetadata *ptr) {
    return long(ptr);
}

void addBlockToFreeList(MallocMetadata *blockMetaData, int blockIndex) {
    validateCookie(blockMetaData);
    if (!block_array[blockIndex]) {
        block_array[blockIndex] = blockMetaData;
        return;
    }

    MallocMetadata *curr = block_array[blockIndex];
    while (curr->next && getAddress(curr) < getAddress(blockMetaData))
    {
        if(curr) validateCookie(curr);
        curr = curr->next;
    }


    // all blocks have lower address than the given one
    if (!curr->next) {
        blockMetaData->next = nullptr;
        blockMetaData->prev = curr;
        curr->next = blockMetaData;
        return;
    }
    // all blocks have higher address than the given one
    if (!curr->prev) {
        blockMetaData->next = curr;
        curr->prev = blockMetaData;
        block_array[blockIndex] = blockMetaData;
        return;
    }

    // Set the block below the next higher address and after the lowest address after mine
    MallocMetadata *temp = curr->prev;
    curr->prev = blockMetaData;
    blockMetaData->next = curr;
    blockMetaData->prev = temp;
    temp->next = blockMetaData;
}

void removeBlockFromFreeList(MallocMetadata *currBlock, int blockIndex) {
    validateCookie(currBlock);
    MallocMetadata *temp = currBlock->prev;
    if (temp) {
        if (!currBlock->next) {
            temp->next = nullptr;
            currBlock->next = nullptr;
            currBlock->prev = nullptr;
            return;
        }
        temp->next = currBlock->next;
        (currBlock->next)->prev = temp;
        currBlock->next = nullptr;
        currBlock->prev = nullptr;
    } else {
        if (!currBlock->next) {
            block_array[blockIndex] = nullptr;
            currBlock->next = nullptr;
            currBlock->prev = nullptr;
            return;
        }
        currBlock->next->prev = nullptr;
        block_array[blockIndex] = currBlock->next;
        currBlock->next = nullptr;
        currBlock->prev = nullptr;
    }
}

MallocMetadata *getNextBlock(MallocMetadata *oldPtr, size_t new_size) {
    return (MallocMetadata *) (getAddress(oldPtr) + new_size);
}

MallocMetadata *splitBlock(MallocMetadata *currBlock, int blockPower, int blockIndex) {
    validateCookie(currBlock);
    size_t old_size_of_block = currBlock->size + sizeof(MallocMetadata);
    // remove old block
    removeBlockFromFreeList(currBlock, blockIndex);

    // add 2 blocks = half size in the heap memory and add 1 to free list
    // currBlock becomes the next half size block
    setMetaData(currBlock, old_size_of_block / 2 - sizeof(MallocMetadata), true);

    MallocMetadata *secondBlock = getNextBlock(currBlock, old_size_of_block / 2);
    setMetaData(secondBlock, old_size_of_block / 2 - sizeof(MallocMetadata), true);

    addBlockToFreeList(currBlock, blockIndex - 1);
    addBlockToFreeList(secondBlock, blockIndex - 1);

    return currBlock;
}

int getBlockIndex(size_t size) {
    //remember when we call this function with a size we need to add the size of metadata before
    int power = 1, index = 0;
    while (index < AMOUNT_OF_BLOCK_TYPES && power * MIN_SIZE < size) {
        power *= 2;
        index++;
    }
    return index;
}

void *allocateSize(size_t size) {
    //we get the size + metadata size here
    unsigned int power = 1;
    int currIndex = 0;
    // we change currIndex inside the findBestFit function because we pass a pointer
    MallocMetadata *metaData = findBestFit(size, &power, &currIndex);
    if(!metaData) return nullptr;
    validateCookie(metaData);
    //this check is ok because size = meta.size + sizeof(meta)
    while (currIndex > 0 && (power * MIN_SIZE) / 2 >= size) {
        metaData = splitBlock(metaData, power, currIndex);
        num_meta_data_bytes += sizeof(MallocMetadata);
        num_allocated_blocks +=1;
        currIndex--;
        power /= 2;
    }

    removeBlockFromFreeList(metaData, currIndex);
    setMetaData(metaData, metaData->size, false);

    return (void*)((char*)metaData + sizeof(MallocMetadata));
}


void *incVoidPtr(void *old_p, int size) {
    return (void *) ((char *) old_p + size);
}

void initializeBlockArray() {
    // reset all pointers to null
    for (auto &j: block_array) j = nullptr;

    void *curr = heap_bottom;
    MallocMetadata *last;
    MallocMetadata *currMetaData;
    for (int i = 0; i < NUM_OF_MAX_BLOCK_SIZE; i++) {
        currMetaData = (MallocMetadata *) curr;
        setMetaData(currMetaData, MAX_BLOCK_MEMORY - sizeof(MallocMetadata), true);
        // increase curr
        if (!block_array[AMOUNT_OF_BLOCK_TYPES - 1]) {
            block_array[AMOUNT_OF_BLOCK_TYPES - 1] = currMetaData;
        } else {
            last->next = currMetaData;
            currMetaData->prev = last;
        }
        curr = incVoidPtr(curr, MAX_BLOCK_MEMORY);
        last = currMetaData;
    }
}

void *initializeFirstMalloc() {
    void *pb_ptr = sbrk(0);
    long addr = (long) (pb_ptr);
//    printf("pb_ptr:%ld %p\n",addr, pb_ptr);
    long inc = HEAP_SIZE - addr % HEAP_SIZE;

    void *new_ptr = sbrk(inc);
    if (new_ptr == (void *) -1) return nullptr;

    heap_bottom = sbrk(HEAP_SIZE); // now new_ptr holds the starting address (multiple of HEAP_SIZE)
    if (heap_bottom == (void *) -1) return nullptr;

    initializeBlockArray();

    return heap_bottom;
}

void *smalloc(size_t size) {
    if (is_first_malloc) {
        std::srand(std::time(nullptr));
        cookies = std::rand();
        std::cout << cookies << std::endl;
        void *new_ptr = initializeFirstMalloc();
        if (!new_ptr) return nullptr;

        num_allocated_blocks = 32;
        is_first_malloc = false;
    }
    if (!size || size > BIG)
        return nullptr;


    void *new_ptr;
    if (size + sizeof (MallocMetadata) >= MAX_BLOCK_MEMORY) {
        // TODO: handle mmap error failed
        MallocMetadata *data = (MallocMetadata*)mmap(NULL, size +sizeof(MallocMetadata),
                              PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE,
                              -1, 0);

        setMetaData(data, size, false);
        addNewMapRegion(data);

        num_allocated_blocks +=1;
        mmap_total_size += size + sizeof(MallocMetadata);
        return (void*)((char*)data + sizeof(MallocMetadata));
    }

    // allocate the size while splitting blocks (all stats handled inside)
    new_ptr = allocateSize(size + sizeof(MallocMetadata));
    return new_ptr;
}

void *scalloc(size_t num, size_t size) {
    void *new_ptr = smalloc(size * num);
    if (!new_ptr) return nullptr;

    memset(new_ptr, 0, size * num);
    return new_ptr;
}



MallocMetadata *getBuddyBlock(MallocMetadata *currMetaData) {
    validateCookie(currMetaData);
    long addr = (long) currMetaData;
    //I think this needs to be the size of the block for XOR to work so added metadata size
    size_t size = currMetaData->size + sizeof(MallocMetadata);
    long buddyAddr = size ^ addr;

    // block_array returns the actual pointer to the correct place (before metadata) so we dont need to do curr - sizeof(metadata)
    MallocMetadata *curr = block_array[getBlockIndex(size)];
    while (curr) {
        if (buddyAddr == (long) curr) break;
        validateCookie(curr);
        curr = curr->next;
    }
    return curr;
}

bool isFreeBlock(MallocMetadata *ptr) {
    MallocMetadata *curr = block_array[getBlockIndex(ptr->size)];
    while (curr) {
        validateCookie(curr);
        if ((long) ptr == (long) curr) return true;
        curr = curr->next;
    }
    return false;
}

bool canBeFreedInMapped(MallocMetadata* p)
{
    MallocMetadata* curr = mapListHead;
    while(curr)
    {
        validateCookie(curr);
        if((long)p == long(curr)) return true;
        curr = curr->next;
    }
    return false;
}

MallocMetadata* sfree_aux(void *p, bool is_realloc = false, size_t size=0)
{
    if (!p) return nullptr;

    // I think we need to do p - metadatasize for this to work
    MallocMetadata *currMetaData = (MallocMetadata *) ((char *)p - sizeof(MallocMetadata));
    validateCookie(currMetaData);
    if(currMetaData->size+sizeof(currMetaData) > MAX_BLOCK_MEMORY)
    {
        if(!canBeFreedInMapped(currMetaData)) return nullptr;
        removeBlockFroMemoryList(currMetaData);
        mmap_total_size -= currMetaData->size + sizeof(MallocMetadata);
        munmap((void*)currMetaData, currMetaData->size + sizeof(MallocMetadata));

        num_allocated_blocks -=1;

        return nullptr;
    }
    if (isFreeBlock(currMetaData)) return nullptr;
    if(!isFreeBlock(currMetaData) && currMetaData->is_free) return nullptr;

    // check if buddy exists using the address (getAddress) is inside the free list (important)
    // if it does exist remove the buddy from free list and call UniteBlocks()
    // run this logic while there is a buddy and the block index is under 10
    MallocMetadata *buddyBlock = getBuddyBlock(currMetaData);
    while (buddyBlock && getBlockIndex(buddyBlock->size + sizeof(MallocMetadata)) < AMOUNT_OF_BLOCK_TYPES - 1) {
        removeBlockFromFreeList(buddyBlock, getBlockIndex(buddyBlock->size + sizeof(MallocMetadata)));

        if ((long) currMetaData > (long) buddyBlock) {
            setMetaData(currMetaData, currMetaData->size, true);
            currMetaData = buddyBlock;
        }
        else
        {
            setMetaData(buddyBlock, buddyBlock->size, true);
        }
        //changed the new size to be correct
        long newSize = (currMetaData->size+sizeof(MallocMetadata))*2-sizeof(MallocMetadata);
        setMetaData(currMetaData, newSize, true);

        num_allocated_blocks -=1;
        if(is_realloc && currMetaData->size >= size)
        {
            setMetaData(currMetaData, newSize, false);
            return currMetaData;
        }
        buddyBlock = getBuddyBlock(currMetaData);

    }

    // after the while loop add the new block to the free list
    addBlockToFreeList(currMetaData, getBlockIndex(currMetaData->size));
    return nullptr;
}

void sfree(void *p) {
    sfree_aux(p);
}

void *srealloc(void *oldp, size_t size) {
    if (!oldp) {
        void *new_ptr = smalloc(size);
        return new_ptr;
    }
    MallocMetadata* new_ptr;
    MallocMetadata *metaData = (MallocMetadata *) ((char *) oldp - sizeof(MallocMetadata));
    validateCookie(metaData);
    if (size > metaData->size) {
        if(size + sizeof(MallocMetadata) > MAX_BLOCK_MEMORY)
        {
            sfree_aux(oldp);
            return smalloc(size);
        }
        if(isFreeBlock(metaData)) return nullptr;
        if(!isFreeBlock(metaData) && metaData->is_free) return nullptr;
        new_ptr = sfree_aux(oldp, true, size);
        if(!new_ptr)
        {
            new_ptr = (MallocMetadata*)smalloc(size);
            new_ptr = (MallocMetadata*)((char*)new_ptr - sizeof(MallocMetadata));
        }
        new_ptr = (MallocMetadata*)((char*)new_ptr + sizeof(MallocMetadata));
        //check if memmove here is correct, why do we put oldp into new_ptr?
        memmove(new_ptr, oldp, metaData->size);
        return new_ptr;
    } else {
        if (!size || size > BIG) return nullptr;
        return oldp;
    }

}

size_t _num_free_blocks() {
    int counter = 0;
    MallocMetadata *current;
    for(int i=0;i<AMOUNT_OF_BLOCK_TYPES;i++)
    {
        current = block_array[i];
        while (current != nullptr) {
            counter++;
            current = current->next;
        }
    }

    return counter;
}

size_t _num_free_bytes() {
    int sum = 0;
    MallocMetadata *current;
    for(int i=0;i<AMOUNT_OF_BLOCK_TYPES;i++)
    {
        current = block_array[i];
        while (current != nullptr) {
            validateCookie(current);
            sum+=current->size;
            current = current->next;
        }
    }
    return sum;
}

size_t _num_allocated_blocks() {
    return num_allocated_blocks;
}



size_t _num_meta_data_bytes() {
    return _num_allocated_blocks() * sizeof(MallocMetadata);
}

size_t _num_allocated_bytes() {
    if(is_first_malloc) return 0;
    return HEAP_SIZE + mmap_total_size - _num_meta_data_bytes();
}

size_t _size_meta_data() {
    return sizeof(MallocMetadata);
}

//int main() {
//    int exitCode = 0xDEADBEEF & 0xFF;  // Keep only the lower 8 bits
//    pid_t pid = fork();
//    if (pid == 0) {  // Child process
//        performCorruption();
//        std::exit(0);  // Ensure child process terminates with the desired code
//    } else {  // Parent process
//        int status;
//        waitpid(pid, &status, 0);
//        std::cout << "Status:" << WIFEXITED(status) << std::endl;
////        REQUIRE(WEXITSTATUS(status) == exitCode);
//    }
//    return 0;
//}