// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Copyright (c) 2025-2026 natusor (MONEU)
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pow.h"
#include "../crypto/arith_uint256.h"
#include <cstring>

namespace PNC {

bool CheckProofOfWork(const bytes32& hash, uint32_t bits)
{
    bool fNegative = false;
    bool fOverflow = false;
    arith_uint256 bnTarget;
    bnTarget.SetCompact(bits, &fNegative, &fOverflow);

    arith_uint256 bnPowLimit;
    bnPowLimit.SetCompact(NetParams::POW_LIMIT_COMPACT);

    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > bnPowLimit)
        return false;

    if (ArithFromBytes32(hash) > bnTarget)
        return false;

    return true;
}

uint32_t CalculateNextWorkRequired(uint32_t lastBits,
                                   uint64_t lastBlockTime,
                                   uint64_t firstBlockTime)
{
    const uint64_t idealTimespan = NetParams::POW_TARGET_TIMESPAN;

    int64_t actualTimespan =
        (int64_t)lastBlockTime - (int64_t)firstBlockTime;

    const int64_t minTimespan =
        (int64_t)(idealTimespan * NetParams::POW_MIN_TIMESPAN_PERCENT) / 100;
    const int64_t maxTimespan =
        (int64_t)(idealTimespan * NetParams::POW_MAX_TIMESPAN_PERCENT) / 100;

    if (actualTimespan < minTimespan) actualTimespan = minTimespan;
    if (actualTimespan > maxTimespan) actualTimespan = maxTimespan;

    arith_uint256 bnPowLimit;
    bnPowLimit.SetCompact(NetParams::POW_LIMIT_COMPACT);

    bool negative = false;
    bool overflow = false;
    arith_uint256 bnNew;
    bnNew.SetCompact(lastBits, &negative, &overflow);
    if (negative || overflow || bnNew == 0) {
        return NetParams::POW_LIMIT_COMPACT;
    }

    bnNew *= (uint32_t)actualTimespan;
    bnNew /= arith_uint256((uint64_t)idealTimespan);

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    return bnNew.GetCompact();
}

bool IsRetargetHeight(uint64_t height)
{
    if (height == 0) return false;
    return (height % NetParams::DIFFICULTY_ADJUSTMENT_INTERVAL) == 0;
}

// ASERT by Mark Lundeberg. aserti3-2d spec by freetrader, Jonathan Toomim,
// Calin Culianu and Mark Lundeberg. Integer code from Bitcoin ABC / Bitcoin
// Cash Node, Copyright (c) 2020 The Bitcoin developers, MIT license.
static_assert(int64_t(-1) >> 1 == int64_t(-1),
              "ASERT needs arithmetic right shift of negative numbers");

arith_uint256 CalculateASERT(const arith_uint256& refTarget,
                             int64_t spacing,
                             int64_t timeDiff,
                             int64_t heightDiff,
                             const arith_uint256& powLimit,
                             int64_t halfLife)
{
    if (refTarget == 0 || refTarget > powLimit || heightDiff < 0 ||
        halfLife <= 0 || (powLimit >> 224) != 0) {
        return powLimit;
    }

    const int64_t kMaxOffset = (int64_t(1) << 47) - 1;
    int64_t offset = timeDiff - spacing * (heightDiff + 1);
    if (offset > kMaxOffset) offset = kMaxOffset;
    if (offset < -kMaxOffset) offset = -kMaxOffset;
    const int64_t exponent = (offset * 65536) / halfLife;

    int64_t shifts = exponent >> 16;
    const uint16_t frac = static_cast<uint16_t>(exponent);

    // 2^x approximation, error under 0.013%
    const uint32_t factor =
        65536 + ((+195766423245049ull * frac + 971821376ull * frac * frac +
                  5127ull * frac * frac * frac + (1ull << 47)) >> 48);

    arith_uint256 nextTarget = refTarget * factor;

    shifts -= 16;
    if (shifts <= 0) {
        if (-shifts >= 256) {
            nextTarget = 0;
        } else {
            nextTarget >>= static_cast<unsigned int>(-shifts);
        }
    } else {
        if (shifts >= 256) {
            nextTarget = powLimit;
        } else {
            const arith_uint256 shifted =
                nextTarget << static_cast<unsigned int>(shifts);
            if ((shifted >> static_cast<unsigned int>(shifts)) != nextTarget) {
                nextTarget = powLimit;
            } else {
                nextTarget = shifted;
            }
        }
    }

    if (nextTarget == 0) {
        nextTarget = arith_uint256(1);
    } else if (nextTarget > powLimit) {
        nextTarget = powLimit;
    }
    return nextTarget;
}

bool IsAsertHeight(uint64_t height)
{
    return height > NetParams::ASERT_ANCHOR_HEIGHT;
}

uint32_t CalculateASERTBits(uint32_t anchorBits,
                            uint64_t anchorParentTime,
                            uint64_t parentTime,
                            uint64_t parentHeight)
{
    arith_uint256 powLimit;
    powLimit.SetCompact(NetParams::POW_LIMIT_COMPACT);

    bool negative = false;
    bool overflow = false;
    arith_uint256 refTarget;
    refTarget.SetCompact(anchorBits, &negative, &overflow);
    if (negative || overflow || refTarget == 0 || refTarget > powLimit) {
        return NetParams::POW_LIMIT_COMPACT;
    }

    const int64_t timeDiff =
        static_cast<int64_t>(parentTime) -
        static_cast<int64_t>(anchorParentTime);
    const int64_t heightDiff =
        static_cast<int64_t>(parentHeight) -
        static_cast<int64_t>(NetParams::ASERT_ANCHOR_HEIGHT);

    const arith_uint256 next = CalculateASERT(
        refTarget, static_cast<int64_t>(NetParams::BLOCK_TIME_TARGET),
        timeDiff, heightDiff, powLimit, NetParams::ASERT_HALF_LIFE);
    return next.GetCompact();
}

bytes32 GetBlockProof(uint32_t bits)
{
    bool fNegative = false;
    bool fOverflow = false;
    arith_uint256 bnTarget;
    bnTarget.SetCompact(bits, &fNegative, &fOverflow);
    if (fNegative || fOverflow || bnTarget == 0) {
        bytes32 zero{};
        return zero;
    }
    arith_uint256 work = (~bnTarget / (bnTarget + 1)) + 1;
    return ArithToBytes32(work);
}

bytes32 AddChainWork(const bytes32& parentWork, uint32_t bits)
{
    arith_uint256 total = ArithFromBytes32(parentWork);
    total += ArithFromBytes32(GetBlockProof(bits));
    return ArithToBytes32(total);
}

int CompareWork(const bytes32& a, const bytes32& b)
{
    return std::memcmp(a.data(), b.data(), 32);
}

bytes32 CompactToTarget(uint32_t bits)
{
    bool fNegative = false;
    bool fOverflow = false;
    arith_uint256 bnTarget;
    bnTarget.SetCompact(bits, &fNegative, &fOverflow);
    if (fNegative || fOverflow || bnTarget == 0) {
        bytes32 zero{};
        return zero;
    }
    return ArithToBytes32(bnTarget);
}

} // namespace PNC
