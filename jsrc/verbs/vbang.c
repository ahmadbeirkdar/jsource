/* Copyright 1990-2008, Jsoftware Inc.  All rights reserved.               */
/* Licensed use only. Any other use is in violation of copyright.          */
/*                                                                         */
/* Verbs: !                                                                */

#include "j.h"

static const Z z1 = {1, 0};

static D coeff[] = {0.0,
                    1.0,
                    0.5772156649015329,
                    -0.6558780715202538,
                    -0.0420026350340952,
                    0.1665386113822915,
                    -0.0421977345555443,
                    -0.009621971527877,
                    0.007218943246663,
                    -0.0011651675918591,
                    -0.0002152416741149,
                    0.0001280502823882,
                    -0.0000201348547807,
                    -0.0000012504934821,
                    0.000001133027232,
                    -0.0000002056338417,
                    6.116095e-9,
                    5.0020075e-9,
                    -1.1812746e-9,
                    1.043427e-10,
                    7.7823e-12,
                    -3.6968e-12,
                    5.1e-13,
                    -2.06e-14,
                    -5.4e-15,
                    1.4e-15,
                    1.0e-16};

static I terms = sizeof(coeff) / sizeof(D);

static Z
jtzhorner(J jt, I n, D* c, Z v) {
    Z s;
    D* d = n + c;
    s    = zeroZ;
    DQ(n, s = jtzplus(jt, zrj0(*--d), jtztymes(jt, v, s)););
    return s;
}

static D
dgps(D v) {
    D *d = terms + coeff, s = 0.0;
    DQ(terms, s = *--d + v * s;);
    return 1 / s;
}
/* Abramowitz & Stegun, 6.1.34 */

static Z
jtzgps(J jt, Z z) {
    return jtzdiv(jt, z1, zhorner(terms, coeff, z));
}

D
jtdgamma(J jt, D x) {
    B b;
    D t;
    t = 1.0;
    b = x == floor(x);
    if (b && 0 >= x) {
        ASSERT(x > x - 1, EVLIMIT);
        return x == 2 * floor(x / 2) ? inf : infm;
    }
    if (0 <= x)
        while (1 < x) {
            t *= --x;
            if (t == inf) return inf;
        }
    else {
        while (0 > x) {
            t *= x++;
            if (t == inf) return 0.0;
        }
        t = 1.0 / t;
    }
    return b ? t : t * dgps(x);
} /* gamma(x) using recurrence formula */

static Z
jtzgrecur(J jt, Z z) {
    Z t;
    t = z1;
    if (0 <= z.re)
        while (0.5 < z.re) {
            --z.re;
            t = jtztymes(jt, t, z);
            if (t.re == inf) return t;
        }
    else {
        while (-0.5 > z.re) {
            t = jtztymes(jt, t, z);
            ++z.re;
            if (t.re == inf) return zeroZ;
        }
        t = jtzdiv(jt, z1, t);
    }
    return jtztymes(jt, t, jtzgps(jt, z));
} /* gamma(z) using recurrence formula */

static Z
jtzgauss(J jt, D n, Z z) {
    D d = 1 / n;
    Z p, t;
    if (1 >= n) return jtzgrecur(jt, z);
    p = jtztymes(jt, jtzpow(jt, zrj0(2 * PI), zrj0((1 - n) / 2)), jtzpow(jt, zrj0(n), jtzminus(jt, z, zrj0(0.5))));
    t = jtzdiv(jt, z, zrj0(n));
    DQ((I)n, p = jtztymes(jt, p, jtzgrecur(jt, t)); t.re += d;);
    return p;
} /* Abramowitz & Stegun, 6.1.20 */

