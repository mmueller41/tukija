#include "cip.hpp"
#include "pd.hpp"

void *Cip::operator new(size_t, Pd &pd)
{
    size_t size = align_up(sizeof(Cip), PAGE_SIZE);
    return Buddy::alloc(static_cast<unsigned short>(size / PAGE_SIZE), pd.quota, Buddy::NOFILL);
}