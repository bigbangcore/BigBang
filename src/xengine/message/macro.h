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
    _41_, _42_, _43_, _44_, _45_, _46_, _47_, _48_, _49_, _50_, \
    _51_, _52_, _53_, _54_, _55_, _56_, _57_, _58_, _59_, _60_, \
    _61_, N, ...) N
#define COUNT_COUPLE_N()                                            \
    20, 20, 20, 19, 19, 19, 18, 18, 18, 17, 17, 17, 16, 16, 16,     \
        15, 15, 15, 14, 14, 14, 13, 13, 13, 12, 12, 12, 11, 11, 11, \
        10, 10, 10, 9, 9, 9, 8, 8, 8, 7, 7, 7, 6, 6, 6,             \
        5, 5, 5, 4, 4, 4, 3, 3, 3, 2, 2, 2, 1, 1, 1, 0

#define TYPE_LIST_0()
#define TYPE_LIST_1(type, name, value) type
#define TYPE_LIST_2(type, name, value, ...) type, TYPE_LIST_1(__VA_ARGS__)
#define TYPE_LIST_3(type, name, value, ...) type, TYPE_LIST_2(__VA_ARGS__)
#define TYPE_LIST_4(type, name, value, ...) type, TYPE_LIST_3(__VA_ARGS__)
#define TYPE_LIST_5(type, name, value, ...) type, TYPE_LIST_4(__VA_ARGS__)
#define TYPE_LIST_6(type, name, value, ...) type, TYPE_LIST_5(__VA_ARGS__)
#define TYPE_LIST_7(type, name, value, ...) type, TYPE_LIST_6(__VA_ARGS__)
#define TYPE_LIST_8(type, name, value, ...) type, TYPE_LIST_7(__VA_ARGS__)
#define TYPE_LIST_9(type, name, value, ...) type, TYPE_LIST_8(__VA_ARGS__)
#define TYPE_LIST_10(type, name, value, ...) type, TYPE_LIST_9(__VA_ARGS__)
#define TYPE_LIST_11(type, name, value, ...) type, TYPE_LIST_10(__VA_ARGS__)
#define TYPE_LIST_12(type, name, value, ...) type, TYPE_LIST_11(__VA_ARGS__)
#define TYPE_LIST_13(type, name, value, ...) type, TYPE_LIST_12(__VA_ARGS__)
#define TYPE_LIST_14(type, name, value, ...) type, TYPE_LIST_13(__VA_ARGS__)
#define TYPE_LIST_15(type, name, value, ...) type, TYPE_LIST_14(__VA_ARGS__)
#define TYPE_LIST_16(type, name, value, ...) type, TYPE_LIST_15(__VA_ARGS__)
#define TYPE_LIST_17(type, name, value, ...) type, TYPE_LIST_16(__VA_ARGS__)
#define TYPE_LIST_18(type, name, value, ...) type, TYPE_LIST_17(__VA_ARGS__)
#define TYPE_LIST_19(type, name, value, ...) type, TYPE_LIST_18(__VA_ARGS__)
#define TYPE_LIST_20(type, name, value, ...) type, TYPE_LIST_19(__VA_ARGS__)

#define TYPE_LIST_TMP3(F, ...) F(__VA_ARGS__)
#define TYPE_LIST_TMP2(N, ...) TYPE_LIST_TMP3(TYPE_LIST_##N, ##__VA_ARGS__)
#define TYPE_LIST_TMP1(N, ...) TYPE_LIST_TMP2(N, ##__VA_ARGS__)
#define TYPE_LIST(...) TYPE_LIST_TMP1(COUNT_COUPLE_NUM(__VA_ARGS__), ##__VA_ARGS__)

