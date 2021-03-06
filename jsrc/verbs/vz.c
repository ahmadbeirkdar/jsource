/* Copyright 1990-2009, Jsoftware Inc.  All rights reserved.               */
/* Licensed use only. Any other use is in violation of copyright.          */
/*                                                                         */
/* Verbs: Complex-Valued Scalar Functions                                  */

#include "j.h"
#include "vcomp.h"

static const Z zj = {0, 1};
static const Z z1 = {1, 0};

static D
hypoth(D u, D v) {
    D p, q, t;
    MMM(u, v);
    return INF(p) ? inf : p ? (t = q / p, p * sqrt(1 + t * t)) : 0;
}

static Z
jtzjx(J jt, Z v) {
    Z z;
    z.re = -v.im;
    z.im = v.re;
    return z;
}
static Z
jtzmj(J jt, Z v) {
    Z z;
    z.re = v.im;
    z.im = -v.re;
    return z;
}

Z
zrj0(D a) {
    Z z;
    z.re = a;
    z.im = 0.0;
    return z;
}

ZS1(jtzconjug, zr = a; zi = -b;)

ZS2(jtzplus, zr = a + c; zi = b + d;)

ZS2(jtzminus, zr = a - c; zi = b - d;)

Z
jtztrend(J jt, Z v) {
    D a, b, t;
    Z z;
    a = v.re;
    b = v.im;
    if (ZOV(v)) {
        a /= 2;
        b /= 2;
    }
    t = hypoth(a, b);
    if (t < inf) {
        if (!t) ++t;
        z.re = a / t;
        z.im = b / t;
    } else
        switch ((INF(a) ? 2 : 0) + INF(b)) {
            case 1:
                z.re = 0.0;
                z.im = (D)SGN(b);
                break;
            case 2:
                z.re = (D)SGN(a);
                z.im = 0.0;
                break;
            case 3: ZASSERT(0, EVNAN);
        }
    return z;
}

Z
jtztymes(J jt, Z u, Z v) {
    D a, b, c, d;
    Z z;
    a    = u.re;
    b    = u.im;
    c    = v.re;
    d    = v.im;
    z.re = TYMES(a, c) - TYMES(b, d);
    z.im = TYMES(a, d) + TYMES(b, c);
    return z;
}

Z
jtzdiv(J jt, Z u, Z v) {
    ZF2DECL;
    D t;
    if (ZNZ(v)) {
        if (ABS(c) < ABS(d)) {
            t = a;
            a = -b;
            b = t;
            t = c;
            c = -d;
            d = t;
        }
        a /= c;
        b /= c;
        d /= c;
        t  = 1 + d * d;
        zr = (a + TYMES(b, d)) / t;
        zi = (b - TYMES(a, d)) / t;
    } else if (ZNZ(u))
        switch (2 * (I)(0 > a) + (I)(0 > b)) {
            case 0:
                if (a > b)
                    zr = inf;
                else
                    zi = inf;
                break;
            case 1:
                if (a > -b)
                    zr = inf;
                else
                    zi = -inf;
                break;
            case 2:
                if (a < -b)
                    zr = -inf;
                else
                    zi = inf;
                break;
            case 3:
                if (a < b)
                    zr = -inf;
                else
                    zi = -inf;
        }
    ZEPILOG;
}

Z
jtznegate(J jt, Z v) {
    return jtzminus(jt, zeroZ, v);
}

D
zmag(Z v) {
    return hypoth(v.re, v.im);
}

B
jtzeq(J jt, Z u, Z v) {
    D a = u.re, b = u.im, c = v.re, d = v.im, p, q;
    if (a == c && TEQ(b, d)) return 1;  // fast check for equality - also picks up cases where one component is infinite
    if (b == d && TEQ(a, c)) return 1;
    if (ZEZ(u) || ZEZ(v) || 1.0 == jt->cct || (0 > a != 0 > c && 0 > b != 0 > d)) return 0;
    if (ZOV(u) || ZOV(v)) {
        a /= 2;
        b /= 2;
        c /= 2;
        d /= 2;
    }
    if (ZUN(u) || ZUN(v)) {
        a *= 2;
        b *= 2;
        c *= 2;
        d *= 2;
    }
    p = hypoth(a, b);
    q = hypoth(c, d);
    return p != inf && q != inf && hypoth(a - c, b - d) <= (1.0 - jt->cct) * MAX(p, q);
}

Z
jtzfloor(J jt, Z v) {
    D p, q;
    ZF1DECL;
    zr = floor(a);
    p  = a - zr;
    zi = floor(b);
    q  = b - zi;
    if (1 <= p + q + (1.0 - jt->cct))
        if (p >= q)
            ++zr;
        else
            ++zi;  // could improve this test
    ZEPILOG;
}

