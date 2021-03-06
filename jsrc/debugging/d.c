/* Copyright 1990-2006, Jsoftware Inc.  All rights reserved.               */
/* Licensed use only. Any other use is in violation of copyright.          */
/*                                                                         */
/* Debug: Error Signalling and Display                                     */

#include "j.h"
#include "d.h"

// All the 'display' routines in this file simply add characters to the error buffer.  They can't be typed
// here, because the error may be captured by a higher-level routine

// add n chars at *s to the error buffer at jt->etxn, increment jt->etxn
static void
jtep(J jt, I n, C* s) {
    I m;
    m = NETX - jt->etxn;
    m = MIN(n, m);
    if (0 < m) {
        memcpy(jt->etx + jt->etxn, s, m);
        jt->etxn += m;
    }
}

static void
jteputs(J jt, C* s) {
    jtep(jt, (I)strlen(s), s);
}

static void
jteputc(J jt, C c) {
    jtep(jt, 1L, &c);
}

static void
jteputl(J jt, A w) {
    jtep(jt, AN(w), CAV(w));
    jteputc(jt, CLF);
}

static void
jteputv(J jt, A w) {
    I m = NETX - jt->etxn;
    if (m > 0) { jt->etxn += thv(w, MIN(m, 200), jt->etx + jt->etxn); }
}  // stop writing when there is no room in the buffer
   /* numeric vector w */

static void
jteputq(J jt, A w, I nflag) {
    C q = CQUOTE, *s;
    if (jtequ(jt, ds(CALP), w))
        jteputs(jt, " a." + !nflag);
    else {
        jteputc(jt, q);
        s = CAV(w);
        DO(AN(w), jteputc(jt, s[i]); if (q == s[i]) jteputc(jt, q););
        jteputc(jt, q);
    }
} /* string w, possibly with quotes */

static void
jtefmt(J jt, C* s, I i) {
    if (15 < NETX - jt->etxn) {
        C* v = jt->etx + jt->etxn;
        sprintf(v, s, i);
        jt->etxn += strlen(v);
    }
}

void
jtshowerr(J jt) {
    C b[1 + 2 * NETX], *p, *q, *r;
    if (jt->etxn && jt->tostdout) {
        p = b;
        q = jt->etx;
        r = q + jt->etxn;
        while (q < r && p < b + 2 * NETX - 3) {
            if (*q == CLF) {
                strcpy(p, jt->outseq);
                p += strlen(jt->outseq);
                ++q;
            } else
                *p++ = *q++;
        }  // avoid buffer overrun on huge typeouts
        *p = 0;

        jsto(jt, MTYOER, b);
    }
    jt->etxn = 0;
}

static void
jtdspell(J jt, C id, A w, I nflag) {
    C c, s[5];
    if (id == CFCONS) {
        if (nflag) jteputc(jt, ' ');
        jteputv(jt, FAV(w)->fgh[2]);
        jteputc(jt, ':');
    } else {
        s[0] = ' ';
        s[4] = 0;
        spellit(id, 1 + s);
        c = s[1];
        jteputs(jt, s + !(c == CESC1 || c == CESC2 || nflag && ((ctype[(UC)c] & ~CA) == 0)));
    }
}

static A
jtsfn0(J jt, A w) {
    return jtsfn(jt, 0, w);
}  // return string form of full name for a NAME block
EVERYFS(sfn0overself, jtsfn0, jtover, 0, VFLAGNONE)

// print a noun; nflag if space needed before name/numeric; return new value of nflag
static I
jtdisp(J jt, A w, I nflag) {
    B b = 1 && AT(w) & NAME + NUMERIC;
    if (b && nflag) jteputc(jt, ' ');
    switch (CTTZ(AT(w))) {
        case B01X:
        case INTX:
        case FLX:
        case CMPXX:
        case XNUMX:
        case RATX: jteputv(jt, w); break;
        case BOXX:
            if (!(AT(w) & BOXMULTIASSIGN)) {
                jteputs(jt, " a:" + !nflag);
                break;
            }
            // If this is an array of names, turn it back into a character string with spaces between
            else {
                w = jtcurtail(jt, jtraze(jt, every2(jtevery(jt, w, (A)&sfn0overself), chrspace, (A)&sfn0overself)));
            }  // }: (string&.> names) ,&.> ' '  then fall through to display it
        case LITX: jteputq(jt, w, nflag); break;
        case NAMEX: jtep(jt, AN(w), NAV(w)->s); break;
        case LPARX: jteputc(jt, '('); break;
        case RPARX: jteputc(jt, ')'); break;
        case ASGNX: dspell(CAV(w)[0], w, nflag); break;
        case MARKX: break;
        default: dspell(FAV(w)->id, w, nflag); break;
    }
    return b;  // new nflag
}

