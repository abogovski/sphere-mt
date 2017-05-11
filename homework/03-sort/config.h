#ifndef SRT_CONFIG_INCLUDED
#define SRT_CONFIG_INCLUDED

#define EMULATE_STD_TO_STRING 0

#include <string>
#include <sstream>

#if EMULATE_STD_TO_STRING
namespace std
{
    template <typename T>
    std::string to_string(T value)
    {
        std::ostringstream os;
        os << value;
        return os.str();
    }
}
#endif // EMULATE_STD_TO_STRING

#endif // SRT_CONFIG_INCLUDED
