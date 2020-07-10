// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ed25519.h"

#include "base25519.h"

namespace curve25519
{

// −121665/121666
static const uint8_t _ecd[32] = { 0xA3, 0x78, 0x59, 0x13, 0xCA, 0x4D, 0xEB, 0x75, 0xAB, 0xD8, 0x41, 0x41, 0x4D, 0x0A, 0x70, 0x00,
                                  0x98, 0xE8, 0x79, 0x77, 0x79, 0x40, 0xC7, 0x8C, 0x73, 0xFE, 0x6F, 0x2B, 0xEE, 0x6C, 0x03, 0x52 };
// 2 * (−121665/121666)
static const uint8_t _ecd2[32] = { 0x59, 0xf1, 0xb2, 0x26, 0x94, 0x9b, 0xd6, 0xeb, 0x56, 0xb1, 0x83, 0x82, 0x9a, 0x14, 0xe0, 0x00,
                                   0x30, 0xd1, 0xf3, 0xee, 0xf2, 0x80, 0x8e, 0x19, 0xe7, 0xfc, 0xdf, 0x56, 0xdc, 0xd9, 0x06, 0x24 };
static const uint8_t _base_x[32] = { 0x1A, 0xD5, 0x25, 0x8F, 0x60, 0x2D, 0x56, 0xC9, 0xB2, 0xA7, 0x25, 0x95, 0x60, 0xC7, 0x2C, 0x69,
                                     0x5C, 0xDC, 0xD6, 0xFD, 0x31, 0xE2, 0xA4, 0xC0, 0xFE, 0x53, 0x6E, 0xCD, 0xD3, 0x36, 0x69, 0x21 };
static const uint8_t _base_y[32] = { 0x58, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
                                     0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66 };

const CFP25519 CEdwards25519::ecd = CFP25519(_ecd);
const CFP25519 CEdwards25519::ecd2 = CFP25519(_ecd2);

const CEdwards25519 CEdwards25519::base = CEdwards25519(CFP25519(_base_x), CFP25519(_base_y), true);

CEdwards25519::CEdwards25519()
  : fX(uint64_t(0)), fY(uint64_t(1)), fZ(uint64_t(1)), fT(uint64_t(0))
{
}

CEdwards25519::CEdwards25519(const CFP25519& x, const CFP25519& y, bool fPrescalar)
  : fX(x), fY(y), fZ(1), fT(x * y)
{
    if (fPrescalar)
    {
        CalcPrescalar();
    }
}

CEdwards25519::CEdwards25519(const CFP25519& x, const CFP25519& y, const CFP25519& z)
  : fX(x), fY(y), fZ(z), fT(x * y)
{
}

CEdwards25519::CEdwards25519(const CFP25519& x, const CFP25519& y, const CFP25519& z, const CFP25519& t)
  : fX(x), fY(y), fZ(z), fT(t)
{
}

CEdwards25519::~CEdwards25519()
{
}

bool CEdwards25519::Unpack(const uint8_t* md32)
{
    fZ = CFP25519(1);
    fY = CFP25519(md32);
    CFP25519 y2 = CFP25519(md32).Square();
    CFP25519 x2 = (y2 - fZ) / (y2 * ecd + fZ);
    fX = x2.Sqrt();
    if (fX.IsZero())
    {
        fT = CFP25519();
        return x2.IsZero();
    }

    if (fX.Parity() != (md32[31] >> 7))
    {
        fX = -fX;
    }
    fT = fX * fY;
    return true;
}

void CEdwards25519::Pack(uint8_t* md32) const
{
    CFP25519 zi = fZ.Inverse();
    CFP25519 tx = fX * zi;
    CFP25519 ty = fY * zi;
    ty.Pack(md32);
    md32[31] ^= tx.Parity() << 7;
}

CEdwards25519& CEdwards25519::Add(const CEdwards25519& q)
{
    CFP25519 a, b, c, d;
    a = (fY - fX) * (q.fY - q.fX);
    b = (fY + fX) * (q.fY + q.fX);
    c = fT * q.fT * ecd2;
    d = fZ * q.fZ;
    d += d;

    FromP1P1(b - a, b + a, d + c, d - c);
    return *this;
}

CEdwards25519& CEdwards25519::Double()
{
    CFP25519 a, b, c, d, e, g;
    a = fX;
    a.Square();
    b = fY;
    b.Square();
    c = fZ;
    c.Square();
    c += c;
    d = -a;
    e = fX + fY;
    e.Square();
    g = d + b;

    FromP1P1(e - a - b, d - b, g, g - c);
    return *this;
}

void CEdwards25519::AddPrescalar(const CEdwards25519& q)
{
    CFP25519 a, b, c, d, e;
    CFP25519 x, y, z, t;
    // Add
    {
        a = (fY - fX) * (q.fY - q.fX);
        b = (fY + fX) * (q.fY + q.fX);
        c = fT * q.fT * ecd2;
        d = fZ * q.fZ;
        d += d;
        x = b - a;
        y = b + a;
        z = d + c;
        t = d - c;

        fX = x * t;
        fY = y * z;
        fZ = z * t;
    }
    // Double 4
    for (int i = 0; i < 4; i++)
    {
        a = fX;
        a.Square();
        b = fY;
        b.Square();
        c = fZ;
        c.Square();
        c += c;
        d = -a;
        e = fX + fY;
        e.Square();
        x = e - a - b;
        y = d - b;
        z = d + b;
        t = z - c;

        fX = x * t;
        fY = y * z;
        fZ = z * t;
    }
    fT = x * y;
}

const CEdwards25519 CEdwards25519::ScalarMult(const uint8_t* u8, std::size_t size, const bool fPreComputation) const
{
    if (preScalar.size() != 16 || preScalar[1].fX != fX || preScalar[1].fY != fY)
    {
        CalcPrescalar();
    }

    CEdwards25519 r;
    int i;
    for (i = size - 1; i > 0 && u8[i] == 0; i--)
        ;
    for (; i > 0; i--)
    {
        r.AddPrescalar(preScalar[u8[i] >> 4]);
        r.AddPrescalar(preScalar[u8[i] & 15]);
    }
    r.AddPrescalar(preScalar[u8[0] >> 4]);
    r.Add(preScalar[u8[0] & 15]);

    return r;
}

void CEdwards25519::FromP1P1(const CFP25519& x, const CFP25519& y, const CFP25519& z, const CFP25519& t)
{
    fX = x * t;
    fY = y * z;
    fZ = z * t;
    fT = x * y;
}

void CEdwards25519::CalcPrescalar() const
{
    preScalar.resize(16);
    preScalar[0] = CEdwards25519();
    preScalar[1] = CEdwards25519(fX, fY, fZ, fT);
    for (int i = 2; i < 16; i += 2)
    {
        preScalar[i] = preScalar[i >> 1];
        preScalar[i].Double();
        preScalar[i + 1] = preScalar[i];
        preScalar[i + 1].Add(*this);
    }
}

} // namespace curve25519
