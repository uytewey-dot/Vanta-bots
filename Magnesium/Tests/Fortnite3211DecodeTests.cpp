#include "../../SDK/Fortnite3211Decode.h"

#include <cstdint>
#include <limits>

using namespace SDK::Fortnite3211Decode;

// Synthetic encoded fixtures derived independently from the source's documented
// inverse transforms. No captured game memory was supplied with the SDK.
// Literal fixtures catch wrong masks, rotation direction, and accidental truncation.
static_assert(DecodeObjectsArray(0x8009FFEF180E74DBULL) == 0x00007FF600100000ULL);
static_assert(DecodeObjectPtr(0x8009FFFF0F444E7BULL) == 0x00007FF600002800ULL);
static_assert(DecodeFFieldClass(0x8009FFFFD4CA4C5BULL) == 0x00007FF600003000ULL);
static_assert(DecodeFieldPtr(0x8009FFFF13F1809BULL) == 0x00007FF600004000ULL);
static_assert(DecodeClassCastFlags(0xFFFFFFFF9ADA2BBAULL) == 0x0001000000000000ULL);
static_assert(DecodePropertyFlags(0xFFFFFFFF45D1A9DBULL) == 0x180ULL);
static_assert(DecodeNumElements(0x1163CD7BU) == 100000U);
static_assert(DecodeObjectIndex(0x96C04D11U) == 42U);
static_assert(DecodeElementSize(0x82A69F83U) == 24U);
static_assert(DecodePropertyOffset(0xE026132FU) == 0x314U);
static_assert(DecodeStructSize(0xA42DD94BU) == 0x890U);

// Nulls are encoded. A caller must not test the stored pointer for null first.
static_assert(DecodeObjectsArray(0xFFFFFFFF180E74DBULL) == 0);
static_assert(DecodeObjectPtr(0xFFFFFFFF27444E7BULL) == 0);
static_assert(DecodeFFieldClass(0xFFFFFFFFE4CA4C5BULL) == 0);
static_assert(DecodeFieldPtr(0xFFFFFFFF53F1809BULL) == 0);
static_assert(DecodeClassCastFlags(0xFFFFFFFF9ADA2BBBULL) == 0);
static_assert(DecodePropertyFlags(0xFFFFFFFF4451A9DBULL) == 0);
static_assert(DecodeNumElements(0x11624BDBU) == 0);
static_assert(DecodeObjectIndex(0x96C04D3BU) == 0);
static_assert(DecodeElementSize(0x82A69F9BU) == 0);
static_assert(DecodePropertyOffset(0xE026103BU) == 0);
static_assert(DecodeStructSize(0xA42DD1DBU) == 0);

// Decoders preserve the complete native unsigned storage width, including invalid
// negative-as-unsigned counters. Runtime validation is the caller's responsibility.
static_assert(DecodeNumElements(0xEE9DB424U) == std::numeric_limits<std::uint32_t>::max());
static_assert(DecodeObjectIndex(0x693FB2C4U) == std::numeric_limits<std::uint32_t>::max());
static_assert(DecodeElementSize(0x7D596064U) == std::numeric_limits<std::uint32_t>::max());
static_assert(DecodePropertyOffset(0x1FD9EFC4U) == std::numeric_limits<std::uint32_t>::max());
static_assert(DecodeStructSize(0x5BD22E24U) == std::numeric_limits<std::uint32_t>::max());
static_assert(DecodeObjectPtr(0xD8BBB184ULL) == std::numeric_limits<std::uint64_t>::max());

int main()
{
    return 0;
}
