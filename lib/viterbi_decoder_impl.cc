/* -*- c++ -*- */
/*
 * Copyright 2015,2016 Free Software Foundation, Inc.
 * Copyright 2021 Federico 'Larroca' La Rocca (minor changes)
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "viterbi_decoder_impl.h"
#include <gnuradio/io_signature.h>

namespace gr {
    namespace isdbt {

        const unsigned char viterbi_decoder_impl::d_puncture_1_2[2] = { 1, 1 };
        const unsigned char viterbi_decoder_impl::d_puncture_2_3[4] = { 1, 1, 0, 1 };
        const unsigned char viterbi_decoder_impl::d_puncture_3_4[6] = { 1, 1, 0, 1, 1, 0 };
        const unsigned char viterbi_decoder_impl::d_puncture_5_6[10] = { 1, 1, 0, 1, 1,
            0, 0, 1, 1, 0 };
        const unsigned char viterbi_decoder_impl::d_puncture_7_8[14] = {
            1, 1, 0, 1, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0
        };
        /* 8-bit parity lookup table, generated by partab.c */
        const unsigned char viterbi_decoder_impl::d_Partab[] = {
            0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1,
            0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0,
            0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0,
            1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1,
            0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0,
            1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1,
            1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0,
            1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
            0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
        };

#ifdef DTV_SSE2
        __GR_ATTR_ALIGNED(16) __m128i viterbi_decoder_impl::d_metric0[4];
        __GR_ATTR_ALIGNED(16) __m128i viterbi_decoder_impl::d_metric1[4];
        __GR_ATTR_ALIGNED(16) __m128i viterbi_decoder_impl::d_path0[4];
        __GR_ATTR_ALIGNED(16) __m128i viterbi_decoder_impl::d_path1[4];
#else
        __GR_ATTR_ALIGNED(16) unsigned char viterbi_decoder_impl::d_metric0_generic[64];
        __GR_ATTR_ALIGNED(16) unsigned char viterbi_decoder_impl::d_metric1_generic[64];
        __GR_ATTR_ALIGNED(16) unsigned char viterbi_decoder_impl::d_path0_generic[64];
        __GR_ATTR_ALIGNED(16) unsigned char viterbi_decoder_impl::d_path1_generic[64];
#endif

#ifdef DTV_SSE2
        __GR_ATTR_ALIGNED(16) branchtab27 viterbi_decoder_impl::Branchtab27_sse2[2];
#else
        __GR_ATTR_ALIGNED(16) branchtab27 viterbi_decoder_impl::Branchtab27_generic[2];
#endif

        __GR_ATTR_ALIGNED(16) unsigned char viterbi_decoder_impl::mmresult[64];
        __GR_ATTR_ALIGNED(16)
            unsigned char viterbi_decoder_impl::ppresult[TRACEBACK_MAX][64];