// display DCPARSE stack frame
static void
jtseeparse(J jt, DC d) {
    A* v;
    I m;
    v       = (A*)d->dcy;  /* list of tokens */
    m       = d->dcix - 1; /* index of active token when error found */
    I nflag = 0;
    I m1    = jt->etxn;  // starting index of sentence text
    DO(d->dcn, if (i == m) jteputs(jt, "    ");
       nflag = jtdisp(jt, v[i], nflag););  // display tokens with spaces before error
    if (jt->etxn < NETX) {  // if we overran the buffer, don't reformat it.  Reformatting requires splitting to words
        // We displayed the sentence.  See if it contains (9 :'string'); if so, replace with {{ string }}
        fauxblock(fauxw);
        A z      = (A)&fauxw;
        AK(z)    = jt->etx + m1 - (C*)z;
        AFLAG(z) = 0;
        AT(z)    = LIT;
        AC(z)    = ACUC1;
        AR(z)    = 1;
        AN(z) = AS(z)[0] = jt->etxn - m1;                 // point to etx for parsed line
        jtunDD((J)((I)jt | JTINPLACEW | JTINPLACEA), z);  // reformat in place
        jt->etxn = m1 + AN(z);                            // set new end-of-sentence pointer
    }
} /* display error line */

A
jtunparse(J jt, A w) {
    A *v, z;
    jt->etxn = 0;
    I nflag  = 0;
    v        = AAV(w);
    DO(AN(w), nflag = jtdisp(jt, v[i], nflag););
    z        = jtstr(jt, jt->etxn, jt->etx);
    jt->etxn = 0;
    return z;
}

// Display DCCALL stack frame
static void
jtseecall(J jt, DC d) {
    A a;
    if (a = d->dca) jtep(jt, AN(a), NAV(a)->s);
    jtefmt(jt, d->dcx && d->dcy ? "[:" FMTI "]" : "[" FMTI "]", lnumsi(d));
} /* display function line */

// display error-message line
static void
jtdhead(J jt, C k, DC d) {
    C s[] = "    ";
    s[0]  = d && d->dcsusp ? '*' : '|';
    jtep(jt, k + 1L, s);
} /* preface stack display line */

void
jtdebdisp(J jt, DC d) {
    A *x, y;
    I e, t;
    e = d->dcj;  // error #, or 0 if no error (if DCCALL or DCPARSE frame)
    t = d->dctype;
    if (e && !jt->etxn && (t == DCPARSE || t == DCCALL)) {
        x = e + AAV(jt->evm);
        jtdhead(jt, 0, 0L);
        jteputl(jt, *x);
    }  // if error, display error header
    switch (t) {
        case DCPARSE:
            jtdhead(jt, 3, d);
            jtseeparse(jt, d);
            if (NETX == jt->etxn) --jt->etxn;
            jteputc(jt, CLF);
            break;
        case DCCALL:
            jtdhead(jt, 0, d);
            jtseecall(jt, d);
            jteputc(jt, CLF);
            break;
        case DCSCRIPT:
            jtdhead(jt, 0, d);
            jtefmt(jt, "[-" FMTI "] ", d->dcn - 1);
            if (0 <= d->dcm) {
                y = *(d->dcm + AAV(jt->slist));
                jtep(jt, AN(y), CAV(y));
            }
            jteputc(jt, CLF);
            break;
    }
}

static B
jtdebsi1(J jt, DC d) {
    I t;
    RZ(d);
    t = d->dctype;
    jtdebdisp(jt, d);
    d = d->dclnk;
    RZ(d && t == DCPARSE);
    t = d->dctype;
    RZ(t == DCSCRIPT || t == DCCALL && d->dcloc);
    jtdebdisp(jt, d);
    return 1;
}

A
jtdbstack(J jt, A w) {
    DC d = jt->sitop;
    ASSERTMTV(w);
    if (d) {
        if (DCCALL != d->dctype) d = d->dclnk;
        while (d) {
            jtdebdisp(jt, d);
            d = d->dclnk;
        }
    }
    return mtm;
} /* 13!:1  display SI stack */

A
jtdbstackz(J jt, A w) {
    A y, z;
    RE(jtdbstack(jt, w));
    RZ(y = jtstr(jt, jt->etxn, jt->etx));
    jt->etxn = 0;
    return df1(z, y, jtcut(jt, ds(CLEFT), num(-2)));
} /* 13!:18  SI stack as result */