#define PARAM_LIST_0()
#define PARAM_LIST_1(type, name, value) type value
#define PARAM_LIST_2(type, name, value, ...) type value, PARAM_LIST_1(__VA_ARGS__)
#define PARAM_LIST_3(type, name, value, ...) type value, PARAM_LIST_2(__VA_ARGS__)
#define PARAM_LIST_4(type, name, value, ...) type value, PARAM_LIST_3(__VA_ARGS__)
#define PARAM_LIST_5(type, name, value, ...) type value, PARAM_LIST_4(__VA_ARGS__)
#define PARAM_LIST_6(type, name, value, ...) type value, PARAM_LIST_5(__VA_ARGS__)
#define PARAM_LIST_7(type, name, value, ...) type value, PARAM_LIST_6(__VA_ARGS__)
#define PARAM_LIST_8(type, name, value, ...) type value, PARAM_LIST_7(__VA_ARGS__)
#define PARAM_LIST_9(type, name, value, ...) type value, PARAM_LIST_8(__VA_ARGS__)
#define PARAM_LIST_10(type, name, value, ...) type value, PARAM_LIST_9(__VA_ARGS__)
#define PARAM_LIST_11(type, name, value, ...) type value, PARAM_LIST_10(__VA_ARGS__)
#define PARAM_LIST_12(type, name, value, ...) type value, PARAM_LIST_11(__VA_ARGS__)
#define PARAM_LIST_13(type, name, value, ...) type value, PARAM_LIST_12(__VA_ARGS__)
#define PARAM_LIST_14(type, name, value, ...) type value, PARAM_LIST_13(__VA_ARGS__)
#define PARAM_LIST_15(type, name, value, ...) type value, PARAM_LIST_14(__VA_ARGS__)
#define PARAM_LIST_16(type, name, value, ...) type value, PARAM_LIST_15(__VA_ARGS__)
#define PARAM_LIST_17(type, name, value, ...) type value, PARAM_LIST_16(__VA_ARGS__)
#define PARAM_LIST_18(type, name, value, ...) type value, PARAM_LIST_17(__VA_ARGS__)
#define PARAM_LIST_19(type, name, value, ...) type value, PARAM_LIST_18(__VA_ARGS__)
#define PARAM_LIST_20(type, name, value, ...) type value, PARAM_LIST_19(__VA_ARGS__)

#define PARAM_LIST_TMP3(F, ...) F(__VA_ARGS__)
#define PARAM_LIST_TMP2(N, ...) PARAM_LIST_TMP3(PARAM_LIST_##N, ##__VA_ARGS__)
#define PARAM_LIST_TMP1(N, ...) PARAM_LIST_TMP2(N, ##__VA_ARGS__)
#define PARAM_LIST(...) PARAM_LIST_TMP1(COUNT_COUPLE_NUM(__VA_ARGS__), ##__VA_ARGS__)

#define NAME_LIST_0()
#define NAME_LIST_1(type, name, value) name
#define NAME_LIST_2(type, name, value, ...) name, NAME_LIST_1(__VA_ARGS__)
#define NAME_LIST_3(type, name, value, ...) name, NAME_LIST_2(__VA_ARGS__)
#define NAME_LIST_4(type, name, value, ...) name, NAME_LIST_3(__VA_ARGS__)
#define NAME_LIST_5(type, name, value, ...) name, NAME_LIST_4(__VA_ARGS__)
#define NAME_LIST_6(type, name, value, ...) name, NAME_LIST_5(__VA_ARGS__)
#define NAME_LIST_7(type, name, value, ...) name, NAME_LIST_6(__VA_ARGS__)
#define NAME_LIST_8(type, name, value, ...) name, NAME_LIST_7(__VA_ARGS__)
#define NAME_LIST_9(type, name, value, ...) name, NAME_LIST_8(__VA_ARGS__)
#define NAME_LIST_10(type, name, value, ...) name, NAME_LIST_9(__VA_ARGS__)
#define NAME_LIST_11(type, name, value, ...) name, NAME_LIST_10(__VA_ARGS__)
#define NAME_LIST_12(type, name, value, ...) name, NAME_LIST_11(__VA_ARGS__)
#define NAME_LIST_13(type, name, value, ...) name, NAME_LIST_12(__VA_ARGS__)
#define NAME_LIST_14(type, name, value, ...) name, NAME_LIST_13(__VA_ARGS__)
#define NAME_LIST_15(type, name, value, ...) name, NAME_LIST_14(__VA_ARGS__)
#define NAME_LIST_16(type, name, value, ...) name, NAME_LIST_15(__VA_ARGS__)
#define NAME_LIST_17(type, name, value, ...) name, NAME_LIST_16(__VA_ARGS__)
#define NAME_LIST_18(type, name, value, ...) name, NAME_LIST_17(__VA_ARGS__)
#define NAME_LIST_19(type, name, value, ...) name, NAME_LIST_18(__VA_ARGS__)
#define NAME_LIST_20(type, name, value, ...) name, NAME_LIST_19(__VA_ARGS__)

#define NAME_LIST_TMP3(F, ...) F(__VA_ARGS__)
#define NAME_LIST_TMP2(N, ...) NAME_LIST_TMP3(NAME_LIST_##N, ##__VA_ARGS__)
#define NAME_LIST_TMP1(N, ...) NAME_LIST_TMP2(N, ##__VA_ARGS__)
#define NAME_LIST(...) NAME_LIST_TMP1(COUNT_COUPLE_NUM(__VA_ARGS__), ##__VA_ARGS__)

#define PARAM(type, ...) PARAM1(0, ##__VA_ARGS__, PARAM_FILL_3(type, __VA_ARGS__), PARAM_FILL_2(type, __VA_ARGS__), PARAM_FILL_1(type))
#define PARAM1(_1_, _2_, _3_, N, ...) N
#define PARAM_FILL_1(type) type, ,
#define PARAM_FILL_2(type, name) type, name, name
#define PARAM_FILL_3(type, name, value) type, name, name = value

