//
// Created by david on 6/28/2023.
//
#include <unistd.h>
#include <cstring>

const int BIG =  100000000;

struct MallocMetadata {
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
};

struct Stats
{
    size_t num_free_blocks;
    size_t num_free_bytes;
    size_t num_allocated_blocks;
    size_t num_allocated_bytes;
    size_t num_meta_data_bytes;
    size_t num_meta_data;
};

void setMetaData(MallocMetadata* metadata,size_t size, bool is_free)
{
    metadata->size = size;
    metadata->is_free = is_free;
}


MallocMetadata* list_head = nullptr;
MallocMetadata* list_last = nullptr;

void* searchFreeBlock(size_t reqSize) {
    MallocMetadata* current = list_head;
    while (current != nullptr) {
        if (current->size >= reqSize && current->is_free) {
            current->is_free = false;
            return (void*)((char*)current + sizeof(MallocMetadata));
        }
        current = current->next;
    }
    return nullptr;
}

void addMetaData(MallocMetadata* new_meta_data) {
    if(!list_head)
    {
        list_head = new_meta_data;
        list_last = new_meta_data;
        return;
    }
    list_last->next = new_meta_data;
    list_last = new_meta_data;
}

void* smalloc(size_t size)
{
    if(!size || size > BIG)
        return nullptr;
    // search free block in list
    void* freeMetaDataPtr = searchFreeBlock(size);
    // if found override data
    if(freeMetaDataPtr)
    {
        return freeMetaDataPtr;
    }

    // writing meta data
    void* new_ptr = sbrk(sizeof(MallocMetadata));
    if(new_ptr == (void*)-1) return nullptr;
    MallocMetadata* new_data = (MallocMetadata*)(new_ptr);
    setMetaData(new_data, size, false);
    // add meta data to list
    addMetaData(new_data);

    // writing the data
    new_ptr = sbrk(size);
    if(new_ptr == (void*)-1) return nullptr;
    return new_ptr;
}
void* scalloc(size_t num, size_t size)
{
    void* new_ptr = smalloc(size*num);
    if(!new_ptr) return nullptr;

    memset(new_ptr, 0, size);
    return new_ptr;
}
void sfree(void* p)
{
    if(!p) return;
    MallocMetadata* metaData = (MallocMetadata*)((char*)p - sizeof(MallocMetadata));
    metaData->is_free = true;
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