//
// Created by david on 6/28/2023.
//
#include <unistd.h>
#include <cstring>
#include <ostream>
const int BIG =  100000000;
const int MAX_BLOCK_MEMORY = 128000;
const int NUM_OF_MAX_BLOCK_SIZE = 32;
const int HEAP_SIZE = MAX_BLOCK_MEMORY*NUM_OF_MAX_BLOCK_SIZE;
const int AMOUNT_OF_BLOCK_TYPES = 11;
const int MIN_SIZE = 128;

void* heap_bottom;
bool is_first_malloc = true;

struct MallocMetadata {
    int cookie;
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
};

void setMetaData(MallocMetadata* metadata,size_t size, bool is_free)
{
    metadata->size = size -sizeof(MallocMetadata);
    metadata->is_free = is_free;
}




MallocMetadata* block_array[AMOUNT_OF_BLOCK_TYPES];

MallocMetadata* findBestFit(size_t size, int* power, int* currIndex)
{
    for(int i=0;i<AMOUNT_OF_BLOCK_TYPES;i++)
    {
        if(size <= *power * MIN_SIZE)
        {
            if(block_array[i])
            {
                *currIndex = i;
                return block_array[i];
            }
        }
        *power*=2;
    }
}

long getAddress(MallocMetadata* ptr)
{
    return long(ptr);
}

void addBlockToFreeList(MallocMetadata* blockMetaData, int blockIndex)
{
    MallocMetadata* curr = block_array[blockIndex];
    if(!curr) {
        curr = blockMetaData;
        return;
    }
    while(curr->next && getAddress(curr) < getAddress(blockMetaData))
        curr = curr->next;

    // all blocks have lower address than the given one
    if(!curr->next)
    {
        blockMetaData->next = nullptr;
        blockMetaData->prev = curr;
        curr->next = blockMetaData;
        return;
    }
    // all blocks have higher address than the given one
    if(!curr->prev)
    {
        blockMetaData->next = curr;
        curr->prev = blockMetaData;
        block_array[blockIndex] = blockMetaData;
        return;
    }

    // Set the block below the next higher address and after the lowest address after mine
    MallocMetadata* temp = curr->prev;
    curr->prev = blockMetaData;
    blockMetaData->next = curr;
    blockMetaData->prev = temp;
    temp->next = blockMetaData;
}

MallocMetadata* addBlockToMemory(void* ptr,size_t size, bool is_free)
{
    MallocMetadata* new_ptr = (MallocMetadata*)(ptr);
    setMetaData(new_ptr, size, is_free);
}

