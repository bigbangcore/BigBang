// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_MESSAGE_MACRO_H
#define XENGINE_MESSAGE_MACRO_H

#include <boost/version.hpp>
#include <type_traits>

namespace xengine
{
// support maximum 20 parameters
#define COUNT_COUPLE_NUM(...) COUNT_COUPLE_NUM2(0, ##__VA_ARGS__, COUNT_COUPLE_N())
#define COUNT_COUPLE_NUM2(...) COUNT_COUPLE_NUM3(__VA_ARGS__)
#define COUNT_COUPLE_NUM3(                                      \
    _1_, _2_, _3_, _4_, _5_, _6_, _7_, _8_, _9_, _10_,          \
    _11_, _12_, _13_, _14_, _15_, _16_, _17_, _18_, _19_, _20,  \
    _21_, _22_, _23_, _24_, _25_, _26_, _27_, _28_, _29_, _30_, \
    _31_, _32_, _33_, _34_, _35_, _36_, _37_, _38_, _39_, _40_, \
    _41_, _42_, N, ...) N
#define COUNT_COUPLE_N()                               \
    0, 20, 20, 19, 19, 18, 18, 17, 17, 16, 16, 15, 15, \
        14, 14, 13, 13, 12, 12, 11, 11, 10, 10,        \
        9, 9, 8, 8, 7, 7, 6, 6, 5, 5,                  \
        4, 4, 3, 3, 2, 2, 1, 1, 0

#define TYPE_LIST_0()
#define TYPE_LIST_1(type, name) type
#define TYPE_LIST_2(type, name, ...) type, TYPE_LIST_1(__VA_ARGS__)
#define TYPE_LIST_3(type, name, ...) type, TYPE_LIST_2(__VA_ARGS__)
#define TYPE_LIST_4(type, name, ...) type, TYPE_LIST_3(__VA_ARGS__)
#define TYPE_LIST_5(type, name, ...) type, TYPE_LIST_4(__VA_ARGS__)
#define TYPE_LIST_6(type, name, ...) type, TYPE_LIST_5(__VA_ARGS__)
#define TYPE_LIST_7(type, name, ...) type, TYPE_LIST_6(__VA_ARGS__)
#define TYPE_LIST_8(type, name, ...) type, TYPE_LIST_7(__VA_ARGS__)
#define TYPE_LIST_9(type, name, ...) type, TYPE_LIST_8(__VA_ARGS__)
#define TYPE_LIST_10(type, name, ...) type, TYPE_LIST_9(__VA_ARGS__)
#define TYPE_LIST_11(type, name, ...) type, TYPE_LIST_10(__VA_ARGS__)
#define TYPE_LIST_12(type, name, ...) type, TYPE_LIST_11(__VA_ARGS__)
#define TYPE_LIST_13(type, name, ...) type, TYPE_LIST_12(__VA_ARGS__)
#define TYPE_LIST_14(type, name, ...) type, TYPE_LIST_13(__VA_ARGS__)
#define TYPE_LIST_15(type, name, ...) type, TYPE_LIST_14(__VA_ARGS__)
#define TYPE_LIST_16(type, name, ...) type, TYPE_LIST_15(__VA_ARGS__)
#define TYPE_LIST_17(type, name, ...) type, TYPE_LIST_16(__VA_ARGS__)
#define TYPE_LIST_18(type, name, ...) type, TYPE_LIST_17(__VA_ARGS__)
#define TYPE_LIST_19(type, name, ...) type, TYPE_LIST_18(__VA_ARGS__)
#define TYPE_LIST_20(type, name, ...) type, TYPE_LIST_19(__VA_ARGS__)

#define TYPE_LIST_TMP3(F, ...) F(__VA_ARGS__)
#define TYPE_LIST_TMP2(N, ...) TYPE_LIST_TMP3(TYPE_LIST_##N, ##__VA_ARGS__)
#define TYPE_LIST_TMP1(N, ...) TYPE_LIST_TMP2(N, ##__VA_ARGS__)
#define TYPE_LIST(...) TYPE_LIST_TMP1(COUNT_COUPLE_NUM(__VA_ARGS__), ##__VA_ARGS__)

#define PARAM_LIST_0()
#define PARAM_LIST_1(type, name) type name
#define PARAM_LIST_2(type, name, ...) type name, PARAM_LIST_1(__VA_ARGS__)
#define PARAM_LIST_3(type, name, ...) type name, PARAM_LIST_2(__VA_ARGS__)
#define PARAM_LIST_4(type, name, ...) type name, PARAM_LIST_3(__VA_ARGS__)
#define PARAM_LIST_5(type, name, ...) type name, PARAM_LIST_4(__VA_ARGS__)
#define PARAM_LIST_6(type, name, ...) type name, PARAM_LIST_5(__VA_ARGS__)
#define PARAM_LIST_7(type, name, ...) type name, PARAM_LIST_6(__VA_ARGS__)
#define PARAM_LIST_8(type, name, ...) type name, PARAM_LIST_7(__VA_ARGS__)
#define PARAM_LIST_9(type, name, ...) type name, PARAM_LIST_8(__VA_ARGS__)
#define PARAM_LIST_10(type, name, ...) type name, PARAM_LIST_9(__VA_ARGS__)
#define PARAM_LIST_11(type, name, ...) type name, PARAM_LIST_10(__VA_ARGS__)
#define PARAM_LIST_12(type, name, ...) type name, PARAM_LIST_11(__VA_ARGS__)
#define PARAM_LIST_13(type, name, ...) type name, PARAM_LIST_12(__VA_ARGS__)
#define PARAM_LIST_14(type, name, ...) type name, PARAM_LIST_13(__VA_ARGS__)
#define PARAM_LIST_15(type, name, ...) type name, PARAM_LIST_14(__VA_ARGS__)
#define PARAM_LIST_16(type, name, ...) type name, PARAM_LIST_15(__VA_ARGS__)
#define PARAM_LIST_17(type, name, ...) type name, PARAM_LIST_16(__VA_ARGS__)
#define PARAM_LIST_18(type, name, ...) type name, PARAM_LIST_17(__VA_ARGS__)
#define PARAM_LIST_19(type, name, ...) type name, PARAM_LIST_18(__VA_ARGS__)
#define PARAM_LIST_20(type, name, ...) type name, PARAM_LIST_19(__VA_ARGS__)