Z
jtzceil(J jt, Z v) {
    return jtznegate(jt, jtzfloor(jt, jtznegate(jt, v)));
}

// u | v
Z
jtzrem(J jt, Z u, Z v) {
    D a, b, d;
    Z q;
    if (ZEZ(u)) return v;  // if u=0, return v
    ZASSERT(!ZINF(v), EVNAN);
    if (INF(u.re) && !u.im && !v.im) {
        // infinity found
        if (u.re == inf) return 0 <= v.re ? v : u;
        if (u.re == infm) return 0 >= v.re ? v : u;
    }
    ZASSERT(!ZINF(u), EVNONCE);
    // general case, return v - u * <. v % u
    // calculate v % u as (v * +u) % |u
    d    = u.re * u.re + u.im * u.im;
    a    = u.re * v.re + u.im * v.im;
    b    = u.re * v.im - u.im * v.re;
    q.re = a / d;
    q.im = b / d;
    q    = jtzfloor(jt, q);  // do proper complex floor
    return jtzminus(jt, v, jtztymes(jt, u, q));
}

Z
jtzgcd(J jt, Z u, Z v) {
    D a, b;
    Z t, z;
    I lim;
    ZASSERT(!(ZINF(u) || ZINF(v)), EVNAN);
    for (lim = 2048; lim > 0 && ZNZ(u); --lim) {
        t    = jtzrem(jt, u, v);
        v.re = u.re;
        v.im = u.im;
        u.re = t.re;
        u.im = t.im;
    }                            // max # iters is log(MAXFLOAT)/log(phi)
    if (lim == 0) return zeroZ;  // if Euclid failed, return 0j0
    z.re = a = v.re;
    z.im = b = v.im;
    switch (2 * (I)(0 > a) + (I)(0 > b)) {
        case 0:
            if (!a) {
                z.re = b;
                z.im = 0;
            }
            break;
        case 1:
            z.re = -b;
            z.im = a;
            break;
        case 2:
            if (!b) {
                z.re = -a;
                z.im = 0;
            } else {
                z.re = b;
                z.im = -a;
            }
            break;
        case 3: z.re = -a; z.im = -b;
    }
    return z;
}

Z
jtzlcm(J jt, Z u, Z v) {
    ZASSERT(!(ZINF(u) || ZINF(v)), EVNAN);
    return ZEZ(u) || ZEZ(v) ? zeroZ : jtztymes(jt, u, jtzdiv(jt, v, jtzgcd(jt, u, v)));
}

Z
jtzexp(J jt, Z v) {
    D a, b, c, s, t;
    Z z;
    a = v.re;
    b = v.im;
    if (a < EMIN)
        z.re = z.im = 0.0;
    else {
        ZASSERT(-THMAX < b && b < THMAX, EVLIMIT);
        c = cos(b);
        s = sin(b);
        if (a <= EMAX) {
            t    = exp(a);
            z.re = t * c;
            z.im = t * s;
        } else {
            if (!c)
                z.re = 0;
            else {
                t    = a + log(ABS(c));
                t    = EMAX < t ? inf : exp(t);
                z.re = 0 > c ? -t : t;
            }
            if (!s)
                z.im = 0;
            else {
                t    = a + log(ABS(s));
                t    = EMAX < t ? inf : exp(t);
                z.im = 0 > s ? -t : t;
            }
        }
    }
    return z;
}

Z
jtzlog(J jt, Z v) {
    ZF1DECL;
    zr = b ? log(hypoth(a, b)) : INF(a) ? inf : a ? log(hypoth(a, b)) : -inf;
    zi = a || b ? atan2(b, a) : 0;

    ZEPILOG;
}

Z
jtzpow(J jt, Z u, Z v) {
    ZF2DECL;
    D m;
    I n;
    if (!a && !b) {
        z.re = d ? 0 : 0 > c ? inf : !c;
        z.im = 0;
        return z;
    }
    if (!d && IMIN < c && c <= FLIMAX && (n = (I)floor(c), c == n)) {
        if (0 > n) {
            u = jtzdiv(jt, z1, u);
            n = -n;
        }
        z = z1;
        while (n) {
            if (1 & n) z = jtztymes(jt, z, u);
            u = jtztymes(jt, u, u);
            n >>= 1;
        }
        return z;
    }
    z = jtzexp(jt, jtztymes(jt, v, jtzlog(jt, u)));
    if (!b && !d) {
        m = floor(c);
        if (0 > a && c > m && c == 0.5 + m) z.re = 0;
        if (c == m) z.im = 0;
    }
    return z;
}

