// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "circular.h"

using namespace std;

namespace xengine
{

///////////////////////////////
// circularbuf

circularbuf::circularbuf(size_t maximum_size)
{
    buf_size_ = MIN_SIZE;
    while (buf_size_ < maximum_size)
    {
        buf_size_ <<= 1;
    }

    size_mask_ = buf_size_ - 1;
    buffer_.resize(buf_size_);

    setg(&buffer_[0], &buffer_[0], &buffer_[0]);
    setp(&buffer_[0], &buffer_[0] + buf_size_ - 1);

    begin_ = 0;
}

size_t circularbuf::size() const
{
    return (pptr() - &buffer_[0] - begin_) & size_mask_;
}

size_t circularbuf::freespace() const
{
    return (buf_size_ - size() - 1);
}

circularbuf::pos_type circularbuf::putpos() const
{
    return pos_type((pptr() - &buffer_[0]) & size_mask_);
}

void circularbuf::consume(size_t n)
{
    size_t ppos = (pptr() - &buffer_[0]) & size_mask_;
    size_t gpos = (gptr() - &buffer_[0]) & size_mask_;
    if (n >= size())
    {
        begin_ = ppos;
    }
    else
    {
        begin_ = (begin_ + n) & size_mask_;
    }

    if ((ppos >= begin_ && (gpos < begin_ || gpos > ppos))
        || (ppos < begin_ && (gpos < begin_ && gpos > ppos)))
    {
        if (begin_ <= ppos)
        {
            setg(&buffer_[0] + begin_, &buffer_[0] + begin_, &buffer_[0] + ppos);
        }
        else //if (begin_ > ppos)
        {
            setg(&buffer_[0] + begin_, &buffer_[0] + begin_, &buffer_[0] + buf_size_);
        }
    }
}

circularbuf::int_type circularbuf::underflow()
{
    size_t ppos = (pptr() - &buffer_[0]) & size_mask_;
    size_t gpos = (gptr() - &buffer_[0]) & size_mask_;

    if (gpos < ppos)
    {
        setg(&buffer_[0] + gpos, &buffer_[0] + gpos, &buffer_[0] + ppos);
        return traits_type::to_int_type(*gptr());
    }
    else if (gpos > ppos)
    {
        setg(&buffer_[0] + gpos, &buffer_[0] + gpos, &buffer_[0] + buf_size_);
        return traits_type::to_int_type(*gptr());
    }
    else // (gpos == ppos)
    {
        return traits_type::eof();
    }
}

circularbuf::int_type circularbuf::overflow(int_type c)
{
    size_t ppos = (pptr() - &buffer_[0]) & size_mask_;
    size_t pend = (begin_ - 1) & size_mask_;
    if (ppos == pend)
    {
        return traits_type::eof();
    }

    if (!traits_type::eq_int_type(c, traits_type::eof()))
    {
        if (ppos > pend)
        {
            setp(&buffer_[0] + ppos, &buffer_[0] + buf_size_);
        }
        else // (ppos < pend)
        {
            setp(&buffer_[0] + ppos, &buffer_[0] + pend);
        }

        *pptr() = traits_type::to_char_type(c);
        pbump(1);
        return c;
    }
    return traits_type::not_eof(c);
}

circularbuf::pos_type
circularbuf::seekoff(off_type off, ios_base::seekdir way, ios_base::openmode mode)
{
    if (mode & ios_base::in)
    {
        pos_type pos = 0;
        if (way == ios_base::beg)
        {
            pos = (begin_ + off) & size_mask_;
        }
        else if (way == ios_base::cur)
        {
            pos = ((gptr() - &buffer_[0]) + off) & size_mask_;
        }
        else
        {
            pos = ((pptr() - &buffer_[0]) + off) & size_mask_;
        }
        return seekpos(pos, mode);
    }
    return pos_type(off_type(-1));
}

circularbuf::pos_type
circularbuf::seekpos(pos_type pos, ios_base::openmode mode)
{
    if (mode & ios_base::in)
    {
        size_t ppos = (pptr() - &buffer_[0]) & size_mask_;
        size_t gpos = pos & size_mask_;
        if ((ppos >= begin_ && (gpos >= begin_ && gpos <= ppos))
            || (ppos < begin_ && (gpos >= begin_ || gpos <= ppos)))
        {
            if (gpos <= ppos)
            {
                setg(&buffer_[0] + gpos, &buffer_[0] + gpos, &buffer_[0] + ppos);
            }
            else //if (gpos > ppos)
            {
                setg(&buffer_[0] + gpos, &buffer_[0] + gpos, &buffer_[0] + buf_size_);
            }
            return pos_type(gpos);
        }
    }

    return pos_type(off_type(-1));
}

streamsize circularbuf::showmanyc()
{
    return streamsize((pptr() - gptr()) & size_mask_);
}

} // namespace xengine