void removeBlockFromFreeList(MallocMetadata* currBlock, int blockIndex)
{
    setMetaData(currBlock, currBlock->size, false);
    MallocMetadata* temp = currBlock->prev;
    if(temp){
        if(!currBlock->next)
        {
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
        if(!currBlock->next){
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

MallocMetadata* getNextBlock(MallocMetadata* oldPtr,size_t new_size)
{
    return (MallocMetadata*)(getAddress(oldPtr) + new_size);
}
MallocMetadata* splitBlock(MallocMetadata* currBlock, int blockPower, int blockIndex)
{
    size_t old_size = currBlock->size;
    // remove old block
    removeBlockFromFreeList(currBlock, blockIndex);
    // add 2 blocks = half size in the heap memory and add 1 to free list
    // currBlock becomes the next half size block
    setMetaData(currBlock, old_size / 2, true);
    addBlockToFreeList(currBlock, blockIndex - 1);
    MallocMetadata* secondBlock = getNextBlock(currBlock, old_size / 2);
    setMetaData(secondBlock, old_size/2, false);
}
void* allocateSize(size_t size)
{
    int power = 1, currIndex = 0;
    MallocMetadata* metaData = findBestFit(size,&power, &currIndex);
    // TODO: check what if (!metaData)
    while(currIndex > 0 && (power * MIN_SIZE) / 2 >= size)
    {
        splitBlock( metaData, power, currIndex);
        currIndex--;
        power/=2;
    }
}


void* incVoidPtr(void* old_p,int size)
{
    return (void*)((char*)old_p + size);
}

void initializeBlockArray()
{
    // reset all pointers to null
    for(auto & j : block_array) j = nullptr;

    void* curr = heap_bottom;
    MallocMetadata* last;
    MallocMetadata* currMetaData;
    for(int i=0;i<NUM_OF_MAX_BLOCK_SIZE;i++)
    {
        currMetaData = (MallocMetadata*)curr;
        setMetaData(currMetaData, MAX_BLOCK_MEMORY-sizeof(MallocMetadata), true);

        // increase curr
        curr = incVoidPtr(curr, MAX_BLOCK_MEMORY);
        if(!block_array[AMOUNT_OF_BLOCK_TYPES - 1])
        {
            block_array[AMOUNT_OF_BLOCK_TYPES-1] = currMetaData;
        }
        else
        {
            last->next = currMetaData;
            currMetaData->prev = last;
        }
        last = currMetaData;
    }
}

void* initializeFirstMalloc()
{
    void* pb_ptr = sbrk(0);
    long addr = (long)(pb_ptr);
//    printf("pb_ptr:%ld %p\n",addr, pb_ptr);
    long inc = HEAP_SIZE - addr % HEAP_SIZE;

    void* new_ptr = sbrk(inc);
    if(new_ptr == (void*)-1) return nullptr;

    heap_bottom = sbrk(HEAP_SIZE); // now new_ptr holds the starting address (multiple of HEAP_SIZE)
    if(heap_bottom == (void*)-1) return nullptr;

    initializeBlockArray();
    return heap_bottom;
}
void* smalloc(size_t size)
{
    if(!size || size > BIG)
        return nullptr;

    if(is_first_malloc)
    {
        void* new_ptr = initializeFirstMalloc();
        if(!new_ptr) return nullptr;
        is_first_malloc = false;
    }

    void* new_ptr;
    if(size >= MAX_BLOCK_MEMORY)
    {
        // TODO: handle memory region here
        return nullptr;
    }

    new_ptr = allocateSize(size + sizeof(MallocMetadata));
    return new_ptr;
}

void* scalloc(size_t num, size_t size)
{
    void* new_ptr = smalloc(size*num);
    if(!new_ptr) return nullptr;

    memset(new_ptr, 0, size*num);
    return new_ptr;
}


void sfree(void* p)
{
    if(!p) return;

    // check if pointer is free (convert pointer to Malloc type and check boolean)

    // check if buddy exists using the address (getAddress) is inside the free list (important)
    // if it does exist remove him from free list and call UniteBlocks()
    // run the above logic while there is a buddy and the block index is under 10
    // (update the is_free boolean)


    // after the while loop add the new block to the free list
    // and update its meta data
    //    metaData->is_free = true;
}
void* srealloc(void* oldp, size_t size)
{
    if(!oldp)
    {
        void* new_ptr = smalloc(size);
        return new_ptr;
    }
    MallocMetadata* metaData = (MallocMetadata*)((char*)oldp - sizeof(MallocMetadata));
    if(size > metaData->size)
    {
        void* new_ptr = smalloc(size);
        if(!new_ptr) return nullptr;
        memmove(new_ptr, oldp, metaData->size);
        sfree(oldp);
        return new_ptr;
    }
    else
    {
        if(!size || size > BIG) return nullptr;
        return oldp;
    }

}
size_t _num_free_blocks()
{
    MallocMetadata* current = list_head;
    int counter = 0;
    while (current != nullptr) {
        counter+=current->is_free;
        current = current->next;
    }
    return counter;
}

size_t _num_free_bytes()
{
    MallocMetadata* current = list_head;
    int sum = 0;
    while (current != nullptr) {
        sum+=current->is_free ? current->size: 0;
        current = current->next;
    }
    return sum;
}
size_t _num_allocated_blocks()
{
    MallocMetadata* current = list_head;
    int counter = 0;
    while (current != nullptr) {
        counter+=!(current->is_free);
        current = current->next;
    }
    return counter;
}
size_t _num_allocated_bytes()
{
    MallocMetadata* current = list_head;
    int sum = 0;
    while (current != nullptr) {
        sum+=current->is_free ? 0: current->size;
        current = current->next;
    }
    return sum;
}
size_t _num_meta_data_bytes()
{
    MallocMetadata* current = list_head;
    int counter = 0;
    while (current != nullptr) {
        counter++;
        current = current->next;
    }
    return counter * sizeof(MallocMetadata);
}
size_t _size_meta_data()
{
    return sizeof(MallocMetadata);
}

int main()
{
    smalloc(50);
    return 0;
}