#ifdef DTV_SSE2
        void viterbi_decoder_impl::viterbi_chunks_init_sse2(__m128i* mm0, __m128i* pp0)
        {
#else
            void viterbi_decoder_impl::viterbi_chunks_init_generic(unsigned char* mm0,
                    unsigned char* pp0)
            {
#endif
                // Initialize starting metrics to prefer 0 state
                int i, j;

#ifdef DTV_SSE2
                for (i = 0; i < 4; i++) {
                    mm0[i] = _mm_setzero_si128();
                    pp0[i] = _mm_setzero_si128();
                }

                int polys[2] = { POLYA, POLYB };
                for (i = 0; i < 32; i++) {
                    Branchtab27_sse2[0].c[i] =
                        (polys[0] < 0) ^ d_Partab[(2 * i) & abs(polys[0])] ? 1 : 0;
                    Branchtab27_sse2[1].c[i] =
                        (polys[1] < 0) ^ d_Partab[(2 * i) & abs(polys[1])] ? 1 : 0;
                }
#else
                for (i = 0; i < 64; i++) {
                    mm0[i] = 0;
                    pp0[i] = 0;
                }

                int polys[2] = { POLYA, POLYB };
                for (i = 0; i < 32; i++) {
                    Branchtab27_generic[0].c[i] =
                        (polys[0] < 0) ^ d_Partab[(2 * i) & abs(polys[0])] ? 1 : 0;
                    Branchtab27_generic[1].c[i] =
                        (polys[1] < 0) ^ d_Partab[(2 * i) & abs(polys[1])] ? 1 : 0;
                }
#endif

                for (i = 0; i < 64; i++) {
                    mmresult[i] = 0;
                    for (j = 0; j < TRACEBACK_MAX; j++) {
                        ppresult[j][i] = 0;
                    }
                }
            }

#ifdef DTV_SSE2
            void viterbi_decoder_impl::viterbi_butterfly2_sse2(
                    unsigned char* symbols, __m128i* mm0, __m128i* mm1, __m128i* pp0, __m128i* pp1)
            {
                int i;

                __m128i *metric0, *metric1;
                __m128i *path0, *path1;

                metric0 = mm0;
                path0 = pp0;
                metric1 = mm1;
                path1 = pp1;

                // Operate on 4 symbols (2 bits) at a time

                __m128i m0, m1, m2, m3, decision0, decision1, survivor0, survivor1;
                __m128i metsv, metsvm;
                __m128i shift0, shift1;
                __m128i tmp0, tmp1;
                __m128i sym0v, sym1v;

                sym0v = _mm_set1_epi8(symbols[0]);
                sym1v = _mm_set1_epi8(symbols[1]);

                for (i = 0; i < 2; i++) {
                    if (symbols[0] == 2) {
                        metsvm = _mm_xor_si128(Branchtab27_sse2[1].v[i], sym1v);
                        metsv = _mm_sub_epi8(_mm_set1_epi8(1), metsvm);
                    } else if (symbols[1] == 2) {
                        metsvm = _mm_xor_si128(Branchtab27_sse2[0].v[i], sym0v);
                        metsv = _mm_sub_epi8(_mm_set1_epi8(1), metsvm);
                    } else {
                        metsvm = _mm_add_epi8(_mm_xor_si128(Branchtab27_sse2[0].v[i], sym0v),
                                _mm_xor_si128(Branchtab27_sse2[1].v[i], sym1v));
                        metsv = _mm_sub_epi8(_mm_set1_epi8(2), metsvm);
                    }

                    m0 = _mm_add_epi8(metric0[i], metsv);
                    m1 = _mm_add_epi8(metric0[i + 2], metsvm);
                    m2 = _mm_add_epi8(metric0[i], metsvm);
                    m3 = _mm_add_epi8(metric0[i + 2], metsv);

                    decision0 = _mm_cmpgt_epi8(_mm_sub_epi8(m0, m1), _mm_setzero_si128());
                    decision1 = _mm_cmpgt_epi8(_mm_sub_epi8(m2, m3), _mm_setzero_si128());
                    survivor0 =
                        _mm_or_si128(_mm_and_si128(decision0, m0), _mm_andnot_si128(decision0, m1));
                    survivor1 =
                        _mm_or_si128(_mm_and_si128(decision1, m2), _mm_andnot_si128(decision1, m3));

                    shift0 = _mm_slli_epi16(path0[i], 1);
                    shift1 = _mm_slli_epi16(path0[2 + i], 1);
                    shift1 = _mm_add_epi8(shift1, _mm_set1_epi8(1));

                    metric1[2 * i] = _mm_unpacklo_epi8(survivor0, survivor1);
                    tmp0 = _mm_or_si128(_mm_and_si128(decision0, shift0),
                            _mm_andnot_si128(decision0, shift1));

                    metric1[2 * i + 1] = _mm_unpackhi_epi8(survivor0, survivor1);
                    tmp1 = _mm_or_si128(_mm_and_si128(decision1, shift0),
                            _mm_andnot_si128(decision1, shift1));

                    path1[2 * i] = _mm_unpacklo_epi8(tmp0, tmp1);
                    path1[2 * i + 1] = _mm_unpackhi_epi8(tmp0, tmp1);
                }

                metric0 = mm1;
                path0 = pp1;
                metric1 = mm0;
                path1 = pp0;

                sym0v = _mm_set1_epi8(symbols[2]);
                sym1v = _mm_set1_epi8(symbols[3]);

                for (i = 0; i < 2; i++) {
                    if (symbols[2] == 2) {
                        metsvm = _mm_xor_si128(Branchtab27_sse2[1].v[i], sym1v);
                        metsv = _mm_sub_epi8(_mm_set1_epi8(1), metsvm);
                    } else if (symbols[3] == 2) {
                        metsvm = _mm_xor_si128(Branchtab27_sse2[0].v[i], sym0v);
                        metsv = _mm_sub_epi8(_mm_set1_epi8(1), metsvm);
                    } else {
                        metsvm = _mm_add_epi8(_mm_xor_si128(Branchtab27_sse2[0].v[i], sym0v),
                                _mm_xor_si128(Branchtab27_sse2[1].v[i], sym1v));
                        metsv = _mm_sub_epi8(_mm_set1_epi8(2), metsvm);
                    }

                    m0 = _mm_add_epi8(metric0[i], metsv);
                    m1 = _mm_add_epi8(metric0[i + 2], metsvm);
                    m2 = _mm_add_epi8(metric0[i], metsvm);
                    m3 = _mm_add_epi8(metric0[i + 2], metsv);

                    decision0 = _mm_cmpgt_epi8(_mm_sub_epi8(m0, m1), _mm_setzero_si128());
                    decision1 = _mm_cmpgt_epi8(_mm_sub_epi8(m2, m3), _mm_setzero_si128());
                    survivor0 =
                        _mm_or_si128(_mm_and_si128(decision0, m0), _mm_andnot_si128(decision0, m1));
                    survivor1 =
                        _mm_or_si128(_mm_and_si128(decision1, m2), _mm_andnot_si128(decision1, m3));

                    shift0 = _mm_slli_epi16(path0[i], 1);
                    shift1 = _mm_slli_epi16(path0[2 + i], 1);
                    shift1 = _mm_add_epi8(shift1, _mm_set1_epi8(1));

                    metric1[2 * i] = _mm_unpacklo_epi8(survivor0, survivor1);
                    tmp0 = _mm_or_si128(_mm_and_si128(decision0, shift0),
                            _mm_andnot_si128(decision0, shift1));

                    metric1[2 * i + 1] = _mm_unpackhi_epi8(survivor0, survivor1);
                    tmp1 = _mm_or_si128(_mm_and_si128(decision1, shift0),
                            _mm_andnot_si128(decision1, shift1));

                    path1[2 * i] = _mm_unpacklo_epi8(tmp0, tmp1);
                    path1[2 * i + 1] = _mm_unpackhi_epi8(tmp0, tmp1);
                }
            }
#else
            void viterbi_decoder_impl::viterbi_butterfly2_generic(unsigned char* symbols,
                    unsigned char* mm0,
                    unsigned char* mm1,
                    unsigned char* pp0,
                    unsigned char* pp1)
            {
                int i, j, k;

                unsigned char *metric0, *metric1;
                unsigned char *path0, *path1;

                metric0 = mm0;
                path0 = pp0;
                metric1 = mm1;
                path1 = pp1;

                // Operate on 4 symbols (2 bits) at a time

                unsigned char m0[16], m1[16], m2[16], m3[16], decision0[16], decision1[16],
                survivor0[16], survivor1[16];
                unsigned char metsv[16], metsvm[16];
                unsigned char shift0[16], shift1[16];
                unsigned char tmp0[16], tmp1[16];
                unsigned char sym0v[16], sym1v[16];
                unsigned short simd_epi16;

                for (j = 0; j < 16; j++) {
                    sym0v[j] = symbols[0];
                    sym1v[j] = symbols[1];
                }

                for (i = 0; i < 2; i++) {
                    if (symbols[0] == 2) {
                        for (j = 0; j < 16; j++) {
                            metsvm[j] = Branchtab27_generic[1].c[(i * 16) + j] ^ sym1v[j];
                            metsv[j] = 1 - metsvm[j];
                        }
                    } else if (symbols[1] == 2) {
                        for (j = 0; j < 16; j++) {
                            metsvm[j] = Branchtab27_generic[0].c[(i * 16) + j] ^ sym0v[j];
                            metsv[j] = 1 - metsvm[j];
                        }
                    } else {
                        for (j = 0; j < 16; j++) {
                            metsvm[j] = (Branchtab27_generic[0].c[(i * 16) + j] ^ sym0v[j]) +
                                (Branchtab27_generic[1].c[(i * 16) + j] ^ sym1v[j]);
                            metsv[j] = 2 - metsvm[j];
                        }
                    }

                    for (j = 0; j < 16; j++) {
                        m0[j] = metric0[(i * 16) + j] + metsv[j];
                        m1[j] = metric0[((i + 2) * 16) + j] + metsvm[j];
                        m2[j] = metric0[(i * 16) + j] + metsvm[j];
                        m3[j] = metric0[((i + 2) * 16) + j] + metsv[j];
                    }

                    for (j = 0; j < 16; j++) {
                        decision0[j] = ((m0[j] - m1[j]) > 0) ? 0xff : 0x0;
                        decision1[j] = ((m2[j] - m3[j]) > 0) ? 0xff : 0x0;
                        survivor0[j] = (decision0[j] & m0[j]) | ((~decision0[j]) & m1[j]);
                        survivor1[j] = (decision1[j] & m2[j]) | ((~decision1[j]) & m3[j]);
                    }

                    for (j = 0; j < 16; j += 2) {
                        simd_epi16 = path0[(i * 16) + j];
                        simd_epi16 |= path0[(i * 16) + (j + 1)] << 8;
                        simd_epi16 <<= 1;
                        shift0[j] = simd_epi16;
                        shift0[j + 1] = simd_epi16 >> 8;

                        simd_epi16 = path0[((i + 2) * 16) + j];
                        simd_epi16 |= path0[((i + 2) * 16) + (j + 1)] << 8;
                        simd_epi16 <<= 1;
                        shift1[j] = simd_epi16;
                        shift1[j + 1] = simd_epi16 >> 8;
                    }
                    for (j = 0; j < 16; j++) {
                        shift1[j] = shift1[j] + 1;
                    }

                    for (j = 0, k = 0; j < 16; j += 2, k++) {
                        metric1[(2 * i * 16) + j] = survivor0[k];
                        metric1[(2 * i * 16) + (j + 1)] = survivor1[k];
                    }
                    for (j = 0; j < 16; j++) {
                        tmp0[j] = (decision0[j] & shift0[j]) | ((~decision0[j]) & shift1[j]);
                    }

                    for (j = 0, k = 8; j < 16; j += 2, k++) {
                        metric1[((2 * i + 1) * 16) + j] = survivor0[k];
                        metric1[((2 * i + 1) * 16) + (j + 1)] = survivor1[k];
                    }
                    for (j = 0; j < 16; j++) {
                        tmp1[j] = (decision1[j] & shift0[j]) | ((~decision1[j]) & shift1[j]);
                    }

                    for (j = 0, k = 0; j < 16; j += 2, k++) {
                        path1[(2 * i * 16) + j] = tmp0[k];
                        path1[(2 * i * 16) + (j + 1)] = tmp1[k];
                    }
                    for (j = 0, k = 8; j < 16; j += 2, k++) {
                        path1[((2 * i + 1) * 16) + j] = tmp0[k];
                        path1[((2 * i + 1) * 16) + (j + 1)] = tmp1[k];
                    }
                }

                metric0 = mm1;
                path0 = pp1;
                metric1 = mm0;
                path1 = pp0;

                for (j = 0; j < 16; j++) {
                    sym0v[j] = symbols[2];
                    sym1v[j] = symbols[3];
                }

                for (i = 0; i < 2; i++) {
                    if (symbols[2] == 2) {
                        for (j = 0; j < 16; j++) {
                            metsvm[j] = Branchtab27_generic[1].c[(i * 16) + j] ^ sym1v[j];
                            metsv[j] = 1 - metsvm[j];
                        }
                    } else if (symbols[3] == 2) {
                        for (j = 0; j < 16; j++) {
                            metsvm[j] = Branchtab27_generic[0].c[(i * 16) + j] ^ sym0v[j];
                            metsv[j] = 1 - metsvm[j];
                        }
                    } else {
                        for (j = 0; j < 16; j++) {
                            metsvm[j] = (Branchtab27_generic[0].c[(i * 16) + j] ^ sym0v[j]) +
                                (Branchtab27_generic[1].c[(i * 16) + j] ^ sym1v[j]);
                            metsv[j] = 2 - metsvm[j];
                        }
                    }

                    for (j = 0; j < 16; j++) {
                        m0[j] = metric0[(i * 16) + j] + metsv[j];
                        m1[j] = metric0[((i + 2) * 16) + j] + metsvm[j];
                        m2[j] = metric0[(i * 16) + j] + metsvm[j];
                        m3[j] = metric0[((i + 2) * 16) + j] + metsv[j];
                    }

                    for (j = 0; j < 16; j++) {
                        decision0[j] = ((m0[j] - m1[j]) > 0) ? 0xff : 0x0;
                        decision1[j] = ((m2[j] - m3[j]) > 0) ? 0xff : 0x0;
                        survivor0[j] = (decision0[j] & m0[j]) | ((~decision0[j]) & m1[j]);
                        survivor1[j] = (decision1[j] & m2[j]) | ((~decision1[j]) & m3[j]);
                    }

                    for (j = 0; j < 16; j += 2) {
                        simd_epi16 = path0[(i * 16) + j];
                        simd_epi16 |= path0[(i * 16) + (j + 1)] << 8;
                        simd_epi16 <<= 1;
                        shift0[j] = simd_epi16;
                        shift0[j + 1] = simd_epi16 >> 8;

                        simd_epi16 = path0[((i + 2) * 16) + j];
                        simd_epi16 |= path0[((i + 2) * 16) + (j + 1)] << 8;
                        simd_epi16 <<= 1;
                        shift1[j] = simd_epi16;
                        shift1[j + 1] = simd_epi16 >> 8;
                    }
                    for (j = 0; j < 16; j++) {
                        shift1[j] = shift1[j] + 1;
                    }

                    for (j = 0, k = 0; j < 16; j += 2, k++) {
                        metric1[(2 * i * 16) + j] = survivor0[k];
                        metric1[(2 * i * 16) + (j + 1)] = survivor1[k];
                    }
                    for (j = 0; j < 16; j++) {
                        tmp0[j] = (decision0[j] & shift0[j]) | ((~decision0[j]) & shift1[j]);
                    }

                    for (j = 0, k = 8; j < 16; j += 2, k++) {
                        metric1[((2 * i + 1) * 16) + j] = survivor0[k];
                        metric1[((2 * i + 1) * 16) + (j + 1)] = survivor1[k];
                    }
                    for (j = 0; j < 16; j++) {
                        tmp1[j] = (decision1[j] & shift0[j]) | ((~decision1[j]) & shift1[j]);
                    }

                    for (j = 0, k = 0; j < 16; j += 2, k++) {
                        path1[(2 * i * 16) + j] = tmp0[k];
                        path1[(2 * i * 16) + (j + 1)] = tmp1[k];
                    }
                    for (j = 0, k = 8; j < 16; j += 2, k++) {
                        path1[((2 * i + 1) * 16) + j] = tmp0[k];
                        path1[((2 * i + 1) * 16) + (j + 1)] = tmp1[k];
                    }
                }
            }
#endif

#ifdef DTV_SSE2
            int viterbi_decoder_impl::viterbi_get_output_sse2(
                    __m128i* mm0, __m128i* pp0, int ntraceback, unsigned char* outbuf)
            {
#else
                int viterbi_decoder_impl::viterbi_get_output_generic(
                        unsigned char* mm0, unsigned char* pp0, int ntraceback, unsigned char* outbuf)
                {
#endif
                    //  Find current best path
                    int i;
                    int bestmetric, minmetric;
                    int beststate = 0;
                    int pos = 0;
#ifndef DTV_SSE2
                    int j;
#endif

                    // Implement a circular buffer with the last ntraceback paths
                    store_pos = (store_pos + 1) % ntraceback;

#ifdef DTV_SSE2
                    // TODO - find another way to extract the value
                    for (i = 0; i < 4; i++) {
                        _mm_store_si128((__m128i*)&mmresult[i * 16], mm0[i]);
                        _mm_store_si128((__m128i*)&ppresult[store_pos][i * 16], pp0[i]);
                    }
#else
                    for (i = 0; i < 4; i++) {
                        for (j = 0; j < 16; j++) {
                            mmresult[(i * 16) + j] = mm0[(i * 16) + j];
                            ppresult[store_pos][(i * 16) + j] = pp0[(i * 16) + j];
                        }
                    }
#endif

                    // Find out the best final state
                    bestmetric = mmresult[beststate];
                    minmetric = mmresult[beststate];

                    for (i = 1; i < 64; i++) {
                        if (mmresult[i] > bestmetric) {
                            bestmetric = mmresult[i];
                            beststate = i;
                        }
                        if (mmresult[i] < minmetric) {
                            minmetric = mmresult[i];
                        }
                    }

                    // Trace back
                    for (i = 0, pos = store_pos; i < (ntraceback - 1); i++) {
                        // Obtain the state from the output bits
                        // by clocking in the output bits in reverse order.
                        // The state has only 6 bits
                        beststate = ppresult[pos][beststate] >> 2;
                        pos = (pos - 1 + ntraceback) % ntraceback;
                    }

                    // Store output byte
                    *outbuf = ppresult[pos][beststate];

#ifdef DTV_SSE2
                    // Zero out the path variable
                    // and prevent metric overflow
                    for (i = 0; i < 4; i++) {
                        pp0[i] = _mm_setzero_si128();
                        mm0[i] = _mm_sub_epi8(mm0[i], _mm_set1_epi8(minmetric));
                    }
#else
                    for (i = 0; i < 4; i++) {
                        for (j = 0; j < 16; j++) {
                            pp0[(i * 16) + j] = 0;
                            mm0[(i * 16) + j] = mm0[(i * 16) + j] - minmetric;
                        }
                    }
#endif

                    int correct_bits = bestmetric - d_last_bestmetric + d_last_minmetric;
                    d_last_bestmetric = bestmetric; 
                    d_last_minmetric = minmetric; 
                    // I want to return the correct number of bits. The metric in this case is the number of bits that are as the chosen path
                    // and thus what I want. However, to avoid metric overflow, the min metric is always substracted from all the paths' cost 
                    // (see the for-loop above). Thus, I will output the difference between the current and the previous best path (which, 
                    // if this substraction would not be performed, it would output the correct bits from the total incoming bits, i.e. 2*8), 
                    // plus the previous minmetric to consider this substraction. Please note that depunctured bits are not considered in the metric. 
                    // This should be considered when calculating the ber. 


                    return correct_bits;
                }

                viterbi_decoder::sptr viterbi_decoder::make(int constellation_size, int rate)
                {
                    return gnuradio::get_initial_sptr(
                            new viterbi_decoder_impl(constellation_size, rate));
                }

                /*
                 * The private constructor
                 */
                viterbi_decoder_impl::viterbi_decoder_impl(int constellation_size, int rate)
                    : block("viterbi_decoder",
                            io_signature::make(1, 1, sizeof(unsigned char)),
                            io_signature::make2(1, 2, sizeof(unsigned char), sizeof(float)))
                      //,
                    //config(constellation, hierarchy, coderate, coderate),
                    //d_bsize(bsize),
                    //d_init(0),
                    //store_pos(0)
                {
                    // d_k: the input of the encoder
                    // d_n: the output of the encoder
                    // d_puncture: depuncturing matrix
                    switch (rate){
                        case 0:
                            d_k = 1;
                            d_n = 2;
                            d_puncture = d_puncture_1_2;
                            d_ntraceback = 5;
                            break;
                        case 1:
                            d_k = 2;
                            d_n = 3;
                            d_puncture = d_puncture_2_3;
                            d_ntraceback = 9;
                            break;
                        case 2:
                            d_k = 3;
                            d_n = 4;
                            d_puncture = d_puncture_3_4;
                            d_ntraceback = 10;
                            break;
                        case 3:
                            d_k = 5;
                            d_n = 6;
                            d_puncture = d_puncture_5_6;
                            d_ntraceback = 15;
                            break;
                        case 4:
                            d_k = 7;
                            d_n = 8;
                            d_puncture = d_puncture_7_8;
                            d_ntraceback = 24;
                            break;
                    }

                    // initial state
                    d_init = 0; 
                    // constellation size
                    d_m = log2(constellation_size);
           
                    // TODO what's the best value for d_bsize??? 
                    //d_bsize = 204*8/d_k; 
                    d_bsize = 24;
                    printf("d_bsize: %i\n",d_bsize);

                    /*
                     * We input n bytes, each carrying m bits => nm bits
                     * The result after decoding is km bits, therefore km/8 bytes.
                     *
                     * out/in rate is therefore km/8n in bytes
                     */

                    assert((d_bsize * d_n) % d_m == 0);
                    set_output_multiple(d_bsize * d_k / 8);

                    /*
                     * Calculate process variables:
                     * Number of symbols (d_m bits) in all blocks
                     * It is also the number of input bytes since
                     * one byte always contains just one symbol.
                     */
                    d_nsymbols = d_bsize * d_n / d_m;
                    // Number of bits after depuncturing a block (before decoding)
                    d_nbits = 2 * d_k * d_bsize;
                    // Number of output bytes after decoding
                    d_nout = d_nbits / 2 / 8;

                    d_last_minmetric = 0;
                    d_last_bestmetric = 0;
                    d_alpha_avg = 1e-5; 
                    d_last_ber_out = 0.5;
                    d_new_ber = 0;
                    
                    set_tag_propagation_policy(TPP_DONT);

                    // Allocate the buffer for the bits
                    d_inbits = new (std::nothrow) unsigned char[d_nbits];
                    if (d_inbits == NULL) {
                        GR_LOG_FATAL(d_logger, "Viterbi Decoder, cannot allocate memory for d_inbits.");
                        throw std::bad_alloc();
                    }

                    mettab[0][0] = 1;
                    mettab[0][1] = 0;
                    mettab[1][0] = 0;
                    mettab[1][1] = 1;

#ifdef DTV_SSE2
                    viterbi_chunks_init_sse2(d_metric0, d_path0);
                    printf("[Viterbi decoder] Choosing SSE2 implementation\n");
#else
                    viterbi_chunks_init_generic(d_metric0_generic, d_path0_generic);
                    printf("[Viterbi decoder] Choosing generic (slower) implementation\n");
#endif
                }

                /*
                 * Our virtual destructor.
                 */
                viterbi_decoder_impl::~viterbi_decoder_impl() { delete[] d_inbits; }

                void viterbi_decoder_impl::forecast(int noutput_items,
                        gr_vector_int& ninput_items_required)
                {
                    int input_required = noutput_items * 8 * d_n / (d_k * d_m);

                    unsigned ninputs = ninput_items_required.size();
                    for (unsigned int i = 0; i < ninputs; i++) {
                        ninput_items_required[i] = input_required;
                    }
                }

                int viterbi_decoder_impl::general_work(int noutput_items,
                        gr_vector_int& ninput_items,
                        gr_vector_const_void_star& input_items,
                        gr_vector_void_star& output_items)
                {
                    int nstreams = input_items.size();
                    int nblocks = 8 * noutput_items / (d_bsize * d_k);
                    int out_count = 0;

                    float *ber_out = (float *)output_items[1]; 
                    bool ber_out_connected = output_items.size()>=2; 
                    int correct_bits = 0; 

                    for (int m = 0; m < nstreams; m++) {
                        const unsigned char* in = (const unsigned char*)input_items[m];
                        unsigned char* out = (unsigned char*)output_items[m];

                        /*
                         * Look for a tag that signals superframe_start and consume all input items
                         * that are in input buffer so far.
                         * This will actually reset the viterbi decoder.
                         */
                        std::vector<tag_t> tags;
                        const uint64_t nread = this->nitems_read(0); // number of items read on port 0
                        this->get_tags_in_range(tags,
                                0,
                                nread,
                                nread + (nblocks * d_nsymbols),
                                pmt::string_to_symbol("resync"));

                        if (!tags.empty()) {
                            d_init = 0;

#ifdef DTV_SSE2
                            viterbi_chunks_init_sse2(d_metric0, d_path0);
#else
                            viterbi_chunks_init_generic(d_metric0_generic, d_path0_generic);
#endif

                            // if we are not aligned with the beginning of a frame, we go 
                            // to that point by consuming all the inputs
                            if (tags[0].offset - nread) {
                                consume_each(tags[0].offset - nread);
                                return (0);
                            }
                        }

                        // This is actually the Viterbi decoder
                        for (int n = 0; n < nblocks; n++) {
                            /*
                             * Depuncture and unpack a block.
                             * We receive the symbol (d_m bits/byte) in one byte (e.g. for QAM16
                             * 00001111). Create a buffer of bytes containing just one bit/byte. Also
                             * depuncture according to the puncture vector.
                             * TODO - reduce the number of branches while depuncturing.
                             */
                            for (int count = 0, i = 0; i < d_nsymbols; i++) {
                                for (int j = (d_m - 1); j >= 0; j--) {
                                    // Depuncture
                                    while (d_puncture[count % (2 * d_k)] == 0) {
                                        d_inbits[count++] = 2;
                                    }

                                    // Insert received bits
                                    d_inbits[count++] = (in[(n * d_nsymbols) + i] >> j) & 1;

                                    // Depuncture
                                    while (d_puncture[count % (2 * d_k)] == 0) {
                                        d_inbits[count++] = 2;
                                    }
                                }
                            }

                            /*
                             * Decode a block.
                             */
                            for (int in_count = 0; in_count < d_nbits; in_count++) {
                                if ((in_count % 4) == 0) { // 0 or 3

#ifdef DTV_SSE2
                                    viterbi_butterfly2_sse2(&d_inbits[in_count & 0xfffffffc],
                                            d_metric0,
                                            d_metric1,
                                            d_path0,
                                            d_path1);
#else
                                    viterbi_butterfly2_generic(&d_inbits[in_count & 0xfffffffc],
                                            d_metric0_generic,
                                            d_metric1_generic,
                                            d_path0_generic,
                                            d_path1_generic);
#endif

                                    if ((in_count > 0) && (in_count % 16) == 8) { // 8 or 11
                                        unsigned char c;

#ifdef DTV_SSE2
                                        correct_bits = viterbi_get_output_sse2(
                                                d_metric0, d_path0, d_ntraceback, &c);
#else
                                        correct_bits = viterbi_get_output_generic(
                                                d_metric0_generic, d_path0_generic, d_ntraceback, &c);
#endif

                                        if (d_init == 0) {
                                            if (out_count >= d_ntraceback) {
                                                out[out_count - d_ntraceback] = c;
                                                if(ber_out_connected){
                                                    //d_new_ber = 1.0-correct_bits/(16.0*d_n/(2*d_k)); 
                                                    d_new_ber = 1.0-correct_bits/(8.0*d_n/d_k); 
                                                    //ber_out[out_count-d_ntraceback] = 1.0-correct_bits/(8.0*d_n/d_k); 
                                                    ber_out[out_count-d_ntraceback] = d_alpha_avg*d_new_ber + (1-d_alpha_avg)*d_last_ber_out;
                                                    d_last_ber_out = ber_out[out_count-d_ntraceback]; 
                                                    // the factorization above raises from considering how many bits 
                                                    // are actually used in the metric when depuncturing is performed. 
                                                    // Not all 16 bits (to generate 8 bits at its output, the mother code needs
                                                    // 16 bits at its input) are depunctured equally (the pattern may change from 
                                                    // code-word to code-word) so expect some negative bers, 
                                                    // which should even out as larger averages are used. 
                                                  }

                                                }
                                            } else {
                                                out[out_count] = c;
                                                if(ber_out_connected)
                                                {
                                                    d_new_ber = 1.0-correct_bits/(8.0*d_n/d_k); 
                                                    ber_out[out_count] = d_alpha_avg*d_new_ber + (1-d_alpha_avg)*d_last_ber_out;
                                                    //printf("ber_out-pre=%f\n",ber_out[out_count]); 
                                                    d_last_ber_out = ber_out[out_count]; 
                                                }

                                            }
                                            out_count++;
                                        }
                                    }
                            }
                        }
                    }

                    int to_out = noutput_items;

                    if (d_init == 0) {
                        ///*
                        // * Send superframe_start to signal this situation
                        // * downstream
                        // */
                        //const uint64_t offset = this->nitems_written(0);
                        //pmt::pmt_t key = pmt::string_to_symbol("frame_begin");
                        //pmt::pmt_t value = pmt::from_long(1);
                        //this->add_item_tag(0, offset, key, value);

                        // Take in consideration the traceback length
                        printf("VITERBI: d_init=True\n");
                        to_out = to_out - d_ntraceback;
                        d_init = 1;
                    }

                    /*
                     * Send frame_begin to signal frame_begin situation
                     * downstream
                     */
                    std::vector<tag_t> tags;
                    const uint64_t nread = this->nitems_read(0); //number of items read on port 0
                    this->get_tags_in_range(tags, 0, nread, nread + (nblocks * d_nsymbols), pmt::string_to_symbol("frame_begin"));
                    if(tags.size()){
                        const uint64_t offset = this->nitems_written(0) + d_ntraceback + (tags[0].offset- this->nitems_read(0))*d_m*d_k/(d_n*8.0);
                        //std::cout << "offset: "<< offset-this->nitems_written(0) << " nitems_written: "<<this->nitems_written(0)<<std::endl; 
                        //const uint64_t offset = tags[0].offset; 
                        pmt::pmt_t key = pmt::string_to_symbol("frame_begin");
                        pmt::pmt_t value = pmt::string_to_symbol("generated by the viterbi decoder");
                        this->add_item_tag(0, offset, key, value);
                    }



                    // Tell runtime system how many input items we consumed on
                    // each input stream.
                    consume_each(nblocks * d_nsymbols);

                    // Tell runtime system how many output items we produced.
                    return (to_out);
                }

            } /* namespace isdbt */
        } /* namespace gr */