#define PARAM_LIST_TMP3(F, ...) F(__VA_ARGS__)
#define PARAM_LIST_TMP2(N, ...) PARAM_LIST_TMP3(PARAM_LIST_##N, ##__VA_ARGS__)
#define PARAM_LIST_TMP1(N, ...) PARAM_LIST_TMP2(N, ##__VA_ARGS__)
#define PARAM_LIST(...) PARAM_LIST_TMP1(COUNT_COUPLE_NUM(__VA_ARGS__), ##__VA_ARGS__)

#define NAME_LIST_0()
#define NAME_LIST_1(type, name) name
#define NAME_LIST_2(type, name, ...) name, NAME_LIST_1(__VA_ARGS__)
#define NAME_LIST_3(type, name, ...) name, NAME_LIST_2(__VA_ARGS__)
#define NAME_LIST_4(type, name, ...) name, NAME_LIST_3(__VA_ARGS__)
#define NAME_LIST_5(type, name, ...) name, NAME_LIST_4(__VA_ARGS__)
#define NAME_LIST_6(type, name, ...) name, NAME_LIST_5(__VA_ARGS__)
#define NAME_LIST_7(type, name, ...) name, NAME_LIST_6(__VA_ARGS__)
#define NAME_LIST_8(type, name, ...) name, NAME_LIST_7(__VA_ARGS__)
#define NAME_LIST_9(type, name, ...) name, NAME_LIST_8(__VA_ARGS__)
#define NAME_LIST_10(type, name, ...) name, NAME_LIST_9(__VA_ARGS__)
#define NAME_LIST_11(type, name, ...) name, NAME_LIST_10(__VA_ARGS__)
#define NAME_LIST_12(type, name, ...) name, NAME_LIST_11(__VA_ARGS__)
#define NAME_LIST_13(type, name, ...) name, NAME_LIST_12(__VA_ARGS__)
#define NAME_LIST_14(type, name, ...) name, NAME_LIST_13(__VA_ARGS__)
#define NAME_LIST_15(type, name, ...) name, NAME_LIST_14(__VA_ARGS__)
#define NAME_LIST_16(type, name, ...) name, NAME_LIST_15(__VA_ARGS__)
#define NAME_LIST_17(type, name, ...) name, NAME_LIST_16(__VA_ARGS__)
#define NAME_LIST_18(type, name, ...) name, NAME_LIST_17(__VA_ARGS__)
#define NAME_LIST_19(type, name, ...) name, NAME_LIST_18(__VA_ARGS__)
#define NAME_LIST_20(type, name, ...) name, NAME_LIST_19(__VA_ARGS__)

#define NAME_LIST_TMP3(F, ...) F(__VA_ARGS__)
#define NAME_LIST_TMP2(N, ...) NAME_LIST_TMP3(NAME_LIST_##N, ##__VA_ARGS__)
#define NAME_LIST_TMP1(N, ...) NAME_LIST_TMP2(N, ##__VA_ARGS__)
#define NAME_LIST(...) NAME_LIST_TMP1(COUNT_COUPLE_NUM(__VA_ARGS__), ##__VA_ARGS__)

// index_sequence & make_index_sequence. It is same in boost 1.62 to be compatible with boost 1.58
template <size_t... I>
struct IndexSequence
{
    static constexpr size_t size()
    {
        return sizeof...(I);
    }
};

template <typename Seq1, typename Seq2>
struct ConcatSequence;

template <size_t... I1, size_t... I2>
struct ConcatSequence<IndexSequence<I1...>, IndexSequence<I2...>> : public IndexSequence<I1..., (sizeof...(I1) + I2)...>
{
};

template <std::size_t I>
struct MakeIndexSequence : public ConcatSequence<typename MakeIndexSequence<I / 2>::type, typename MakeIndexSequence<I - I / 2>::type>
{
};

template <>
struct MakeIndexSequence<0> : public IndexSequence<>
{
};
template <>
struct MakeIndexSequence<1> : public IndexSequence<0>
{
};

template <typename F, typename Tuple, size_t... I>
typename F::result_type Apply_impl(F f, Tuple t, IndexSequence<I...>)
{
    return f(std::get<I>(std::forward<Tuple>(t))...);
}

/// Apply a function with params in a tuple
template <typename F, typename Tuple>
typename F::result_type Apply(F f, Tuple&& t)
{
    using Indices = MakeIndexSequence<std::tuple_size<typename std::decay<Tuple>::type>::value>;
    return Apply_impl(f, std::forward<Tuple>(t), Indices());
}

} // namespace xengine

#endif // XENGINE_MESSAGE_MACRO_H