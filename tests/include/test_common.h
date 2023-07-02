#ifndef tricky_test_common_h
#define tricky_test_common_h

#include <tricky/tricky.h>

enum class eBigError : std::uint64_t
{
    kOne,
    kTwo,
    kThree
};

enum class eReaderError : std::uint8_t
{
    kError1,
    kError2,
};

enum class eWriterError : std::uint8_t
{
    kError3,
    kError4,
    kError5,
};

enum class eBufferError : std::uint8_t
{
    kInvalidIndex,
    kInvalidPointer,
};

enum class eFileError : std::uint8_t
{
    kOpenError,
    kEOF,
    kAccessDenied,
    kPermission,
    kBusyDescriptor,
    kFileNotFound,
    kSystemError
};

enum class eNetworkError : std::uint8_t
{
    kUnreachableHost,
    kLostConnection
};

template <typename T>
using result =
    tricky::result<T, eReaderError, eWriterError, eBufferError, eFileError>;

namespace buffer
{
template <typename T>
using result = tricky::result<T, eBufferError>;
}

namespace writer
{
template <typename T>
using result = tricky::result<T, eWriterError>;
}

namespace reader
{
template <typename T>
using result = tricky::result<T, eReaderError>;
}

namespace network
{
template <typename T>
using result = tricky::result<T, eNetworkError>;
}

namespace subset
{
template <typename T>
using result = tricky::result<T, eFileError, eBufferError, eWriterError>;
}

#endif /* tricky_test_common_h */
