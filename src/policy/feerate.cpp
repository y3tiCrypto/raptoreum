// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <policy/feerate.h>

#include <tinyformat.h>

const std::string CURRENCY_UNIT = "RTM";

CFeeRate::CFeeRate(const CAmount &nFeePaid, size_t nBytes_) {
    assert(nBytes_ <= uint64_t(std::numeric_limits<int64_t>::max()));
    int64_t nSize = int64_t(nBytes_);

    if (nSize > 0) {
        __int128 nFeePaid128 = nFeePaid;
        __int128 nSatoshisPerK128 = nFeePaid128 * 1000 / nSize;
        if (nSatoshisPerK128 > std::numeric_limits<int64_t>::max()) {
            nSatoshisPerK = std::numeric_limits<int64_t>::max();
        } else if (nSatoshisPerK128 < std::numeric_limits<int64_t>::min()) {
            nSatoshisPerK = std::numeric_limits<int64_t>::min();
        } else {
            nSatoshisPerK = int64_t(nSatoshisPerK128);
        }
    } else {
        nSatoshisPerK = 0;
    }
}

CAmount CFeeRate::GetFee(size_t nBytes_) const {
    assert(nBytes_ <= uint64_t(std::numeric_limits<int64_t>::max()));
    int64_t nSize = int64_t(nBytes_);

    __int128 nSatoshisPerK128 = nSatoshisPerK;
    __int128 nFee128 = nSatoshisPerK128 * nSize / 1000;

    CAmount nFee = 0;
    if (nFee128 > std::numeric_limits<int64_t>::max()) {
        nFee = std::numeric_limits<int64_t>::max();
    } else if (nFee128 < std::numeric_limits<int64_t>::min()) {
        nFee = std::numeric_limits<int64_t>::min();
    } else {
        nFee = int64_t(nFee128);
    }

    if (nFee == 0 && nSize != 0) {
        if (nSatoshisPerK > 0)
            nFee = CAmount(1);
        if (nSatoshisPerK < 0)
            nFee = CAmount(-1);
    }

    return nFee;
}

std::string CFeeRate::ToString() const {
    return strprintf("%d.%08d %s/kB", nSatoshisPerK / COIN, nSatoshisPerK % COIN, CURRENCY_UNIT);
}