#define RETURN(type, ...) RET1(0, ##__VA_ARGS__, RET_FILL_2(type, __VA_ARGS__), RET_FILL_1(type))
#define RET1(_1_, _2_, N, ...) N
#define RET_FILL_1(type) type,
#define RET_FILL_2(type, default) type, default

// index_sequence & make_index_sequence. It is same in boost 1.62 to be compatible with boost 1.58
template <size_t... I>
struct IndexSequence
{
    using type = IndexSequence;
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

template <typename R, typename F, typename Tuple, size_t... I>
R Apply_impl(F f, Tuple t, IndexSequence<I...>)
{
    return f(std::get<I>(std::forward<Tuple>(t))...);
}

#define MESSAGE_COMMON_FUNCTION(cls)                               \
    template <typename... Args>                                    \
    static std::shared_ptr<cls> Create(Args&&... args)             \
    {                                                              \
        return std::make_shared<cls>(std::forward<Args>(args)...); \
    }                                                              \
    static uint32 MessageType()                                    \
    {                                                              \
        static const uint32 nType = CMessage::NewMessageType();    \
        return nType;                                              \
    }                                                              \
    static std::string MessageTag()                                \
    {                                                              \
        static const std::string strTag = #cls;                    \
        return strTag;                                             \
    }                                                              \
    virtual uint32 Type() const override                           \
    {                                                              \
        return cls::MessageType();                                 \
    }                                                              \
    virtual std::string Tag() const override                       \
    {                                                              \
        return cls::MessageTag();                                  \
    }

#define CALLED_MESSAGE(cls, ret_type, ret_default, ...)                                    \
    class cls : public CCalledMessage<ret_type>                                            \
    {                                                                                      \
    public:                                                                                \
        using PtrHandlerType = boost::function<void(const std::shared_ptr<cls>&)>;         \
        using FunHandlerType = typename boost::function<ret_type(TYPE_LIST(__VA_ARGS__))>; \
        using ParamType = std::tuple<TYPE_LIST(__VA_ARGS__)>;                              \
                                                                                           \
        ParamType param;                                                                   \
                                                                                           \
    public:                                                                                \
        MESSAGE_COMMON_FUNCTION(cls)                                                       \
        cls(PARAM_LIST(__VA_ARGS__))                                                       \
          : CCalledMessage<ret_type>(ret_default), param(NAME_LIST(__VA_ARGS__))           \
        {                                                                                  \
        }                                                                                  \
        cls(cls&&) = default;                                                              \
        virtual cls* Move() override                                                       \
        {                                                                                  \
            return new cls(std::move(*this));                                              \
        }                                                                                  \
        virtual void Handle(boost::any handler) override                                   \
        {                                                                                  \
            try                                                                            \
            {                                                                              \
                if (handler.type() == typeid(FunHandlerType))                              \
                {                                                                          \
                    auto f = boost::any_cast<FunHandlerType>(handler);                     \
                    CCalledMessage<ret_type>::ApplyHandler(f, param);                      \
                }                                                                          \
                else if (handler.type() == typeid(PtrHandlerType))                         \
                {                                                                          \
                    auto f = boost::any_cast<PtrHandlerType>(handler);                     \
                    f(SharedFromBase<cls>());                                              \
                }                                                                          \
                else                                                                       \
                {                                                                          \
                    throw std::runtime_error("Unknown type");                              \
                }                                                                          \
            }                                                                              \
            catch (const std::exception& e)                                                \
            {                                                                              \
                LOG_ERROR(Tag().c_str(), "Message handler type error: %s", e.what());      \
            }                                                                              \
        }                                                                                  \
    }

#define PUBLISHED_MESSAGE(cls)                                                    \
    MESSAGE_COMMON_FUNCTION(cls)                                                  \
    using PtrHandlerType = boost::function<void(const std::shared_ptr<cls>&)>;    \
    virtual void Handle(boost::any handler) override                              \
    {                                                                             \
        try                                                                       \
        {                                                                         \
            if (handler.type() == typeid(PtrHandlerType))                         \
            {                                                                     \
                auto f = boost::any_cast<PtrHandlerType>(handler);                \
                f(SharedFromBase<cls>());                                         \
            }                                                                     \
            else                                                                  \
            {                                                                     \
                throw std::runtime_error("Unknown type");                         \
            }                                                                     \
        }                                                                         \
        catch (const std::exception& e)                                           \
        {                                                                         \
            LOG_ERROR(Tag().c_str(), "Message handler type error: %s", e.what()); \
        }                                                                         \
    }

} // namespace xengine

#endif // XENGINE_MESSAGE_MACRO_H