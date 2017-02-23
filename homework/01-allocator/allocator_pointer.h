#ifndef ALLOCATOR_POINTER
#define ALLOCATOR_POINTER

// Forward declaration. Do not include real class definition
// to avoid expensive macros calculations and increase compile speed
class Allocator;
class AllocatorNode;

class Pointer {
    friend class Allocator;
    
public:
    Pointer();
    void* get() const;
    
private:
    Pointer(AllocatorNode** inner_ptr);
    AllocatorNode** inner_ptr;
};

#endif //ALLOCATOR_POINTER