static void
jtjsigstr(J jt, I e, I n, C* s) {
    if (jt->jerr) {
        jt->curname = 0;
        return;
    }  // clear error-name indicator
    if (e != EVSTOP) moveparseinfotosi(jt);
    jt->jerr  = (C)e;
    jt->jerr1 = (C)e;
    jt->etxn = 0;  // before we display, move error info from parse variables to si; but if STOP, it's already installed
    jtdhead(jt, 0, 0L);
    if (jt->uflags.us.cx.cx_c.db && !jtspc(jt)) {
        jteputs(jt, "ws full (can not suspend)");
        jteputc(jt, CLF);
        jt->uflags.us.cx.cx_c.db = 0;
    }
    jtep(jt, n, s);
    if (jt->curname) {
        if (!jt->uflags.us.cx.cx_c.glock) {
            jteputs(jt, ": ");
            jtep(jt, AN(jt->curname), NAV(jt->curname)->s);
        }
        jt->curname = 0;
    }
    jteputc(jt, CLF);
    if (n && !jt->uflags.us.cx.cx_c.glock) jtdebsi1(jt, jt->sitop);
    jt->etxn1 = jt->etxn;
} /* signal error e with error text s of length n */

static void
jtjsig(J jt, I e, A x) {
    jsigstr(e, AN(x), CAV(x));
}
/* signal error e with error text x */

void
jtjsigd(J jt, C* s) {
    C buf[100], *d = "domain error: ";
    I m, n, p;
    m = strlen(d);
    memcpy(buf, d, m);
    n = strlen(s);
    p = MIN(n, 100 - m);
    memcpy(buf + m, s, p);
    jsigstr(EVDOMAIN, m + p, buf);
}

void
jtjsignal(J jt, I e) {
    A x;
    if (EVATTN == e || EVBREAK == e || e == EVINPRUPT) *jt->adbreak = 0;
    // template for debug break point
    // if(EVDOMAIN==e){
    // fprintf(stderr,"domain error\n");
    // }
    // Errors > NEVM are internal-only errors that should never make it to the end of execution.
    // Ignore them here - they will not be displayed
    x = BETWEENC(e, 1, NEVM) ? AAV(jt->evm)[e] : mtv;
    jsigstr(e, AN(x), CAV(x));
}

void
jtjsignal3(J jt, I e, A w, I j) {
    if (jt->jerr) return;
    moveparseinfotosi(jt);
    jt->jerr  = (C)e;
    jt->jerr1 = (C)e;
    jt->etxn  = 0;  // before we display, move error info from parse variables to si
    jtdhead(jt, 0, 0L);
    if (jt->uflags.us.cx.cx_c.db && !jtspc(jt)) {
        jteputs(jt, "ws full (can not suspend)");
        jteputc(jt, CLF);
        jt->uflags.us.cx.cx_c.db = 0;
    }
    jteputl(jt, AAV(jt->evm)[jt->jerr]);
    if (!jt->uflags.us.cx.cx_c.glock) {
        if (e == EVCTRL) {
            jtdhead(jt, 3, 0L);
            jtefmt(jt, "[" FMTI "]", j);
            jteputl(jt, w);
        } else {
            jtdhead(jt, 3, 0L);
            jteputl(jt, w);
            jtdhead(jt, 3, 0L);
            DQ(j, jteputc(jt, ' '););
            jteputc(jt, '^');
            jteputc(jt, CLF);
        }
        jtdebsi1(jt, jt->sitop);
    }
    jt->etxn1 = jt->etxn;
} /* signal error e on line w with caret at j */

static A
jtdbsig(J jt, A a, A w) {
    I e;
    RE(0);
    if (!AN(w)) return mtm;
    RZ(w = jtvi(jt, w));
    e = AV(w)[0];
    ASSERT(1 <= e, EVDOMAIN);
    ASSERT(e <= 255, EVLIMIT);
    if (a || e > NEVM) {
        if (!a) a = mtv;
        RZ(a = jtvs(jt, a));
        jtjsig(jt, e, a);
    } else
        jtjsignal(jt, e);
    return 0;
}

A
jtdbsig1(J jt, A w) {
    return jtdbsig(jt, 0L, w);
} /* 13!:8  signal error */
A
jtdbsig2(J jt, A a, A w) {
    return jtdbsig(jt, a, w);
}

A
jtdberr(J jt, A w) {
    ASSERTMTV(w);
    return jtsc(jt, jt->jerr1);
} /* 13!:11 last error number   */
A
jtdbetx(J jt, A w) {
    ASSERTMTV(w);
    return jtstr(jt, jt->etxn1, jt->etx);
} /* 13!:12 last error text     */

A
jtjerrno(J jt) {
    switch (errno) {
        case EMFILE:
        case ENFILE: jtjsignal(jt, EVLIMIT); return 0;
        case ENOENT: jtjsignal(jt, EVFNAME); return 0;
        case EBADF: jtjsignal(jt, EVFNUM); return 0;
        case EACCES: jtjsignal(jt, EVFACCESS); return 0;
        default: jtjsignal(jt, EVFACE); return 0;
    }
} /* see <errno.h> / <winerror.h> */