static D c[] = {1.0, 1.0 / 12, 1.0 / 288, -139.0 / 51840, -571.0 / 2488320};
static Z
jtzstirling(J jt, Z z) {
    Z p, q;
    p = jtztymes(
      jt, jtzsqrt(jt, jtzdiv(jt, zrj0(2 * PI), z)), jtzpow(jt, jtzdiv(jt, z, zrj0(2.718281828459045235360287)), z));
    q = zhorner(5L, c, jtzdiv(jt, z1, z));
    return jtztymes(jt, p, q);
} /* Abramowitz & Stegun, 6.1.37 */

static Z
jtzgamma(J jt, Z z) {
    D y = ABS(z.im);
    return !y ? zrj0(jtdgamma(jt, z.re)) : 20 < y ? jtzstirling(jt, z) : jtzgauss(jt, ceil(y / 0.8660254), z);
}

AMONPS(factI, D, I, , *z = jtdgamma(jt, 1.0 + (D)*x);, HDR1JERR)
AMONPS(factD, D, D, , *z = _isnan(*x) ? *x : jtdgamma(jt, 1.0 + *x);, HDR1JERR)
AMONPS(factZ, Z, Z, , *z = jtzgamma(jt, jtzplus(jt, z1, *x));, HDR1JERR)

#define PQLOOP(expr)                          \
    while (n && h && h != inf && h != infm) { \
        h *= expr;                            \
        --n;                                  \
    }

static D
pq(D h, D m, D* c, D* d) {
    D x = *c, y = *d;
    I n = (I)MIN(m, FLIMAX);
    if (0 >= m) return h;
    switch (2 * (I)(0 > x) + (I)(0 > y)) {
        case 0:
            if (x != y) PQLOOP(x-- / y--);
            break;
        case 1:
            if (x != -y) PQLOOP(x-- / y++) else if (m > 2 * floor(0.5 * m)) h = -h;
            break;
        case 2:
            if (x != -y) PQLOOP(x++ / y--) else if (m > 2 * floor(0.5 * m)) h = -h;
            break;
        case 3:
            if (x != y) PQLOOP(x++ / y++);
            break;
    }
    if (0 >= *c)
        *c += m;
    else
        *c -= m;
    if (0 >= *d)
        *d += m;
    else
        *d -= m;
    return h;
}

static I
signf(D x) {
    return 0 <= x || 1 <= x - 2 * floor(0.5 * x) ? 1 : -1;
}
/* sign of !x */

static D
jtdbin(J jt, D x, D y) {
    D c, d, e, h = 1.0, p, q, r;
    I k = 0;
    c   = y;
    if (0 <= c)
        p = floor(c);
    else {
        k += 4;
        ++c;
        p = floor(-c);
    }
    d = y - x;
    if (0 <= d)
        q = floor(d);
    else {
        k += 2;
        ++d;
        q = floor(-d);
    }
    e = x;
    if (0 <= e)
        r = floor(e);
    else {
        k += 1;
        ++e;
        r = floor(-e);
    }
    switch (k) {
        case 0:
            h = pq(h, q, &c, &d);
            h = pq(h, r, &c, &e);
            break;
        case 1:
            h = pq(h, p, &c, &d);
            h = pq(h, r, &e, &d);
            --e;
            break;
        case 2:
            h = pq(h, p, &c, &e);
            h = pq(h, q, &d, &e);
            --d;
            break;
        case 5:
            h = pq(h, p, &e, &c);
            h = pq(h, q, &e, &d);
            --c;
            --e;
            break;
        case 6:
            h = pq(h, p, &d, &c);
            h = pq(h, r, &d, &e);
            --c;
            --d;
            break;
        case 7:
            h = pq(h, q, &d, &c);
            h = pq(h, r, &e, &c);
            --c;
            --d;
            --e;
            break;
    }
    if (!h) return 0;
    if (h == inf || h == infm) return inf * signf(x) * signf(y) * signf(y - x);
    return h * jtdgamma(jt, 1 + c) / (jtdgamma(jt, 1 + d) * jtdgamma(jt, 1 + e));
} /* x and y-x are not negative integers */

