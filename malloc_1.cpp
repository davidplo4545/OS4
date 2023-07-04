//
// Created by david on 6/28/2023.
//
#include <unistd.h>
const int BIG =  100000000;
void* smalloc(size_t size)
{
    if(!size || size > BIG)
        return nullptr;
    void* new_ptr = sbrk(size);
    if(new_ptr == (void*)-1) return nullptr;
    return new_ptr;
}