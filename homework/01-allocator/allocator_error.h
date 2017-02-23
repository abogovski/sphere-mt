#ifndef ALLOCATOR_ERROR
#define ALLOCATOR_ERROR

#include <stdexcept>

enum class AllocErrorType {
    NoMemory,
    Internal,
    InvalidOperation
};

class AllocError : std::runtime_error {
private:
    AllocErrorType type;

public:
    AllocError(AllocErrorType _type, std::string message)
        : runtime_error(message)
        , type(_type) {
    }

    AllocErrorType getType() const { return type; }
};

#endif // ALLOCATOR_ERROR