Z
jtzsqrt(J jt, Z v) {
    D p, q, t;
    ZF1DECL;
    MMM(a, b);
    if (p) {
        t  = 0.5 * q / p;
        zr = sqrt(ABS(a / 2) + p * sqrt(0.25 + t * t));
        zi = b / (zr + zr);
        if (0 > a) {
            t  = ABS(zi);
            zi = 0 > b ? -zr : zr;
            zr = t;
        }
    }
    ZEPILOG;
}

/* See Abramowitz & Stegun, Handbook of Mathematical Functions,            */
/*   National Bureau of Standards, 1964 6.                                 */

static Z
jtzsin(J jt, Z v) {
    D a, b, c, s;
    Z z;
    a = v.re;
    b = v.im;
    ZASSERT(-THMAX < a && a < THMAX, EVLIMIT);
    s    = sin(a);
    z.re = s ? s * (b < -EMAX2 || EMAX2 < b ? inf : cosh(b)) : 0.0;
    c    = cos(a);
    z.im = c ? c * (b < -EMAX2 ? infm : EMAX2 < b ? inf : sinh(b)) : 0.0;
    return z;
} /* 4.3.55 */

static Z
jtzcos(J jt, Z v) {
    D a, b, c, s;
    Z z;
    a = v.re;
    b = v.im;
    ZASSERT(-THMAX < a && a < THMAX, EVLIMIT);
    c    = cos(a);
    z.re = c ? c * (b < -EMAX2 || EMAX2 < b ? inf : cosh(b)) : 0.0;
    s    = sin(a);
    z.im = s ? -s * (b < -EMAX2 ? infm : EMAX2 < b ? inf : sinh(b)) : 0.0;
    return z;
} /* 4.3.56 */

static Z
jtztan(J jt, Z v) {
    return jtzdiv(jt, jtzsin(jt, v), jtzcos(jt, v));
}

// bug in some versions of Visual Studio
#pragma auto_inline(off)
static Z
jtzp4(J jt, Z v) {
    return jtzsqrt(jt, jtzplus(jt, z1, jtztymes(jt, v, v)));
}
#pragma auto_inline(on)

static Z
jtzm4(J jt, Z v) {
    return 1e16 < hypoth(v.re, v.im)
             ? v
             : jtztymes(jt, jtzplus(jt, v, z1), jtzsqrt(jt, jtzdiv(jt, jtzminus(jt, v, z1), jtzplus(jt, v, z1))));
}

static Z
jtzsinh(J jt, Z v) {
    return jtzmj(jt, jtzsin(jt, jtzjx(jt, v)));
} /* 4.5.7 */

static Z
jtzcosh(J jt, Z v) {
    return jtzcos(jt, jtzjx(jt, v));
} /* 4.5.8 */

static Z
jtztanh(J jt, Z v) {
    return v.re < -TMAX ? zrj0(-1.0) : TMAX < v.re ? z1 : jtzdiv(jt, jtzsinh(jt, v), jtzcosh(jt, v));
}

static Z
jtzp8(J jt, Z v) {
    return jtzsqrt(jt, jtztymes(jt, jtzplus(jt, zj, v), jtzminus(jt, zj, v)));
}

static Z
jtzasinh(J jt, Z v) {
    return 0 > v.re ? jtznegate(jt, jtzasinh(jt, jtznegate(jt, v))) : jtzlog(jt, jtzplus(jt, v, jtzp4(jt, v)));
}

static Z
jtzacosh(J jt, Z v) {
    Z z;
    z = jtzlog(jt, jtzplus(jt, v, jtzm4(jt, v)));
    if (0 >= z.re) {
        z.re = 0;
        z.im = ABS(z.im);
    }
    return z;
}

static Z
jtzatanh(J jt, Z v) {
    return jtztymes(jt, zrj0((D)0.5), jtzlog(jt, jtzdiv(jt, jtzplus(jt, z1, v), jtzminus(jt, z1, v))));
}

static Z
jtzatan(J jt, Z v) {
    ZF1DECL;
    if (!b && (a < -1e13 || 1e13 < a)) return zrj0(0 < a ? PI / 2.0 : -PI / 2.0);
    z = jtzmj(jt, jtzatanh(jt, jtzjx(jt, v)));
    if (!b) z.im = 0;
    return z;
} /* 4.4.22 */

// NaN bug in android asin() _1 o. _1

static Z
jtzasin(J jt, Z v) {
    return !v.im && -1 <= v.re && v.re <= 1 ? zrj0(asin(v.re)) : jtzmj(jt, jtzasinh(jt, jtzjx(jt, v)));
} /* 4.4.20 */