static D
ibin(D x, D y) {
    D d = MIN(x, y - x), p = 1;
    // if x and y are _, d is NaN.  Conversion to int is undefined then.  Test for it, but we're gonna get a NaN error
    // when we finish
    if (_isnan(d)) return 0.0;  // avoid looping if d is invalid now
    DQ((I)d, p *= y-- / d--; if (p == inf) return p;);
    return jround(p);
} /* x and y are non-negative integers; x<=y */

static Z
jtzbin(J jt, Z x, Z y) {
    Z a, b, c;
    a = jtzgamma(jt, jtzplus(jt, z1, y));
    b = jtzgamma(jt, jtzplus(jt, z1, x));
    c = jtzgamma(jt, jtzplus(jt, z1, jtzminus(jt, y, x)));
    return jtzdiv(jt, a, jtztymes(jt, b, c));
}

#define MOD2(x) ((x)-2 * floor(0.5 * (x)))

D
jtbindd(J jt, D x, D y) {
    B id, ix, iy;
    D d;
    if (_isnan(x))
        return x;
    else if (_isnan(y))
        return y;
    d  = y - x;
    id = d == floor(d);
    ix = x == floor(x);
    iy = y == floor(y);
    switch (4 * (I)(ix && 0 > x) + 2 * (I)(iy && 0 > y) + (I)(id && 0 > d)) {
        default: ASSERTSYS(0, "bindd");  // jtbindd(jt,(x),(y))
        case 5: /* 1 0 1 */              /* Impossible */
        case 0:                          /* 0 0 0 */
        case 2: /* 0 1 0 */ return ix && iy ? ibin(x, y) : jtdbin(jt, x, y);
        case 3: /* 0 1 1 */ return (MOD2(x) ? -1 : 1) * ibin(x, x - y - 1);
        case 6: /* 1 1 0 */ return (MOD2(d) ? -1 : 1) * ibin(-1 - y, -1 - x);
        case 1: /* 0 0 1 */
        case 4: /* 1 0 0 */
        case 7: /* 1 1 1 */ return 0;
    }
} /* P.C. Berry, Sharp APL Reference Manual, 1979, p. 132 */

static Z
jtbinzz(J jt, Z x, Z y) {
    B id, ix, iy;
    D rd, rx, ry;
    Z d;
    if (!x.im && !y.im) return zrj0(jtbindd(jt, x.re, y.re));
    d  = jtzminus(jt, y, x);
    rd = d.re;
    id = rd == floor(rd) && 0 == d.im;
    rx = x.re;
    ix = rx == floor(rx) && 0 == x.im;
    ry = y.re;
    iy = ry == floor(ry) && 0 == y.im;
    switch (4 * (I)(ix && 0 > rx) + 2 * (I)(iy && 0 > ry) + (I)(id && 0 > rd)) {
        default: ZASSERT(0, EVSYSTEM);
        case 5: /* 1 0 1 */ /* Impossible */
        case 0:             /* 0 0 0 */
        case 2: /* 0 1 0 */ return jtzbin(jt, x, y);
        case 3: /* 0 1 1 */ return zrj0((MOD2(rx) ? -1 : 1) * ibin(rx, rx - ry - 1));
        case 6: /* 1 1 0 */ return zrj0((MOD2(rd) ? -1 : 1) * ibin(-1 - ry, -1 - rx));
        case 1: /* 0 0 1 */
        case 4: /* 1 0 0 */
        case 7: /* 1 1 1 */ return zeroZ;
    }
}
// TODO: remove bindd and binzz
#define bindd(x, y) jtbindd(jt, (x), (y))
APFX(binDD, D, D, D, bindd, NAN0;, HDR1JERRNAN)
#undef bindd
#define binzz(x, y) jtbinzz(jt, (x), (y))
APFX(binZZ, Z, Z, Z, binzz, NAN0;, HDR1JERRNAN)
#undef binzz
