/*
  Copyright 2019 David Robillard <http://drobilla.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "soft_float.h"

#include "ieee_float.h"
#include "int_math.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>

/// 10^k for k = min_dec_expt, min_dec_expt + dec_expt_step, ..., max_dec_expt
static const SerdSoftFloat soft_pow10[] = {
        {0xFA8FD5A0081C0288, -1220}, {0xBAAEE17FA23EBF76, -1193},
        {0x8B16FB203055AC76, -1166}, {0xCF42894A5DCE35EA, -1140},
        {0x9A6BB0AA55653B2D, -1113}, {0xE61ACF033D1A45DF, -1087},
        {0xAB70FE17C79AC6CA, -1060}, {0xFF77B1FCBEBCDC4F, -1034},
        {0xBE5691EF416BD60C, -1007}, {0x8DD01FAD907FFC3C, -980},
        {0xD3515C2831559A83, -954},  {0x9D71AC8FADA6C9B5, -927},
        {0xEA9C227723EE8BCB, -901},  {0xAECC49914078536D, -874},
        {0x823C12795DB6CE57, -847},  {0xC21094364DFB5637, -821},
        {0x9096EA6F3848984F, -794},  {0xD77485CB25823AC7, -768},
        {0xA086CFCD97BF97F4, -741},  {0xEF340A98172AACE5, -715},
        {0xB23867FB2A35B28E, -688},  {0x84C8D4DFD2C63F3B, -661},
        {0xC5DD44271AD3CDBA, -635},  {0x936B9FCEBB25C996, -608},
        {0xDBAC6C247D62A584, -582},  {0xA3AB66580D5FDAF6, -555},
        {0xF3E2F893DEC3F126, -529},  {0xB5B5ADA8AAFF80B8, -502},
        {0x87625F056C7C4A8B, -475},  {0xC9BCFF6034C13053, -449},
        {0x964E858C91BA2655, -422},  {0xDFF9772470297EBD, -396},
        {0xA6DFBD9FB8E5B88F, -369},  {0xF8A95FCF88747D94, -343},
        {0xB94470938FA89BCF, -316},  {0x8A08F0F8BF0F156B, -289},
        {0xCDB02555653131B6, -263},  {0x993FE2C6D07B7FAC, -236},
        {0xE45C10C42A2B3B06, -210},  {0xAA242499697392D3, -183},
        {0xFD87B5F28300CA0E, -157},  {0xBCE5086492111AEB, -130},
        {0x8CBCCC096F5088CC, -103},  {0xD1B71758E219652C, -77},
        {0x9C40000000000000, -50},   {0xE8D4A51000000000, -24},
        {0xAD78EBC5AC620000, 3},     {0x813F3978F8940984, 30},
        {0xC097CE7BC90715B3, 56},    {0x8F7E32CE7BEA5C70, 83},
        {0xD5D238A4ABE98068, 109},   {0x9F4F2726179A2245, 136},
        {0xED63A231D4C4FB27, 162},   {0xB0DE65388CC8ADA8, 189},
        {0x83C7088E1AAB65DB, 216},   {0xC45D1DF942711D9A, 242},
        {0x924D692CA61BE758, 269},   {0xDA01EE641A708DEA, 295},
        {0xA26DA3999AEF774A, 322},   {0xF209787BB47D6B85, 348},
        {0xB454E4A179DD1877, 375},   {0x865B86925B9BC5C2, 402},
        {0xC83553C5C8965D3D, 428},   {0x952AB45CFA97A0B3, 455},
        {0xDE469FBD99A05FE3, 481},   {0xA59BC234DB398C25, 508},
        {0xF6C69A72A3989F5C, 534},   {0xB7DCBF5354E9BECE, 561},
        {0x88FCF317F22241E2, 588},   {0xCC20CE9BD35C78A5, 614},
        {0x98165AF37B2153DF, 641},   {0xE2A0B5DC971F303A, 667},
        {0xA8D9D1535CE3B396, 694},   {0xFB9B7CD9A4A7443C, 720},
        {0xBB764C4CA7A44410, 747},   {0x8BAB8EEFB6409C1A, 774},
        {0xD01FEF10A657842C, 800},   {0x9B10A4E5E9913129, 827},
        {0xE7109BFBA19C0C9D, 853},   {0xAC2820D9623BF429, 880},
        {0x80444B5E7AA7CF85, 907},   {0xBF21E44003ACDD2D, 933},
        {0x8E679C2F5E44FF8F, 960},   {0xD433179D9C8CB841, 986},
        {0x9E19DB92B4E31BA9, 1013},  {0xEB96BF6EBADF77D9, 1039},
        {0xAF87023B9BF0EE6B, 1066}};

SerdSoftFloat
soft_float_from_double(const double d)
{
	assert(d >= 0.0);

	const uint64_t rep  = double_to_rep(d);
	const uint64_t frac = rep & dbl_mant_mask;
	const int      expt = (int)((rep & dbl_expt_mask) >> dbl_physical_mant_dig);

	if (expt == 0) { // Subnormal
		SerdSoftFloat v = {frac, dbl_subnormal_expt};
		return v;
	}

	const SerdSoftFloat v = {frac + dbl_hidden_bit, expt - dbl_expt_bias};
	return v;
}

double
soft_float_to_double(const SerdSoftFloat v)
{
	return ldexp((double)v.f, v.e);
}

SerdSoftFloat
soft_float_normalize(SerdSoftFloat value)
{
	const unsigned amount = serd_clz64(value.f);

	value.f <<= amount;
	value.e -= (int)amount;

	return value;
}

SerdSoftFloat
soft_float_multiply(const SerdSoftFloat lhs, const SerdSoftFloat rhs)
{
	static const uint64_t mask  = 0xFFFFFFFF;
	static const uint64_t round = 1u << 31;

	const uint64_t l0   = lhs.f >> 32;
	const uint64_t l1   = lhs.f & mask;
	const uint64_t r0   = rhs.f >> 32;
	const uint64_t r1   = rhs.f & mask;
	const uint64_t l0r0 = l0 * r0;
	const uint64_t l1r0 = l1 * r0;
	const uint64_t l0r1 = l0 * r1;
	const uint64_t l1r1 = l1 * r1;
	const uint64_t mid  = (l1r1 >> 32) + (l0r1 & mask) + (l1r0 & mask) + round;

	const SerdSoftFloat r = {l0r0 + (l0r1 >> 32) + (l1r0 >> 32) + (mid >> 32),
	                         lhs.e + rhs.e + 64};

	return r;
}

SerdSoftFloat
soft_float_exact_pow10(const int expt)
{
	static const SerdSoftFloat table[8] = {{0xA000000000000000, -60},
	                                       {0xC800000000000000, -57},
	                                       {0xFA00000000000000, -54},
	                                       {0x9C40000000000000, -50},
	                                       {0xC350000000000000, -47},
	                                       {0xF424000000000000, -44},
	                                       {0x9896800000000000, -40}};

	assert(expt > 0);
	assert(expt < dec_expt_step);
	return table[expt - 1];
}

SerdSoftFloat
soft_float_pow10_under(const int exponent, int* pow10_exponent)
{
	assert(exponent >= min_dec_expt);
	assert(exponent < max_dec_expt + dec_expt_step);

	const int cache_offset = -min_dec_expt;
	const int index        = (exponent + cache_offset) / dec_expt_step;

	*pow10_exponent = min_dec_expt + index * dec_expt_step;

	assert(*pow10_exponent <= exponent);
	assert(exponent < *pow10_exponent + dec_expt_step);

	return soft_pow10[index];
}