static Z
jtzacos(J jt, Z v) {
    return jtzminus(jt, zrj0(PI / 2.0), jtzasin(jt, v));
}

static Z
jtzarc(J jt, Z v) {
    D x, y;
    Z t, z;
    z.re = z.im = 0;
    t           = jtztrend(jt, v);
    x           = t.re;
    y           = t.im;
    if (0 != x || 0 != y) z.re = atan2(y, x);

#

    return z;
}

Z
jtzcir(J jt, Z u, Z v) {
    D r;
    I x;
    Z z;
    z = zeroZ;
    r = u.re;
    x = (I)jround(r);
    ZASSERT(BETWEENC(x, -12, 12) && FFEQ(x, r) && !u.im, EVDOMAIN);  // x must be integer
    switch (x) {
        default: ZASSERT(0, EVDOMAIN);
        case 0: return jtzsqrt(jt, jtztymes(jt, jtzplus(jt, z1, v), jtzminus(jt, z1, v)));
        case 1: return jtzsin(jt, v);
        case -1: return jtzasin(jt, v);
        case 2: return jtzcos(jt, v);
        case -2: return jtzacos(jt, v);
        case 3: return jtztan(jt, v);
        case -3: return jtzatan(jt, v);
        case 4: return jtzp4(jt, v);
        case -4: return jtzm4(jt, v);
        case 5: return jtzsinh(jt, v);
        case -5: return jtzasinh(jt, v);
        case 6: return jtzcosh(jt, v);
        case -6: return jtzacosh(jt, v);
        case 7: return jtztanh(jt, v);
        case -7: return jtzatanh(jt, v);
        case 8: return jtzp8(jt, v);
        case -8: return jtznegate(jt, jtzp8(jt, v));
        case 9: z.re = v.re; return z;
        case -9: return v;
        case 10: z.re = zmag(v); return z;
        case -10: return jtzconjug(jt, v);
        case 11: z.re = v.im; return z;
        case -11: return jtzjx(jt, v);
        case 12: return jtzarc(jt, v);
        case -12: return jtzexp(jt, jtzjx(jt, v));
    }
}

B
jtztridiag(J jt, I n, A a, A x) {
    I i, j, n1 = n - 1;
    Z *av, d, p, *xv;
    av = ZAV(a);
    xv = ZAV(x);
    d  = xv[0];
    for (i = j = 0; i < n1; ++i) {
        ASSERT(ZNZ(d), EVDOMAIN);
        p         = jtzdiv(jt, xv[j + 2], d);
        xv[j + 3] = d = jtzminus(jt, xv[j + 3], jtztymes(jt, p, xv[j + 1]));
        av[i + 1]     = jtzminus(jt, av[i + 1], jtztymes(jt, p, av[i]));
        j += 3;
    }
    ASSERT(ZNZ(d), EVDOMAIN);
    i     = n - 1;
    j     = AN(x) - 1;
    av[i] = jtzdiv(jt, av[i], d);
    for (i = n - 2; i >= 0; --i) {
        j -= 3;
        av[i] = jtzdiv(jt, jtzminus(jt, av[i], jtztymes(jt, xv[j + 1], av[i + 1])), xv[j]);
    }
    return 1;
}

A
jtexppi(J jt, A w, A self) {
    A z;
    B b;
    D r, th, y;
    I k;
    Z *v, t;
    F1RANK(0, jtexppi, UNUSED_VALUE);
    if (!(CMPX & AT(w))) return expn1(pix(w));
    v = ZAV(w);
    r = exp(PI * v->re);
    y = v->im;
    if (b = 0 > y) y = -y;
    th = y - 2 * (I)(y / 2);
    k  = (I)(2 * th);
    if (k != 2 * th)
        k = -1;
    else if (b && k)
        k = 4 - k;
    if (!((UI)k <= (UI)3)) return expn1(pix(w));
    switch (k) {
        case 0:
            t.re = r;
            t.im = 0;
            break;
        case 1:
            t.re = 0;
            t.im = r;
            break;
        case 2:
            t.re = -r;
            t.im = 0;
            break;
        case 3:
            t.re = 0;
            t.im = -r;
            break;
    }
    GAT0(z, CMPX, 1, 0);
    ZAV(z)[0] = t;
    return z;
} /* special code for ^@o. */

Z
jtznonce1(J jt, Z v) {
    ZASSERT(0, EVNONCE);
}

Z
jtznonce2(J jt, Z u, Z v) {
    ZASSERT(0, EVNONCE);
}
