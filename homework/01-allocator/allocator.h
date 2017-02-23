#ifndef ALLOCATOR
#define ALLOCATOR
#include <string>

struct AllocatorNode
{
    static constexpr size_t flg_mask = size_t(1) << (sizeof(size_t) * 8 - 1);
    size_t head;

public:
    void setUsage(bool flag);
    void setLength(size_t length);
    
    bool usage();
    size_t length();
    AllocatorNode* next(int step = 1); // WRN: use step > 1 only for debug purposes
};

// Forward declaration. Do not include real class definition
// to avoid expensive macros calculations and increase compile speed

class Pointer;

/**
 * Wraps given memory area and provides defagmentation allocator interface on
 * the top of it.
 *
 *
 */ 
class Allocator {
public:
    static constexpr size_t pageSize = sizeof(size_t);

    Allocator(void* base, size_t size);

    /**
     * TODO: semantics
     * @param N size_t
     */
    Pointer alloc(size_t N);

    /**
     * TODO: semantics
     * @param p Pointer
     * @param N size_t
     */
    void realloc(Pointer& p, size_t N);

    /**
     * TODO: semantics
     * @param p Pointer
     */
    void free(Pointer& p);

    /**
     * TODO: semantics
     */
    void defrag();

    /**
     * TODO: semantics
     */
    std::string dump() const { return ""; }
    
private:
    void* base;
    
    AllocatorNode* first_node;
    AllocatorNode* last_node;
    
    AllocatorNode** ptr_first;
    AllocatorNode** ptr_last;
    
    void squeze_ptrs();
    AllocatorNode** place_ptr();
    AllocatorNode* find_free_node(size_t N);
    AllocatorNode* force_find_free_node(size_t N);
    void alloc_node(AllocatorNode* node, size_t N);
    void realloc_node(AllocatorNode* node, size_t N);
    void free_node(AllocatorNode* node);
};

#endif // ALLOCATOR
