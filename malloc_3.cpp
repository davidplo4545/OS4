//
// Created by david on 6/28/2023.
//
#include <unistd.h>
#include <cstring>
#include <ostream>
#include <sys/mman.h>

const int BIG = 100000000;
const int MAX_BLOCK_MEMORY = 128000;
const int NUM_OF_MAX_BLOCK_SIZE = 32;
const int HEAP_SIZE = MAX_BLOCK_MEMORY * NUM_OF_MAX_BLOCK_SIZE;
const int AMOUNT_OF_BLOCK_TYPES = 11;
const int MIN_SIZE = 128;

void *heap_bottom;

bool is_first_malloc = true;
int num_allocated_blocks;
int num_allocated_bytes;
struct MallocMetadata {
    int cookie;
    size_t size;
    MallocMetadata *next;
    MallocMetadata *prev;
};
MallocMetadata *block_array[AMOUNT_OF_BLOCK_TYPES];
MallocMetadata *mapListHead =nullptr;

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
void setMetaData(MallocMetadata *metadata, size_t size) {
    metadata->size = size;
}



MallocMetadata *findBestFit(size_t size, int *power, int *currIndex) {
    for (int i = 0; i < AMOUNT_OF_BLOCK_TYPES; i++) {
        if (size <= *power * MIN_SIZE) {
            if (block_array[i]) {
                *currIndex = i;
                return block_array[i];
            }
        }
        *power *= 2;
    }
}

long getAddress(MallocMetadata *ptr) {
    return long(ptr);
}

void addBlockToFreeList(MallocMetadata *blockMetaData, int blockIndex) {
    MallocMetadata *curr = block_array[blockIndex];
    if (!curr) {
        curr = blockMetaData;
        return;
    }
    while (curr->next && getAddress(curr) < getAddress(blockMetaData))
        curr = curr->next;

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
    size_t old_size_of_block = currBlock->size + sizeof(MallocMetadata);
    // remove old block
    removeBlockFromFreeList(currBlock, blockIndex);
    // add 2 blocks = half size in the heap memory and add 1 to free list
    // currBlock becomes the next half size block
    setMetaData(currBlock, old_size_of_block / 2 - sizeof(MallocMetadata));
    addBlockToFreeList(currBlock, blockIndex - 1);
    MallocMetadata *secondBlock = getNextBlock(currBlock, old_size_of_block / 2);
    setMetaData(secondBlock, old_size_of_block / 2 - sizeof(MallocMetadata));
    return secondBlock;
}

int getBlockIndex(size_t size) {
    int power = 1, index = 0;
    while (index < AMOUNT_OF_BLOCK_TYPES && power * MIN_SIZE < size) {
        power *= 2;
        index++;
    }
    return index;
}

void *allocateSize(size_t size) {
    int power = 1, currIndex = 0;
    MallocMetadata *metaData = findBestFit(size, &power, &currIndex);
    currIndex = getBlockIndex(metaData->size);
    // TODO: check what if (!metaData)
    while (currIndex > 0 && (power * MIN_SIZE) / 2 >= size) {
        metaData = splitBlock(metaData, power, currIndex);
        currIndex--;
        power /= 2;
    }
    return metaData;
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
        setMetaData(currMetaData, MAX_BLOCK_MEMORY - sizeof(MallocMetadata));

        // increase curr
        curr = incVoidPtr(curr, MAX_BLOCK_MEMORY);
        if (!block_array[AMOUNT_OF_BLOCK_TYPES - 1]) {
            block_array[AMOUNT_OF_BLOCK_TYPES - 1] = currMetaData;
        } else {
            last->next = currMetaData;
            currMetaData->prev = last;
        }
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
    if (!size || size > BIG)
        return nullptr;

    if (is_first_malloc) {
        void *new_ptr = initializeFirstMalloc();
        if (!new_ptr) return nullptr;
        is_first_malloc = false;
    }

    void *new_ptr;
    if (size >= MAX_BLOCK_MEMORY) {
        // TODO: handle memory region here
        MallocMetadata *data = (MallocMetadata*)mmap(NULL, size +sizeof(MallocMetadata),
                              PROT_READ | PROT_WRITE, MAP_ANONYMOUS,
                              -1, 0);
        setMetaData(data, size);
        addNewMapRegion(data);
        return data;
    }

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
    long addr = (long) currMetaData;
    size_t size = currMetaData->size;
    long buddyAddr = size ^ addr;

    // find buddy in free list
    MallocMetadata *curr = block_array[getBlockIndex(size)];
    while (curr) {
        if (buddyAddr == (long) curr) break;
        curr = curr->next;
    }
    return curr;
}

bool isFreeBlock(MallocMetadata *ptr) {
    MallocMetadata *curr = block_array[getBlockIndex(ptr->size)];
    while (curr) {
        if ((long) ptr == (long) curr) return true;
        curr = curr->next;
    }
    return false;
}

MallocMetadata* sfree_aux(void *p, bool is_realloc = false, size_t size=0)
{
    if (!p) return nullptr;

    // check if pointer is free (convert pointer to Malloc type and check boolean)
    MallocMetadata *currMetaData = (MallocMetadata *) p;
    if(currMetaData->size+sizeof(currMetaData) > 128000)
    {
        removeBlockFroMemoryList(currMetaData);
        munmap((void*)currMetaData, currMetaData->size);
        return nullptr;
    }
    if (isFreeBlock(currMetaData)) return nullptr;

    // check if buddy exists using the address (getAddress) is inside the free list (important)
    // if it does exist remove the buddy from free list and call UniteBlocks()
    // run this logic while there is a buddy and the block index is under 10
    MallocMetadata *buddyBlock = getBuddyBlock(currMetaData);
    while (buddyBlock && getBlockIndex(buddyBlock->size) < AMOUNT_OF_BLOCK_TYPES - 1) {
        removeBlockFromFreeList(buddyBlock, getBlockIndex(buddyBlock->size));

        if ((long) currMetaData > (long) buddyBlock) {
            currMetaData = buddyBlock;
        }
        setMetaData(currMetaData, currMetaData->size*2);
        if(is_realloc && currMetaData->size >= size)
        {
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
    if (size > metaData->size) {
        if(size + sizeof(MallocMetadata) > 128000)
        {
            sfree_aux(oldp);
            return smalloc(size);
        }
        if(isFreeBlock(metaData)) return nullptr;

        new_ptr = sfree_aux(oldp, true, size);
        if(!new_ptr) new_ptr = (MallocMetadata*)smalloc(size);
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
            sum+=current->size;
            current = current->next;
        }
    }
    return sum;
}

size_t _num_allocated_blocks() {
    MallocMetadata *current = list_head;
    int counter = 0;
    while (current != nullptr) {
        counter += !(current->is_free);
        current = current->next;
    }
    return counter;
}

size_t _num_allocated_bytes() {
    MallocMetadata *current = list_head;
    int sum = 0;
    while (current != nullptr) {
        sum += current->is_free ? 0 : current->size;
        current = current->next;
    }
    return sum;
}

size_t _num_meta_data_bytes() {
    MallocMetadata *current = list_head;
    int counter = 0;
    while (current != nullptr) {
        counter++;
        current = current->next;
    }
    return counter * sizeof(MallocMetadata);
}

size_t _size_meta_data() {
    return sizeof(MallocMetadata);
}

int main() {
    smalloc(50);
    return 0;
}