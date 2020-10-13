/* Copyright 1990-2016, Jsoftware Inc.  All rights reserved.               */
/* Licensed use only. Any other use is in violation of copyright.          */
/*                                                                         */
/* Conjunctions: Explicit Definition : and Associates                      */

/* Usage of the f,g,h fields of : defined verbs:                           */
/*  f  character matrix of  left argument to :                             */
/*  g  character matrix of right argument to :                             */
/*  h  4-element vector of boxes                                           */
/*       0  vector of boxed tokens for f                                   */
/*       1  vector of triples of control information                       */
/*       2  vector of boxed tokens for g                                   */
/*       3  vector of triples of control information                       */

#include "j.h"
#include "d.h"
#include "p.h"
#include "w.h"

// DD definitions
#define DDBGN (US)('('+256*'.')  // digraph for start DD
#define DDEND (US)(')'+256*'.')  // digraph for end DD
#define DDSEP 0xf  // ASCII value used to mark line separator inside 9 : string.  Must have class CU so that it ends a final comment

#define BASSERT(b,e)   {if(unlikely(!(b))){jsignal(e); i=-1; z=0; continue;}}
#define BZ(e)          if(unlikely(!(e))){i=-1; z=0; continue;}

// sv->h is the A block for the [2][4] array of saved info for the definition; hv->[4] boxes of info for the current valence;
// line-> box 0 - tokens; x->box 1 - A block for control words; n=#control words; cw->array of control-word data, a CW struct for each
#define LINE(sv)       {A x; \
                        hv=AAV(sv->fgh[2])+4*isdyad;  \
                        line=AAV(hv[0]); x=hv[1]; n=AN(x); cw=(CW*)AV(x);}

// Parse/execute a line, result in z.  If locked, reveal nothing.  Save current line number in case we reexecute
// If the sentence passes a u/v into an operator, the current symbol table will become the prev and will have the u/v environment info
// If the sentence fails, we go into debug mode and don't return until the user releases us
#define parseline(z) {C attnval=*jt->adbreakr; A *queue=line+ci->i; I m=ci->n; if(likely(!attnval)){if(likely(!(gsfctdl&16)))z=parsea(queue,m);else {thisframe->dclnk->dcix=i; z=parsex(queue,m,ci,callframe);}}else{jsignal(EVATTN); z=0;} }

typedef struct{A t,x,line;C*iv,*xv;I j,n; I4 k,w;} CDATA;
/* for_xyz. t do. control data   */
/* line  'for_xyz.'              */
/* t     iteration array         */
/* n     #t                      */
/* k     length of name xyz      */
/* x     text xyz_index          */
/* xv    ptr to text xyz_index   */
/* iv    ptr to text xyz         */
/* j     iteration index         */
/* w     cw code                 */

#define WCD            (sizeof(CDATA)/sizeof(I))

typedef struct{I4 d,t,e,b;} TD;  // line numbers of catchd., catcht., end. and try.
#define WTD            (sizeof(TD)/sizeof(I))
#define NTD            17     /* maximum nesting for try/catch */


static B jtforinit(J jt,CDATA*cv,A t){A x;C*s,*v;I k;
 ASSERT(t!=0,EVCTRL);
 RZ(ras(t)); cv->t=t;                            /* iteration array     */
 SETIC(t,cv->n);                            /* # of items in t     */
 cv->j=-1;                               /* iteration index     */
 cv->x=0;
 k=AN(cv->line)-5; cv->k=(I4)k;                 /* length of item name; -1 if omitted (for.; for_. not allowed) */
 if((-k&-cv->n)<0){                         /* for_xyz.       k>0 and cv->n >0     */
  s=4+CAV(cv->line); RZ(x=str(6+k,s)); ras(x); cv->x=x;
  cv->xv=v=CAV(x); MC(k+v,"_index",6L);  /* index name          */
  cv->iv=s;                              /* item name           */
 }
 R 1;
}    /* for. do. end. initializations */

// A for. block is ending.   free the text string "xyz_index" (if saved) and the iteration array, Don't delete any names
static B jtunstackcv(J jt,CDATA*cv){
 if(cv->x){fa(cv->x);}
 fa(cv->t); 
 R 1;
}

static void jttryinit(J jt,TD*v,I i,CW*cw){I j=i,t=0;
 v->b=(I4)i;v->d=v->t=0;
 while(t!=CEND){
  j=(j+cw)->go;  // skip through just the control words for this try. structure
  switch(t=(j+cw)->type){
   case CCATCHD: v->d=(I4)j; break;
   case CCATCHT: v->t=(I4)j; break;
   case CEND:    v->e=(I4)j; break;
}}}  /* processing on hitting try. */

// tdv points to the try stack or 0 if none; tdi is index of NEXT try. slot to fill; i is goto line number
// if there is a try stack, pop its stack frames if they don't include the line number of the goto
// result is new value for tdi
// This is called only if tdi is nonzero & therefore we have a stack
static I trypopgoto(TD* tdv, I tdi, I dest){
 while(tdi&&!BETWEENC(dest,tdv[tdi-1].b,tdv[tdi-1].e))--tdi;  // discard stack frame if structure does not include dest
 R tdi;
}

#define CHECKNOUN if (!(NOUN&AT(t))){   /* error, T block not creating noun */ \
    /* Signal post-exec error*/ \
    t=pee(line,&cw[ti],EVNONNOUN,gsfctdl<<(BW-2),callframe); \
    /* go to error loc; if we are in a try., send this error to the catch.  z may be unprotected, so clear it, to 0 if error shows, mtm otherwise */ \
    i = cw[ti].go; if (i<SMAX){ RESETERR; z=mtm; if (gsfctdl&4){if(!--tdi){jt->uflags.us.cx.cx_c.db=(UC)(gsfctdl>>8); gsfctdl^=4;} } }else z=0; \
    break; }

// Return next line to execute, in case debug changed it
// If debug is running we have to check for a new line to run, after any execution with error or on any line in case the debugger interrupted something
// result is line to continue on
static I debugnewi(I i, DC thisframe, A self){
 if(thisframe){DC siparent;  // if we can take debug input
  // debug mode was on when this execution started.  See if the execution pointer was changed by debug.
  if((siparent=thisframe->dclnk)&&siparent->dctype==DCCALL&&self==siparent->dcf){   // if prev stack frame is a call to here
   if(siparent->dcnewlineno){  // the debugger has asked for a jump
    i=siparent->dcix;  // get the jump-to line
    siparent->dcnewlineno=0;  // reset semaphore
   }
   siparent->dcix=i;  // notify the debugger of the line we are on, in case we stop  
  }
 }
 R i;  // return the current line, possibly modified
}

// Processing of explicit definitions, line by line
DF2(jtxdefn){F2PREFIP;PROLOG(0048);
 RE(0);
 A *line;   // pointer to the words of the definition.  Filled in by LINE
 I n;  // number of lines in the definition.  Filled in by LINE
 CW *cw;  // pointer to control-word info for the definition.  Filled in by LINE
 I gsfctdl=0;  // flags: 1=locked 2=debug(& not locked) 4=tdi!=0 8=cd!=0 16=thisframe!=0 32=symtable was the original (i. e. AR(symtab)&LSYMINUSE) 256=original debug flag (must be highest bit)
 DC callframe=0;  // pointer to the debug frame of the caller to this function (only if it's named), but 0 if we are not debugging

 TD*tdv=0;  // pointer to base of try. stack
 I tdi=0;  // index of the next open slot in the try. stack
 CDATA*cv;  // pointer to the current entry in the for./select. stack

 A locsym;  // local symbol table to use

 I isdyad=(I)(a!=0)&(I)(w!=0);   // avoid branches, and relieve pressure on a and w
 DC thisframe=0;   // if we allocate a parser-stack frame, this is it
 {A *hv;  // will hold pointer to the precompiled parts
  V *sv=FAV(self); I sflg=sv->flag;   // fetch flags, which are the same even if VXOP is set
  A u,v;  // pointers to args
  // If this is adv/conj, it must be (1/2 : n) executed with no x or y.  Set uv then, and undefine x/y
  u=AT(self)&ADV+CONJ?a:0; v=AT(self)&ADV+CONJ?w:0;
// saved for next rev   a=AT(self)&ADV+CONJ?0:a; w=AT(self)&ADV+CONJ?0:w;
  if(likely(!(jt->uflags.us.cx.cx_us | (sflg&(VLOCK|VXOP|VTRY1|VTRY2))))){
   // Normal case of verbs. Read the info for the parsed definition, including control table and number of lines
   LINE(sv);
   // Create symbol table for this execution.  If the original symbol table is not in use (rank unflagged), use it;
   // otherwise clone a copy of it.  We have to do this before we create the debug frame
   // This code duplicated below
   locsym=hv[3];  // fetch pointer to preallocated symbol table
   ASSERT(locsym!=0,EVDOMAIN);  // if the valence is not defined, give valence error
   if(!(AR(locsym)&LSYMINUSE)){AR(locsym)|=LSYMINUSE;gsfctdl|=32;}  // remember if we are using the original symtab
   else{RZ(locsym=clonelocalsyms(locsym));}
  } else {  // something special required
   // If this is a modifier-verb referring to x or y, set u, v to the modifier operands, and sv to the saved modifier (f=type, g=compiled text).  The flags don't change
   if(sflg&VXOP){u=sv->fgh[0]; v=sv->fgh[2]; sv=VAV(sv->fgh[1]);}
   // Read the info for the parsed definition, including control table and number of lines
   LINE(sv);
   locsym=hv[3];  // fetch pointer to preallocated symbol table
   ASSERT(locsym!=0,EVDOMAIN);  // if the valence is not defined, give valence error
   if(!(AR(locsym)&LSYMINUSE)){AR(locsym)|=LSYMINUSE;gsfctdl|=32;}  // remember if we are using the original symtab
   else{RZ(locsym=clonelocalsyms(locsym));}

   gsfctdl|=jt->uflags.us.cx.cx_c.glock||sv->flag&VLOCK;  // 1=lock bit
   // if we are in debug mode, the call to this defn should be on the stack (unless debug was entered under program control).  If it is, point to its
   // stack frame, which functions as a flag to indicate that we are debugging.  If the function is locked we ignore debug mode
   if(!(gsfctdl&1)&&jt->uflags.us.cx.cx_c.db){
    if(jt->sitop&&jt->sitop->dctype==DCCALL){   // if current stack frame is a call
     gsfctdl|=2;   // set debug flag if debug requested and not locked (no longer used)
     if(sv->flag&VNAMED){
      callframe=jt->sitop;  // if this is a named (rather than anonymous call like 3 : 0"1), there is a tight link with the caller.  Indicate that
     }
     // If we are in debug mode, and the current stack frame has the DCCALL type, pass the debugger
     // information about this execution: the local symbols and the control-word table
     if(self==jt->sitop->dcf){  // if the stack frame is for this exec
      jt->sitop->dcloc=locsym; jt->sitop->dcc=hv[1];  // install info about the exec
      // Use this out-of-the-way place to ensure that the compiler will not try to put for. and try. stuff into registers
      forcetomemory(&tdi); forcetomemory(&tdv); forcetomemory(&cv); 
     }
    }

    // With debug on, we will save pointers to the sentence being executed in the stack frame we just allocated
    // We will keep cxspecials set as long as savdcy is 0, i. e. in all levels where debug was set at the start of the frame
    jt->cxspecials=1;
   }

   // If the verb contains try., allocate a try-stack area for it.  Remember debug state coming in so we can restore on exit
   if(sv->flag&VTRY1+VTRY2){A td; GAT0(td,INT,NTD*WTD,1); tdv=(TD*)AV(td); gsfctdl |= jt->uflags.us.cx.cx_c.db<<8;}
  }
  // End of unusual processing
  SYMPUSHLOCAL(locsym);   // Chain the calling symbol table to this one

  // assignsym etc should never be set here; if it is, there must have been a pun-in-ASGSAFE that caused us to mark a
  // derived verb as ASGSAFE and it was later overwritten with an unsafe verb.  That would be a major mess; we'll invest 2 stores
  // in preventing it - still not a full fix, since invalid inplacing may have been done already
  CLEARZOMBIE
  // Assign the special names x y m n u v.  Do this late in initialization because it would be bad to fail after assigning to yx (memory leak would result)
  // For low-rank short verbs, this takes a significant amount of time using IS, because the name doesn't have bucket info and is
  // not an assignment-in-place
  // So, we short-circuit the process by assigning directly to the name.  We take advantage of the fact that we know the
  // order in which the symbols were defined: y then x; and we know that insertions are made at the end; so we know
  // the bucketx for xy are 0 or maybe 1.  We have precalculated the buckets for each table size, so we can install the values
  // directly.
  // Assignments here are special.  At this point we know that the value is coming in from a different namespace, and that it will be
  // freed in that namespace.  Thus, (1) there is no need to realize a virtual block - we can just assign it to x/y, knowing that it is
  // not tying up a backer that could have been freed otherwise (we do have to raise the usecount, but not by ras()); (2) if the input
  // has been abandoned, we do not need to raise the usecount here: we can just mark the arg non-inplaceable, usecount 1 and take advantage
  // of assignment in place here - in this case we must flag the name to suppress decrementing the usecount on reassignment or final free.  In
  // both cases we know the block will be freed by the caller.
  // Virtual abandoned blocks are both cases at once.  That's OK.
  UI4 yxbucks = *(UI4*)LXAV0(locsym);  // get the yx bucket indexes, stored in first hashchain by crelocalsyms
  L *ybuckptr = LXAV0(locsym)[(US)yxbucks]+jt->sympv;  // pointer to sym block for y
  L *xbuckptr = LXAV0(locsym)[yxbucks>>16]+jt->sympv;  // pointer to sym block for x
  if(w){  // If y given, install it & incr usecount as in assignment.  Include the script index of the modification
   // If input is abandoned inplace and not the same as x, DO NOT increment usecount, but mark as abandoned and make not-inplace.  Otherwise ra
   // We can handle an abandoned argument only if it is direct or recursive, since only those values can be assigned to a name
   if((a!=w)&SGNTO0(AC(w)&(((AT(w)^AFLAG(w))&RECURSIBLE)-1))&((I)jtinplace>>JTINPLACEWX)){
    ybuckptr->flag=LPERMANENT|LWASABANDONED; AC(w)&=~ACINPLACE;  // remember, blocks from every may be 0x8..2, and we must preserve the usecount then as if we ra()d it
   }else ra(w);
   ybuckptr->val=w; ybuckptr->sn=jt->slisti;
  }
    // for x (if given), slot is from the beginning of hashchain EXCEPT when that collides with y; then follow y's chain
    // We have verified that hardware CRC32 never results in collision, but the software hashes do (needs to be confirmed on ARM CPU hardware CRC32C)
  if(a){
   if(!C_CRC32C&&xbuckptr==ybuckptr)xbuckptr=xbuckptr->next+jt->sympv;
   if((a!=w)&SGNTO0(AC(a)&(((AT(a)^AFLAG(a))&RECURSIBLE)-1))&((I)jtinplace>>JTINPLACEAX)){
    xbuckptr->flag=LPERMANENT|LWASABANDONED; AC(a)&=~ACINPLACE;
   }else ra(a);
   xbuckptr->val=a; xbuckptr->sn=jt->slisti;
  }
  // Do the other assignments, which occur less frequently, with IS
  if((I)u|(I)v){
   if(u){(IS(mnuvxynam[2],u)); if(NOUN&AT(u))IS(mnuvxynam[0],u); }  // assign u, and m if u is a noun
   if(v){(IS(mnuvxynam[3],v)); if(NOUN&AT(v))IS(mnuvxynam[1],v); }  // bug errors here must be detected
  }
 }
 FDEPINC(1);   // do not use error exit after this point; use BASSERT, BGA, BZ
 // remember tnextpushx.  We will tpop after every sentence to free blocks.  Do this AFTER any memory
 // allocation that has to remain throughout this routine.
 // If the user turns on debugging in the middle of a definition, we will raise old when he does
 A *old=jt->tnextpushp;

 // loop over each sentence
 A cd=0;  // pointer to block holding the for./select. stack, if any
 I r=0;  // number of unused slots allocated in for./select. stack
 I i=0;  // control-word number of the executing line
 A z=mtm;  // last B-block result; will become the result of the execution. z=0 is treated as an error condition inside the loop, so we have to init the result to i. 0 0
 A t=0;  // last T-block result
 I bi;   // cw number of last B-block result.  Needed only if it gets a NONNOUN error
 I ti;   // cw number of last T-block result.  Needed only if it gets a NONNOUN error
 while(1){CW *ci;
  // i holds the control-word number of the current control word
  // Check for debug and other modes
  if(jt->cxspecials){  // fast check to see if we have overhead functions to perform
   if(!(gsfctdl&(16+1))&&jt->uflags.us.cx.cx_c.db){
    // If we haven't done so already, allocate an area to use for the SI entries for sentences executed here, if needed.  We need a new area only if we are debugging.  Don't do it if locked.
    // We have to have 1 debug frame to hold parse-error information in, but it is allocated earlier if debug is off
    // We check before every sentence in case the user turns on debug in the middle of this definition
    // NOTE: this stack frame could be put on the C stack, but that would reduce the recursion limit because the frame is pretty big
    // If there is no calling stack frame we can't turn on debug mode because we can't suspend
    // If we are executing a recursive call to JDo we can't go into debug because we can't prompt
    DC d; for(d=jt->sitop;d&&DCCALL!=d->dctype;d=d->dclnk);  /* find bottommost call                 */
    if(d&&jt->recurstate<RECSTATEPROMPT){  // if there is a call and thus we can suspend; and not prompting already
     BZ(thisframe=deba(DCPARSE,0L,0L,0L));  // if deba fails it will be before it modifies sitop.  Remember our stack frame
     old=jt->tnextpushp;  // protect the stack frame against free
     gsfctdl|=16+2;  // indicate we have a debug frame and are in debug mode
    }
   }

   i=debugnewi(i,thisframe,self);  // get possibly-changed execution line
   if((UI)i>=(UI)n)break;

   // if performance monitor is on, collect data for it
   if(PMCTRBPMON&jt->uflags.us.uq.uq_c.pmctrbstk&&C1==jt->pmrec&&FAV(self)->flag&VNAMED)pmrecord(jt->curname,jt->global?LOCNAME(jt->global):0,i,isdyad?VAL2:VAL1);
   // If the executing verb was reloaded during debug, switch over to the modified definition
   if((gsfctdl&16)&&jt->redefined){DC siparent;A *hv;
    if((siparent=thisframe->dclnk)&&jt->redefined==siparent->dcn&&DCCALL==siparent->dctype&&self!=siparent->dcf){
     self=siparent->dcf; V *sv=FAV(self); LINE(sv); siparent->dcc=hv[1];
     // Clear all local bucket info in the definition, since it doesn't match the symbol table now
     DO(AN(hv[0]), if(AT(line[i])&NAME){NAV(line[i])->bucket=0;});
    }
    jt->redefined=0;
   }
   if(!((I)jt->redefined|(I)jt->pmctr|(I)(gsfctdl&16)))jt->cxspecials=0;  // if no more special work to do, close the gate
  }

  // Don't do the loop-exit test until debug has had the chance to update the execution line.  For example, we might be asked to reexecute the last line of the definition
  if((UI)i>=(UI)n)break;
  ci=i+cw;   // ci->control-word info
  // process the control word according to its type
  I cwtype;
  switch(cwtype=ci->type){
  case CIF: case CWHILE: case CELSEIF: 
   i=ci->go;  // Go to the next sentence, whatever it is
   if((UI)i>=(UI)n||(((cwtype=(ci=i+cw)->type)^CTBLOCK)+jt->cxspecials))break;  // avoid indirect-branch overhead on the likely case
  case CASSERT:
  case CTBLOCK:
tblockcase:
   // execute and parse line as if for B block, except save the result in t
   // If there is a possibility that the previous B result may become the result of this definition,
   // protect it during the frees during the T block.  Otherwise, just free memory
   if(ci->canend&2)tpop(old);else z=gc(z,old);   // 2 means previous B can't be the result
   parseline(t);
   // Check for assert.  Since this is only for T-blocks we tolerate the test (rather than duplicating code)
   if(ci->type==CASSERT&&jt->assert&&t&&!(NOUN&AT(t)&&all1(eq(num(1),t))))t=pee(line,ci,EVASSERT,gsfctdl<<(BW-2),callframe);  // if assert., signal post-execution error if result not all 1s.  May go into debug; sets to result after debug
   if(likely(t!=0)){ti=i,++i;  // if no error, continue on
    if((UI)i<(UI)n&&!(((cwtype=(ci=i+cw)->type)^CDO)+jt->cxspecials))goto docase;  // avoid indirect-branch overhead on the likely case
   }else if((gsfctdl&16)&&DB1&jt->uflags.us.cx.cx_c.db)ti=i,i=debugnewi(i+1,thisframe,self);  // error in debug mode: when coming out of debug, go to new line (there had better be one)
   else if(EVTHROW==jt->jerr){if(gsfctdl&4&&(tdv+tdi-1)->t){i=(tdv+tdi-1)->t+1; RESETERR;}else BASSERT(0,EVTHROW);}  // if throw., and there is a catch., do so
   else{i=ci->go; if(i<SMAX){RESETERR; z=mtm; if(gsfctdl&4){if(!--tdi){jt->uflags.us.cx.cx_c.db=(UC)(gsfctdl>>8); gsfctdl^=4;}}}else z=0;}  // uncaught error: if we take error exit, we might not have protected z, which is not needed anyway; so clear it to prevent invalid use
     // if we are not taking the error exit, we still need to set z to a safe value since we might not have protected it.  This is B1 try. if. error do. end. catch. return. end.
   break;
  case CDO:
docase:
   // do. here is one following if., elseif., or while. .  It always follows a T block, and skips the
   // following B block if the condition is false.
  {A tt=t; tt=t?t:mtv;  // missing t looks like '' which is true
   //  Start by assuming condition is true; set to move to the next line then
   ++i;
   // Quick true cases are: nonexistent t; empty t; direct numeric t with low byte nonzero.  This gets most of the true.  We add in char types and BOX cause it's free (they are always true)
   if(AN(tt)&&(-(AT(tt)&(B01|LIT|INT|FL|CMPX|C2T|C4T|BOX))&-((I)CAV(tt)[0]))>=0){I nexti=ci->go;  // C cond is false if (type direct or BOX) and (value not 0).  J cond is true then.  Musn't fetch CAV[0] if AN==0
    // here the type is indirect or the low byte is 0.  We must compare more
    while(1){  // 2 loops if sparse
     if(AT(tt)&INT+B01){i=BIV0(tt)?i:nexti; break;} // INT and B01 are most common
     if(AT(tt)&FL){i=DAV(tt)[0]?i:nexti; break;}
     if(AT(tt)&CMPX){i=DAV(tt)[0]||DAV(tt)[1]?i:nexti; break;}
     if(AT(tt)&(RAT|XNUM)){i=1<AN(XAV(tt)[0])||IAV(XAV(tt)[0])[0]?i:nexti; break;}
     if(!(AT(tt)&NOUN)){CHECKNOUN}  // will take error
     // other types test true, which is how i is set
     if(!(SPARSE&AT(tt)))break;
     BZ(tt=denseit(tt)); if(AN(tt)==0)break;  // convert sparse to dense - this could make the length go to 0, in which case true
    }
   }
   }
   t=0;  // Indicate no T block, now that we have processed it
   if((UI)i>=(UI)n||((((cwtype=(ci=i+cw)->type)&31)^CBBLOCK)+jt->cxspecials))break;  // avoid indirect-branch overhead on the likely case
   // fall through if continuing to BBLOCK (normal)
  case CBBLOCK: case CBBLOCKEND:
dobblock:
   // B-block (present on every sentence in the B-block)
   // run the sentence
   tpop(old); parseline(z);
   // if there is no error, or ?? debug mode, step to next line
   if(likely(z!=0)){bi=i; i+=(cwtype>>5)+1;  // go to next sentence, or to the one after that if it's harmless end. 
    if((UI)i<(UI)n&&!((((cwtype=(ci=i+cw)->type)&31)^CBBLOCK)+jt->cxspecials))goto dobblock;  // avoid indirect-branch overhead on the likely case
    // BBLOCK is usually followed by another BBLOCK, but another important followon is END followed by BBLOCK.  BBLOCKEND means
    // 'bblock followed by end that falls through', i. e. a bblock whose successor is i+2.  By handling that we process all sequences of if. T do. B end. B... without having to go through the switch;
    // this means the switch will learn to go to the if.
   }else if((gsfctdl&16)&&jt->uflags.us.cx.cx_c.db&(DB1)){  // error in debug mode
    z=mtm,bi=i,i=debugnewi(i+1,thisframe,self);   // Remember the line w/error; fetch continuation line if any. it is OK to have jerr set if we are in debug mode, but z must be a harmless value to avoid error protecting it
   // if the error is THROW, and there is a catcht. block, go there, otherwise pass the THROW up the line
   }else if(EVTHROW==jt->jerr){
    if(gsfctdl&4&&(tdv+tdi-1)->t){i=(tdv+tdi-1)->t+1; RESETERR; z=mtm;}else BASSERT(0,EVTHROW);  // z might not be protected if we hit error
   // for other error, go to the error location; if that's out of range, keep the error; if not,
   // it must be a try. block, so clear the error.  Pop the try. stack, and if it pops back to 0, restore debug mode (since we no longer have a try.)
   // NOTE ERROR: if we are in a for. or select., going to the catch. will leave the stack corrupted,
   // with the for./select. structures hanging on.  Solution would be to save the for/select stackpointer in the
   // try. stack, so that when we go to the catch. we can cut the for/select stack back to where it
   // was when the try. was encountered
   }else{i=ci->go; if(i<SMAX){RESETERR; z=mtm; if(gsfctdl&4){if(!--tdi){jt->uflags.us.cx.cx_c.db=(UC)(gsfctdl>>8); gsfctdl^=4;}}}  // z might not have been protected: keep it safe. This is B1 try. error catch. return. end.
   }
   break;

  // The rest of the cases are accessed only by indirect branch or fixed fallthrough
  case CTRY:
   // try.  create a try-stack entry, step to next line
   BASSERT(tdi<NTD,EVLIMIT);
   tryinit(tdv+tdi,i,cw);
   // turn off debugging UNLESS there is a catchd; then turn on only if user set debug mode
   // if debugging is already off, it stays off
   if(jt->uflags.us.cx.cx_c.db)jt->uflags.us.cx.cx_c.db=(gsfctdl&16)&&(UC)(tdv+tdi)->d?jt->dbuser:0;
   ++tdi; ++i; gsfctdl|=4;  // bump tdi pointer, set flag
   break;
  case CCATCH: case CCATCHD: case CCATCHT:
   // catch.  pop the try-stack, go to end., reset debug state.  There should always be a try. stack here
   if(gsfctdl&4){if(!--tdi){jt->uflags.us.cx.cx_c.db=(UC)(gsfctdl>>8); gsfctdl^=4;} i=1+(tdv+tdi)->e;}else i=ci->go; break;
  case CTHROW:
   // throw.  Create a faux error
   BASSERT(0,EVTHROW);
  case CFOR:
  case CSELECT: case CSELECTN:
   // for./select. push the stack.  If the stack has not been allocated, start with 9 entries.  After that,
   // if it fills up, double it as required
   if(!r)
    if(gsfctdl&8){I m=AN(cd)/WCD; BZ(cd=ext(1,cd)); cv=(CDATA*)AV(cd)+m-1; r=AN(cd)/WCD-m;}
    else  {r=9; GAT0E(cd,INT,9*WCD,1,i=-1; z=0; continue); ras(cd); cv=(CDATA*)AV(cd)-1; gsfctdl|=8;}   // 9=r

   ++cv; --r; 
   // indicate no t result (test value for select., iteration array for for.) and clear iteration index
   // remember the line number of the for./select.
   cv->t=cv->x=0; cv->line=line[ci->i]; cv->w=ci->type; ++i;
   break;
  case CDOF:   // do. after for.
   // do. after for. .  If this is first time, initialize the iterator
   if(!cv->t){
    BASSERT(t!=0,EVCTRL);   // Error if no sentences in T-block
    CHECKNOUN    // if t is not a noun, signal error on the last line executed in the T block
    BZ(forinit(cv,t)); t=0;
   }
   ++cv->j;  // step to first (or next) iteration
   if(cv->x){A x;  // assign xyz and xyz_index for for_xyz.
    if(!(ci->canend&2))BZ(z=rat(z));   // if z might be the result, protect it over the possible frees during this assignment
    symbisdel(nfs(6+cv->k,cv->xv),x=sc(cv->j),  locsym);  // Assign iteration number.  since there is no sentence, take deletion off nvr stack
    symbisdel(nfs(  cv->k,cv->iv),cv->j<cv->n?from(x,cv->t):mtv,locsym);
   }
   if(cv->j<cv->n){  // if there are more iterations to do...
    ++i; continue;   // advance to next line and process it
   }
   // if there are no more iterations, fall through...
  case CENDSEL:
   // end. for select., and do. for for. after the last iteration, must pop the stack - just once
   // Must rat() if the current result might be final result, in case it includes the variables we will delete in unstack
   // (this includes ONLY xyz_index, so perhaps we should avoid rat if stack empty or xyz_index not used)
   if(!(ci->canend&2))BZ(z=rat(z)); unstackcv(cv); --cv; ++r; 
   i=ci->go;    // continue at new location
   break;
  case CBREAKS:
  case CCONTS:
   // break./continue-in-while. must pop the stack if there is a select. nested in the loop.  These are
   // any number of SELECTN, up to the SELECT 
   if(!(ci->canend&2))BZ(z=rat(z));   // protect possible result from pop, if it might be the final result
   do{I fin=cv->w==CSELECT; unstackcv(cv); --cv; ++r; if(fin)break;}while(1);
    // fall through to...
  case CBREAK:
  case CCONT:  // break./continue. in while., outside of select.
   i=ci->go;   // After popping any select. off the stack, continue at new address
   // It must also pop the try. stack, if the destination is outside the try.-end. range
   if(gsfctdl&4){tdi=trypopgoto(tdv,tdi,i); gsfctdl^=tdi?0:4;}
   break;
  case CBREAKF:
   // break. in a for. must first pop any active select., and then pop the for.
   // We just pop till we have popped a non-select.
   // Must rat() if the current result might be final result, in case it includes the variables we will delete in unstack
   if(!(ci->canend&2))BZ(z=rat(z));   // protect possible result from pop
   do{I fin=cv->w!=CSELECT&&cv->w!=CSELECTN; unstackcv(cv); --cv; ++r; if(fin)break;}while(1);
   i=ci->go;     // continue at new location
   // It must also pop the try. stack, if the destination is outside the try.-end. range
   if(gsfctdl&4){tdi=trypopgoto(tdv,tdi,i); gsfctdl^=tdi?0:4;}
   break;
  case CRETURN:
   // return.  Protect the result during free, pop the stack back to empty, set i (which will exit)
   i=ci->go;   // If there is a try stack, restore to initial debug state.  Probably safe to  do unconditionally
   if(gsfctdl&4)jt->uflags.us.cx.cx_c.db=(UC)(gsfctdl>>8);  // if we had an unfinished try. struct, restore original debug state
   break;
  case CCASE:
  case CFCASE:
   // case. and fcase. are used to start a selection.  t has the result of the T block; we check to
   // make sure this is a noun, and save it on the stack in cv->t.  Then clear t
   if(!cv->t){
    // This is the first case.  That means the t block has the select. value.  Save it.
    BASSERT(t!=0,EVCTRL);  // error if select. case.
    CHECKNOUN    // if t is not a noun, signal error on the last line executed in the T block
    BZ(ras(t)); cv->t=t; t=0;  // protect t from free while we are comparing with it, save in stack
   }
   i=ci->go;  // Go to next sentence, which might be in the default case (if T block is empty)
   if((UI)i<(UI)n&&!(((cwtype=(ci=i+cw)->type)^CTBLOCK)+jt->cxspecials))goto tblockcase;  // avoid indirect-branch overhead on the likely case, which is case. t-block do.
   break;  // if it's not a t-block, take the indirect branch
  case CDOSEL:   // do. after case. or fcase.
   // do. for case./fcase. evaluates the condition.  t is the result (a T block); if it is nonexistent
   // or not all 0, we advance to the next sentence (in the case); otherwise skip to next test/end
   ++i;  // go to NSI if case tests true
   if(t){    // if t is not a noun, signal error on the last line executed in the T block
    CHECKNOUN
    if(!((AT(t)|AT(cv->t))&BOX)){
     // if neither t nor cv is boxed, just compare for equality.  Boxed empty goes through the other path
     if(!equ(t,cv->t))i=ci->go;  // should perhaps take the scalar case specially & send it through singleton code
    }else{
     if(all0(eps(boxopen(cv->t),boxopen(t))))i=ci->go;  // if case tests false, jump around bblock   test is cv +./@:,@:e. boxopen t
    }
    // Clear t to ensure that the next case./fcase. does not reuse this value
    t=0;
   }
   if((UI)i<(UI)n&&!((((cwtype=(ci=i+cw)->type)&31)^CBBLOCK)+jt->cxspecials))goto dobblock;  // avoid indirect-branch overhead on the likely  case. ... do. bblock
   break;
  default:   //   CELSE CWHILST CGOTO CEND
   if(unlikely(2<=*jt->adbreakr)) { BASSERT(0,EVBREAK);} 
     // this is JBREAK0, but we have to finish the loop.  This is double-ATTN, and bypasses the TRY block
   i=ci->go;  // Go to the next sentence, whatever it is
  }
 }  // end of main loop

 // We still must not take an error exit in this runout.  We have to hang around to the end to restore symbol tables, pointers, etc.

 FDEPDEC(1);  // OK to ASSERT now
 //  z may be 0 here and may become 0 before we exit
 if(z){
  // There was a result (normal case)
  // If we are executing a verb (whether or not it started with 3 : or [12] :), make sure the result is a noun.
  // If it isn't, generate a post-execution error for the non-noun
  if(AT(z)&NOUN){
   // If we are returning a virtual block, we are going to have to realize it.  This is because it might be (indeed, probably is) backed by a local symbol that
   // is going to be summarily freed by the symfreeha() below.  We could modify symfreeha to recognize when we are freeing z, but the case is not common enough
   // to be worth the trouble
   realizeifvirtual(z);
  }else if(AT(self)&ADV+CONJ){  // non-noun result, but OK from adv/conj
   // if we are returning a non-noun, we have to cleanse it of any implicit locatives that refer to the symbol table in use now.
   // It is OK to refer to other symbol tables, since they will be removed if they try to escape at higher lavels and in the meantime can be executed; but
   // there is no way we could have a reference to such an implied locative unless we also had a reference to the current table; so we replace only the
   // first locative in eqch branch
   z=fix(z,sc(FIXALOCSONLY|FIXALOCSONLYLOWEST));
  }else {pee(line,&cw[bi],EVNONNOUN,gsfctdl<<(BW-2),callframe); z=0;}  // signal error, set z to 'no result'
 }else{
  // No result.  Must be an error
  if(gsfctdl&4)jt->uflags.us.cx.cx_c.db=(UC)(gsfctdl>>8);  // if we had an unfinished try. struct, restore original debug state
  // Since we initialized z to i. 0 0, there's nothing more to do
 }

 if((gsfctdl&16)){debz();}   // pair with the deba if we did one
 A prevlocsyms=(A)AM(locsym);  // get symbol table to return to, before we free the old one
 if(!(gsfctdl&8)){
  // Normal path.  protect the result block and free everything allocated here, possibly including jt->locsyms if it was cloned (it is on the stack now)
  z=EPILOGNORET(z);  // protect return value from being freed when the symbol table is.  Must also be before stack cleanup, in case the return value is xyz_index or the like
 }else{
  // Unusual path with an unclosed contruct (e. g. return. from inside for. loop).  We have to free up the for. stack, but the return value might be one of the names
  // to be deleted on the for. stack, so we must protect the result before we pop the stack.  BUT, EPILOG frees all locally-allocated blocks, which might include the symbol
  // table that we need to pop from.  So we protect the symbol table during the cleanup of the result and stack.
  ra(locsym);  // protect local syms
  z=EPILOGNORET(z);  // protect return value from being freed when the symbol table is.  Must also be before stack cleanup, in case the return value is xyz_index or the like
  CDATA *cvminus1 = (CDATA*)VAV(cd)-1; while(cv!=cvminus1){unstackcv(cv); --cv;}  // clean up any remnants left on the for/select stack
  fa(cd);  // have to delete explicitly, because we had to ext() the block and thus protect it with ra()
  fa(locsym);  // unprotect local syms.  This deletes them if they were cloned
 }
 // locsym may have been freed now

 // If we are using the original local symbol table, clear it (free all values, free non-permanent names) for next use.  We know it hasn't been freed yet
 // We detect original symbol table by rank LSYMINUSE - other symbol tables are assigned rank 0.
 // Tables are born with NAMEADDED off.  It gets set when a name is added.  Setting back to initial state here, we clear NAMEADDED
 if(gsfctdl&32){AR(locsym)=LLOCALTABLE; symfreeha(locsym);}
 // Pop the private-area stack; set no assignment (to call for result display)
 SYMSETLOCAL(prevlocsyms);
 jt->asgn=0;
 // Now that we have deleted all the local symbols, we can see if we were returning one.
 // See if EPILOG pushed a pointer to the block we are returning.  If it did, and the usecount we are returning is 1, set this
 // result as inplaceable and install the address of the tpop stack into AM (as is required for all inplaceable blocks).  The the usecount is inplaceable 1,
 // we don't do this, because it is possible that the AM slot was inherited from higher up the stack.
 // Note that we know we are not returning a virtual block here, so it is OK to write to AM

 if(likely(z!=0))if((_ttop!=jt->tnextpushp)==AC(z)){AC(z)=ACINPLACE|ACUC1; ABACK(z)=(A)_ttop;}  // AC can't be 0
 RETF(z);
}

// execution of u : v, selecting the version of self to use based on  valence
static DF1(xv1){A z; R df1(z,  w,FAV(self)->fgh[0]);}
static DF2(xv2){A z; R df2(z,a,w,FAV(self)->fgh[1]);}

static DF1(xn1 ){R xdefn(0L,w, self);}  // Transfer monadic xdef to the common code - inplaceable
static DF1(xadv){R xdefn(w, 0L,self);}  // inplaceable

// Nilad.  See if an anonymous verb needs to be named.  If so, result is the name, otherwise 0
static F1(jtxopcall){R jt->uflags.us.cx.cx_c.db&&DCCALL==jt->sitop->dctype?jt->sitop->dca:0;}


// This handles adverbs/conjs that refer to x/y.  Install a[/w] into the derived verb as f/h, and copy the flags
// point g in the derived verb to the original self
// If we have to add a name for debugging purposes, do so
// Flag the operator with VOPR, and remove VFIX for it so that the compound can be fixed
DF2(jtxop2){A ff,x;
 RZ(ff=fdef(0,CCOLON,VERB, xn1,jtxdefn, a,self,w,  (VXOP|VFIX|VJTFLGOK1|VJTFLGOK2)^FAV(self)->flag, RMAX,RMAX,RMAX));
 R (x=xopcall(0))?namerefop(x,ff):ff;
}
static DF1(xop1){
 R xop2(w,0,self);
}


// obsolete // h is the compiled form of an explicit function: an array of 2*HN boxes.
// obsolete // Boxes 0&HN contain the enqueued words  for the sentences, jammed together
// obsolete // Return code indicating what we found:
// w is a box containing enqueued words for the sentences of a definition, jammed together
// 8: nv found 4: mu found 2: x found 1: y found.  u./v. count as u./v.
static I jtxop(J jt,A w){I i,k;
 // init flags to 'not found'
 I fndflag=0;
// obsolete  // Loop through monad and dyad
// obsolete  A *wv=AAV(w); 
// obsolete  for(k=0;k<=HN+0;k+=HN){    // for monad and dyad cases...
// obsolete   A w=wv[k];  // w is now the box containing the words of the expdef
 A *wv=AAV(w);
   
 I in=AN(w);
 // Loop over each word, starting at beginning where the references are more likely
 for(i=0;i<in;++i) {
  A w=wv[i];  // w is box containing a queue value.  If it's a name, we inspect it
  if(AT(w)&NAME){
   // Get length/string pointer
   I n=AN(w); C *s=NAV(w)->s;
   if(n){
    // Set flags if this is a special name, or an indirect locative referring to a special name in the last position, or u./v.
    if(n==1||(n>=3&&s[n-3]=='_'&&s[n-2]=='_')){
     if(s[n-1]=='n'||s[n-1]=='v')fndflag|=8;
     if(s[n-1]=='m'||s[n-1]=='u')fndflag|=4;
     else if(s[n-1]=='x')fndflag|=2;
     else if(s[n-1]=='y')fndflag|=1;
    }   // 'one-character name'
   }  // 'name is not empty'
  } // 'is name'
  if(AT(w)&VERB){
    if((FAV(w)->id&-2)==CUDOT)fndflag|=4;  // u./v.
  }
  // exit if we have seen enough: mnuv plus x.  No need to wait for y.  If we have seen only y, keep looking for x
  if(fndflag>=8+4+2)R fndflag;
 }  // loop for each word
 R fndflag;  // return what we found
}

static A jtcolon0(J jt, I deftype){A l,z;C*p,*q,*s;I m,n;
 n=0; RZ(z=exta(LIT,1L,1L,300L)); s=CAV(z);
 while(1){
  RE(l=jgets("\001"));   // abort if error on input
  if(!l)break;  // exit loop if EOF
  if(deftype!=0)RZ(l=ddtokens(l,8+2));  // if non-noun def, handle DDs, for explicit def, return string, allow jgets().  Leave noun contents untouched
  m=AN(l); p=q=CAV(l); 
  while(p<q+m&&' '==*p)++p; if(p<q+m&&')'==*p){while(p<q+m&&' '==*++p); if(p>=m+q)break;}  // if ) with nothing else but blanks, stop
  while(AN(z)<=n+m){RZ(z=ext(0,z)); s=CAV(z);}
  MC(s+n,q,m); n+=m; s[n]=CLF; ++n;  // append LF at end of each line
 }
 R str(n,s);
}    /* enter nl terminated lines; ) on a line by itself to exit */

// w is character array or list
// if table, take , w ,. LF    if list take ,&LF^:(LF~:{:) w)
static F1(jtlineit){
 R 1<AR(w)?ravel(stitch(w,scc(CLF))):AN(w)&&CLF==cl(w)?w:over(w,scc(CLF));
}

// Convert ASCII w to boxed lines.  Create separate lists of boxes for monad and dyad
// if preparsed it set, we know the lines have gone through wordil already & it is OK
// to do it again.  This means we are processing 9 :  n
static B jtsent12c(J jt,A w,A*m,A*d,I preparsed){C*p,*q,*r,*s,*x;A z;
 ASSERT(!AN(w)||LIT&AT(w),EVDOMAIN);
 ASSERT(2>=AR(w),EVRANK);
 if(preparsed){
  // Handling 9 : n lines.  Lines are separated by DDSEP, and there may be DDSEP embedded in strings.  Convert the whole thing to words, which will
  // leave the embedded DDSEP embedded; then split on the individual DDSEP tokens
  // The line comes in without trailing DDSEP.  Add it
  RZ(w=over(w,scc(DDSEP)));  // force trailing separator
  // tokenize the lines.  Each LF is its own token.
  A wil; RZ(wil=wordil(w)); makewritable(wil); I wiln=AS(wil)[0]; I (*wilv)[2]=voidAV(wil); // line index, and number of lines; array of (start,end+1) for each line
  // Compact the word-list to a line-list.  Go through the words looking for LF.  For each line, add an entry to the line-list with all the characters except the LF
  I dyad1=-1; I i=0;  // start of dyad, input pointer through wilv
  I currlinest=0;  // character# at which current line (ending in next LF) starts
  I linex=0;  // index of next output (start,end) to fill
  C *wv=CAV(w);  // the character list backing the words
  while(i<wiln){
   // we have just finished a line.  currlinest is the first character position of the next line
   // if we are still looking for end-of-monad, see if this line starts with : folllowed by DDSEP
   if(dyad1<0 && wv[wilv[i][0]]==':' && wilv[i][1]-wilv[i][0]==1 && i+1<wiln && wv[wilv[i+1][0]]==DDSEP){
    // we found : on a line by itself.  Remember where, and skip over the line
    dyad1=linex;  // save # lines in monad
    currlinest=wilv[i+1][1];  // skip past the LF
    i+=2;  // quietly eat the : and LF
   }else{
    // a real line.  scan to find next DDSEP.  There must be one.
    while(wv[wilv[i][0]]!=DDSEP)++i;  // advance to LF
    // add the new line - everything except the LF - to the line list
    wilv[linex][0]=currlinest; wilv[linex][1]=wilv[i][0];  // add the line
    ++linex; currlinest=wilv[i][0]+1;  // advance line pointer, skip over the LF to start of next line
    ++i;  // skip over the LF we processed
   }
  }
  // Now we have compacted all the lines.  Box them: monad first, then dyad
  // We fiddle with wil to make it look first like the monad, then the dyad
  dyad1=dyad1<0?linex:dyad1;  // now 0..dyad1 are the monad, dyad1..linex are the dyad
  AS(wil)[0]=dyad1;  // set length
  *m=jtboxcut0(jt,wil,w,ds(CWORDS));
  AS(wil)[0]=linex-dyad1; AK(wil)+=dyad1<<(LGSZI+1);  // advance to dyad, set its length
  *d=jtboxcut0(jt,wil,w,ds(CWORDS));
 }else{
  // the lines have not been processed before - they might not contain valid words.  Search for
  // the ':' line, and cut the remaining lines on trailing LFs
  RZ(w=lineit(w));  // make lines LF-terminated
  x=p=r=CAV(w);  /* p: monad start; r: dyad start */
  q=s=p+AN(w);   /* q: monad end;   s: dyad end   */
  while(x<s){
   q=x;
   while(' '==*x)++x; if(':'==*x){while(' '==*++x); if(CLF==*x){r=++x; break;}}
   while(CLF!=*x++);
  }
  if(x==s)q=r=s;
  A zc=cut(ds(CBOX),num(-2));  // create function for <;._2
  *m=df1(z,str(q-p,p),zc);
  *d=df1(z,str(s-r,r),zc);
 }
 R *m&&*d;
}    /* literal fret-terminated or matrix sentences into monad/dyad */

static B jtsent12b(J jt,A w,A*m,A*d){A t,*wv,y,*yv;I j,*v;
 ASSERT(1>=AR(w),EVRANK);
 wv=AAV(w); 
 GATV(y,BOX,AN(w),AR(w),AS(w)); yv=AAV(y);
 DO(AN(w), RZ(yv[i]=vs(wv[i])););
 RZ(t=indexof(y,link(chrcolon,str(1L,":")))); v=AV(t); j=MIN(*v,*(1+v));
 *m=take(sc(j  ),y); 
 *d=drop(sc(j+1),y);
 R 1;
}    /* boxed sentences into monad/dyad */

// Install bucket info into the NAME type t, if it is a local name
// actstv points to the chain headers, actstn is the number of chains
static void jtcalclocalbuckets(J jt, A t, LX *actstv, I actstn){LX k;
 if(!(NAV(t)->flag&(NMLOC|NMILOC))){  // don't store if we KNOW we won't be looking up in the local symbol table
  I4 compcount=0;  // number of comparisons before match
  // lv[j] is a simplename.  We will install the bucket/index fields
  NM *tn = NAV(t);  // point to the NM part of the name block
  // Get the bucket number by reproducing the calculation in the symbol-table routine
  tn->bucket=(I4)SYMHASH(tn->hash,actstn);  // bucket number of name hash
  // search through the chain, looking for a match on name.  If we get a match, the bucket index is the one's complement
  // of the number of items compared before the match.  If we get no match, the bucket index is the number
  // of items compared (= the number of items in the chain)
  for(k=actstv[tn->bucket];k;++compcount,k=(jt->sympv)[k].next){  // k chases the chain of symbols in selected bucket
   if(tn->m==NAV((jt->sympv)[k].name)->m&&!memcmpne(tn->s,NAV((jt->sympv)[k].name)->s,tn->m)){compcount=~compcount; break;}
  }
  tn->bucketx=compcount;
 }
}

EVERYFS(onmself,jtonm,0,0,VFLAGNONE)  // create self to pass into every

// create local-symbol table for a definition
//
// The goal is to save time allocating/deallocating the symbol table for a verb; and to
// save time in name lookups.  We scan the tokens, looking for assignments, and extract
// all names that are the target of local assignment (including fixed multiple assignments).
// We then allocate a symbol table sufficient to hold these values, and a symbol for each
// one.  These symbols are marked LPERMANENT.  When an LPERMANENT symbol is deleted, its value is
// cleared but the symbol remains.
// Then, all simplenames in the definition, whether assigned or not, are converted to bucket/index
// form, using the known size of the symbol table.  If the name was an assigned name, its index
// is given; otherwise the index tells how many names to skip before starting the name search.
//
// This symbol table is created with no values.  When the explicit definition is started, it
// is used; values are filled in as they are defined & removed at the end of execution (but the symbol-table
// entries for them are not removed).  If
// a definition is recursive, it will create a new symbol table, starting it off with the
// permanent entries from this one (with no values).  We create this table with rank 0, and we set
// the rank to 1 while it is in use, to signify that it must be cloned rather than used inplace.

// l is the A block for all the words/queues used in the definition
// c is the table of control-word info used in the definition
// type is the m operand to m : n, indicating part of speech to be produce
// dyad is 1 if this is the dyadic definition
// flags is the flag field for the verb we are creating; indicates whether uvmn are to be defined
//
// We save the symbol chain numbers for y/x in the AM field of the SYMB block
A jtcrelocalsyms(J jt, A l, A c,I type, I dyad, I flags){A actst,*lv,pfst,t,wds;C *s;I j,ln;
 // Allocate a pro-forma symbol table to hash the names into
 RZ(pfst=stcreate(2,40,0L,0L));
 // Do a probe-for-assignment for every name that is locally assigned in this definition.  This will
 // create a symbol-table entry for each such name
 // Start with the argument names.  We always assign y, and x EXCEPT when there is a monadic guaranteed-verb
 RZ(probeis(mnuvxynam[5],pfst));if(!(!dyad&&(type>=3||(flags&VXOPR)))){RZ(probeis(mnuvxynam[4],pfst));}
 if(type<3){RZ(probeis(mnuvxynam[2],pfst)); RZ(probeis(mnuvxynam[0],pfst));}
 if(type==2){RZ(probeis(mnuvxynam[3],pfst)); RZ(probeis(mnuvxynam[1],pfst));}
 // Go through the definition, looking for local assignment.  If the previous token is a simplename, add it
 // to the table.  If it is a literal constant, break it into words, convert each to a name, and process.
 ln=AN(l); lv=AAV(l);  // Get # words, address of first box
 for(j=1;j<ln;++j) {   // start at 1 because we look at previous word
  t=lv[j-1];  // t is the previous word
  // look for 'names' =./=: .  If found (and the names do not begin with `), replace the string with a special form: a list of boxes where each box contains a name.
  // This form can appear only in compiled definitions
  if(AT(lv[j])&ASGN&&AT(t)&LIT&&AN(t)&&CAV(t)[0]!=CGRAVE){
   A neww=words(t);
   if(AN(neww)){  // ignore blank string
    A newt=every(neww,(A)&onmself);  // convert every word to a NAME block
    if(newt){t=lv[j-1]=newt; AT(t)|=BOXMULTIASSIGN;}else RESETERR  // if no error, mark the block as MULTIASSIGN type and save it in the compiled definition; also set as t for below.  If error, catch it later
   }
  }

  if((AT(lv[j])&ASGN+ASGNLOCAL)==(ASGN+ASGNLOCAL)) {  // local assignment
   if(AT(lv[j])&ASGNTONAME){    // preceded by name?
    // Lookup the name, which will create the symbol-table entry for it
    RZ(probeis(t,pfst));
   } else if(AT(t)&LIT) {
    // LIT followed by =.  Probe each word.  Now that we support lists of NAMEs, this is used only for AR assignments
    // First, convert string to words
    s=CAV(t);   // s->1st character; remember if it is `
    if(wds=words(s[0]==CGRAVE?str(AN(t)-1,1+s):t)){I kk;  // convert to words (discarding leading ` if present)
     I wdsn=AN(wds); A *wdsv = AAV(wds), wnm;
     for(kk=0;kk<wdsn;++kk) {
      // Convert word to NAME; if local name, add to symbol table
      if((wnm=onm(wdsv[kk]))) {
       if(!(NAV(wnm)->flag&(NMLOC|NMILOC)))RZ(probeis(wnm,pfst));
      } else RESETERR
     }
    } else RESETERR  // if invalid words, ignore - we don't catch it here
   }else if((AT(t)&BOX+BOXMULTIASSIGN)==BOX+BOXMULTIASSIGN){  // not NAME, not LIT; is it NAMEs box?
    // the special form created above.  Add each non-global name to the symbol table
    A *tv=AAV(t); DO(AN(t), if(!(NAV(tv[i])->flag&(NMLOC|NMILOC)))RZ(probeis(tv[i],pfst));)
   }
  } // end 'local assignment'
 }  // for each word in sentence

 // Go through the control-word table, looking for for_xyz.  Add xyz and xyz_index to the local table too.
 I cn=AN(c); CW *cwv=(CW*)AV(c);  // Get # control words, address of first
 for(j=0;j<cn;++j) {   // look at each control word
  if(cwv[j].type==CFOR){  // for.
   I cwlen = AN(lv[cwv[j].i]);
   if(cwlen>4){  // for_xyz.
    // for_xyz. found.  Lookup xyz and xyz_index
    A xyzname = str(cwlen+1,CAV(lv[cwv[j].i])+4);
    RZ(probeis(nfs(cwlen-5,CAV(xyzname)),pfst));  // create xyz
    MC(CAV(xyzname)+cwlen-5,"_index",6L);    // append _index to name
    RZ(probeis(nfs(cwlen+1,CAV(xyzname)),pfst));  // create xyz_index
   }
  }
 }

 // Count the assigned names, and allocate a symbol table of the right size to hold them.  We won't worry too much about collisions, since we will be assigning indexes in the definition.
 // We choose the smallest feasible table to reduce the expense of clearing it at the end of executing the verb
 I pfstn=AN(pfst); LX*pfstv=LXAV0(pfst),pfx; I asgct=0;
 for(j=SYMLINFOSIZE;j<pfstn;++j){  // for each hashchain
  for(pfx=pfstv[j];pfx;pfx=(jt->sympv)[pfx].next)++asgct;  // chase the chain and count
 }

 asgct = asgct + (asgct>>1); // leave 33% empty space, since we will have resolved most names here
 RZ(actst=stcreate(2,asgct,0L,0L));  // Allocate the symbol table we will use
 *(UI4*)LXAV0(actst)=(UI4)((SYMHASH(NAV(mnuvxynam[4])->hash,AN(actst)-SYMLINFOSIZE)<<16)+SYMHASH(NAV(mnuvxynam[5])->hash,AN(actst)-SYMLINFOSIZE));  // get the yx bucket indexes for a table of this size, save in first hashchain

 // Transfer the symbols from the pro-forma table to the result table, hashing using the table size
 // For fast argument assignment, we insist that the arguments be the first symbols added to the table.
 // So we add them by hand - just y and possibly x.
 RZ(probeis(mnuvxynam[5],actst));if(!(!dyad&&(type>=3||(flags&VXOPR)))){RZ(probeis(mnuvxynam[4],actst));}
 for(j=1;j<pfstn;++j){  // for each hashchain
  for(pfx=pfstv[j];pfx;pfx=(jt->sympv)[pfx].next){L *newsym;
   A nm=(jt->sympv)[pfx].name;
   RZ(newsym=probeis(nm,actst));  // create new symbol (or possibly overwrite old argument name)
   newsym->flag = (jt->sympv)[pfx].flag|LPERMANENT;   // Mark as permanent
  }
 }
 I actstn=AN(actst)-SYMLINFOSIZE; LX*actstv=LXAV0(actst);  // # hashchains in new symbol table, and pointer to hashchain table

 // Go back through the words of the definition, and add bucket/index information for each simplename
 // Note that variable names must be replaced by clones so they are not overwritten
 // Don't do this if this definition might return a non-noun (i. e. is an adverb/conjunction not operating on xy)
 // In that case, the returned result might contain local names; but these names contain bucket information
 // and are valid only in conjunction with the symbol table for this definition.  To prevent the escape of
 // incorrect bucket information, don't have any (this is easier than trying to remove it from the returned
 // result).  The definition will still benefit from the preallocation of the symbol table.
 if(type>=3 || flags&VXOPR){  // If this is guaranteed to return a noun...
  for(j=0;j<ln;++j) {
   if(AT(t=lv[j])&NAME) {
    jtcalclocalbuckets(jt,t,actstv,actstn);  // install bucket info into name
   }else if((AT(t)&BOX+BOXMULTIASSIGN)==BOX+BOXMULTIASSIGN){
    A *tv=AAV(t); DO(AN(t), jtcalclocalbuckets(jt,tv[i],actstv,actstn);)  // install bucket info into boxed names
   }
  }
 }  // 'noun result guaranteed'
 R actst;
}

// a is a local symbol table, possibly in use
// result is a copy of it, ready to use.  All PERMANENT symbols are copied over and given empty values
// The rank-flag of the table is 'not modified'
// static A jtclonelocalsyms(J jt, A a){A z;I j;I an=AN(a); I *av=AV(a);I *zv;
A jtclonelocalsyms(J jt, A a){A z;I j;I an=AN(a); LX *av=LXAV0(a),*zv;
 RZ(z=stcreate(2,AN(a),0L,0L)); zv=LXAV0(z);  // allocate the clone; zv->clone hashchains
 // Copy the first hashchain, which has the x/v hashes
 zv[0]=av[0]; // Copy as Lx; really it's a UI4
 // Go through each hashchain of the model, after the first one
 for(j=SYMLINFOSIZE;j<an;++j) {LX *zhbase=&zv[j]; LX ahx=av[j]; LX ztx=0; // hbase->chain base, hx=index of current element, tx is element to insert after
  while(ahx&&(jt->sympv)[ahx].flag&LPERMANENT) {L *l;  // for each permanent entry...
   RZ(l=symnew(zhbase,ztx)); 
   A nm=(jt->sympv)[ahx].name;
   l->name=nm; ras(l->name);  // point symbol table to the name block, and increment its use count accordingly
   l->flag=(jt->sympv)[ahx].flag&(LINFO|LPERMANENT);  // clear all but PERMANENT and INFO, in case we try to delete the name (as in for_xyz. or 4!:55)
   ztx = ztx?(jt->sympv)[ztx].next : *zhbase;  // ztx=index to value we just added.  We avoid address calculation because of the divide.  If we added
      // at head, the added block is the new head; otherwise it's pointed to by previous tail
   ahx = (jt->sympv)[ahx].next;  // advance to next symbol
  }
 }
 R z;
}

F2(jtcolon){A d,h,*hv,m;B b;C*s;I flag=VFLAGNONE,n,p;
 RZ(a&&w);
 if(VERB&AT(a)&AT(w)){  // v : v case
  // If nested v : v, prune the tree
  if(CCOLON==FAV(a)->id&&FAV(a)->fgh[0]&&VERB&AT(FAV(a)->fgh[0])&&VERB&AT(FAV(a)->fgh[1]))a=FAV(a)->fgh[0];  // look for v : v; don't fail if fgh[0]==0 (namerefop).  Must test fgh[0] first
  if(CCOLON==FAV(w)->id&&FAV(w)->fgh[0]&&VERB&AT(FAV(w)->fgh[0])&&VERB&AT(FAV(w)->fgh[1]))w=FAV(w)->fgh[1];
  R fdef(0,CCOLON,VERB,xv1,xv2,a,w,0L,((FAV(a)->flag&FAV(w)->flag)&VASGSAFE),mr(a),lr(w),rr(w));  // derived verb is ASGSAFE if both parents are 
 }
 RE(n=i0(a));  // m : n; set n=value of a argument
 if(equ(w,num(0))){RZ(w=colon0(n)); if(!n)R w;}   // if m : 0, read up to the ) .  If 0 : 0, return the string unedited
 if((C2T+C4T)&AT(w))RZ(w=cvt(LIT,w));
 if(10<n){s=CAV(w); p=AN(w); if(p&&CLF==s[p-1])RZ(w=str(p-1,s));}
 else{  // not tacit translator - preparse the body
  RZ(BOX&AT(w)?sent12b(w,&m,&d):sent12c(w,&m,&d,n==9)); INCORP(m); INCORP(d);  // get monad & dyad parts; we are incorporating them into hv[]
  if(4==n){if((-AN(m)&(AN(d)-1))<0)d=m; m=mtv;}  //  for 4 :, make the single def given the dyadic one
  GAT0(h,BOX,2*HN,1); hv=AAV(h);
  if(n){  // if not noun, audit the valences as valid sentences
   RE(b=preparse(m,hv,hv+1)); if(b)flag|=VTRY1; hv[2   ]=jt->retcomm?m:mtv;
   RE(b=preparse(d,hv+HN,hv+HN+1)); if(b)flag|=VTRY2; hv[2+HN]=jt->retcomm?d:mtv;
  }
 }
 if(!n){ra0(w); RCA(w);}  // noun - return it.  Since this is a noun result, we make sure it is recursive usecount
 // Non-noun results cannot become inputs to verbs, so we do not force them to be recursive
 if((1LL<<n)&0x206){  // types 1, 2, 9
// obsolete   I fndflag=xop(h);   // 4=mnuv 2=x 1=y
  I fndflag=xop(hv[0])|xop(hv[0+HN]);   // 4=mnuv 2=x 1=y, combined for both valences
  // for 9 : n, figure out best type after looking at n
  if(n==9){
   I defflg=fndflag|1; CTLZI(fndflag,n); n=(0x2143>>(n<<2))&0xf; // replace 9 by value depending on what was seen
   if(n==4){hv[HN]=hv[0]; hv[0]=mtv; hv[HN+1]=hv[1]; hv[1]=mtv; hv[HN+2]=hv[2]; hv[2]=mtv; flag=(flag&~VTRY2)+VTRY1; }  // if we created a dyadic verb, shift the monad over to the dyad and clear the monad, incl try flag
  } 
  if(n<=2){  // adv or conj after autodetection
   fndflag=(fndflag&7)|((fndflag&8)>>1);  // scaf put back in old form
   b=fndflag>4;   // set if there is mnuv and xy scaf
   if(b)flag|=VXOPR;   // if this def refers to xy, set VXOPR
   ASSERT(!BETWEENC(fndflag,1,3),EVNONCE);  // scaf
// obsolete if(BETWEENC(fndflag,1,3))jfwrite(str(129,"************ Old-style definition encountered.  It will be invalid after the beta period.\nIt has x/y without u/v/m/n **********\n"),num(2));
   // if there is only one valence defined, that will be the monad.  Swap it over to the dyad in two cases: (1) it is a conjunction with uv only: the operands will be the two verbs;
   // (2) it is an operator with a reference to x
   if(((-AN(m))&(AN(d)-1)&(((fndflag-5)&(1-n))|(5-fndflag)))<0){A*u=hv,*v=hv+HN,x; DQ(HN, x=*u; *u++=*v; *v++=x;);}  // if not, it executes on uv only; if conjunction, make the default the 'dyad' by swapping monad/dyad
   // for adv/conj, b has operator status from here on
  }
 }
 flag|=VFIX;  // ensures that f. will not look inside n : n
 // Create a symbol table for the locals that are assigned in this definition.  It would be better to wait until the
 // definition is executed, so that we wouldn't take up the space for library verbs; but since we don't explicitly free
 // the components of the explicit def, we'd better do it now, so that the usecounts are all identical
 if(4>=n) {
  // Don't bother to create a symbol table for an empty definition, since it is a domain error
  if(AN(hv[1]))RZ(hv[3] = rifvs(crelocalsyms(hv[0],hv[1],n,0,flag)));  // wordss,cws,type,monad,flag
  if(AN(hv[HN+1]))RZ(hv[HN+3] = rifvs(crelocalsyms(hv[HN+0], hv[HN+1],n,1,flag)));  // words,cws,type,dyad,flag
 }

 switch(n){
  case 3:  R fdef(0,CCOLON, VERB, xn1,jtxdefn,       num(n),0L,h, flag|VJTFLGOK1|VJTFLGOK2, RMAX,RMAX,RMAX);
  case 1:  R fdef(0,CCOLON, ADV,  b?xop1:xadv,0L,    num(n),0L,h, flag, RMAX,RMAX,RMAX);
  case 2:  R fdef(0,CCOLON, CONJ, 0L,b?jtxop2:jtxdefn, num(n),0L,h, flag, RMAX,RMAX,RMAX);
  case 4:  R fdef(0,CCOLON, VERB, xn1,jtxdefn,       num(n),0L,h, flag|VJTFLGOK1|VJTFLGOK2, RMAX,RMAX,RMAX);
  case 13: R vtrans(w);
  default: ASSERT(0,EVDOMAIN);
 }
}

// input reader for direct definition
// This is a drop-in for tokens().  It takes a string and env, and returns tokens created by enqueue().  Can optionally return string
//
// Any DDs found are collected and converted into ( m : string ).  This is done recursively.  If an unterminated
// DD is found, we call jgets() to get the next line, taking as many lines as needed to leave with a valid line.
// The string for a DD will contain a trailing LF plus one LF for each LF found inside the DD.
//
// Bit 2 of env suppresses the call to jgets().  It will be set if it is known that there is no way to get more
// input, for example if the string comes from (". y) or from an event.  If an unterminated DD is found when bit 2 is set,
// the call fails with control error
//
// Bit 3 of env is set if the caller wants the returned value as a string rather than as enqueued words
//
// If the call to jgets() returns EOF, indicating end-of-script, that is also a control error
A jtddtokens(J jt,A w,I env){
// TODO: Use LF for DDSEP, support {{), make nouns work
 PROLOG(000);F1PREFIP;
 RZ(w);
#if 0  // set to 1 to disable DD
 R (env&8)?w:tokens(w,env&3);  // return the input unmodified
#endif
 // find word boundaries, remember if last word is NB
 A wil; RZ(wil=wordil(w));  // get index to words
 C *wv=CAV(w); I nw=AS(wil)[0]; I (*wilv)[2]=voidAV(wil);  // cv=pointer to chars, nw=#words including final NB   wilv->[][2] array of indexes into wv word start/end
 // scan for start of DD/end of DD.
 I firstddbgnx;  // index of first/last start of DD, and end of DD
 for(firstddbgnx=0;firstddbgnx<nw;++firstddbgnx){US ch2=*(US*)(wv+wilv[firstddbgnx][0]); ASSERT(!(ch2==DDEND&&(wilv[firstddbgnx][1]-wilv[firstddbgnx][0]==2)),EVCTRL) if(ch2==DDBGN&&(wilv[firstddbgnx][1]-wilv[firstddbgnx][0]==2))break; }
 if(firstddbgnx>=nw&&(env&8))R w;    //   If no DD chars found, and caller wants a string, return w fast
 // loop till all DDs found
 while(firstddbgnx<nw){
  // We know that firstddbgnx is DDBGN
  I ddendx=-1, ddbgnx=firstddbgnx;  // end/start of DD, indexes into wilv.  This will be the pair we process.  We advance ddbgnx and stop when we hit ddendx
  I scanstart=firstddbgnx+1;  // start looking for DDEND after the first known DDBGN.  But if there turns out to be no DDEND, advance start ptr to avoid rescanning
  while(1){I i;  // loop till we find a complete DD
   for(i=scanstart;i<nw;++i){US ch2=*(US*)(wv+wilv[i][0]); if(ch2==DDEND&&(wilv[i][1]-wilv[i][0]==2)){ddendx=i; break;} if(ch2==DDBGN&&(wilv[i][1]-wilv[i][0]==2))ddbgnx=i;}  // find DD pair
   if(ddendx>=0)break;  // we found an end, and we started at a begin, so we have a pair.  Go process it

   // Here the current line ended with no end DD.  We have to continue onto the next line
   ASSERT(!(env&4),EVCTRL);   // Abort if we are not allowed to continue (as for an event or ". y)
   scanstart=AM(wil);  // Get # words, not including final NB.  We have looked at em all, so start next look after all of them
   A neww=jgets("");  // fetch next line.
   RE(0); ASSERT(neww!=0,EVCTRL); // fail if jgets failed, or if it returned EOF - problem either way
   // join the new line onto the end of the old one (after discarding trailing NB in the old).  Must add an LF character and a word for it
   w=jtapip(jtinplace,w,scc(DDSEP));   // append a separator, which is all that remains of the original line   scaf use faux or constant block
   jtinplace=(J)((I)jtinplace|JTINPLACEW);  // after the first one, we can certainly inplace on top of w
   I oldchn=AN(w);  // len after adding LF, before adding new line
   RZ(w=jtapip(jtinplace,w,neww));   // join the new character list onto the old, inplace if possible
   // Remove the trailing comment if any from wil, and add a word for the added LF
   makewritable(wil);  // We will modify the block rather than curtailing it
   AS(wil)[0]=scanstart; AN(wil)=2*scanstart;  // set AS and AN to discard comment
   A lfwd; fauxblockINT(lffaux,2,2); fauxINT(lfwd,lffaux,2,2)  AS(lfwd)[0]=2; AS(lfwd)[1]=1; IAV(lfwd)[0]=oldchn-1; IAV(lfwd)[1]=oldchn; 
   RZ(wil=jtapip(jtinplace,wil,lfwd));  // add a new word for added LF.  # words in wil is now scanstart+1
   ++scanstart;  // update count of words already examined so we start after the added LF word
   A newwil; RZ(newwil=wordil(neww));  // get index to new words
   I savam=AM(newwil);  // save AM for insertion if final new string block
   makewritable(newwil);  // We will modify the block rather than adding to it
   I *nwv=IAV(newwil); DO(AN(newwil), *nwv++ += oldchn;)  // relocate all the new words so that they point to chars after the added LF
   RZ(wil=jtapip(jtinplace,wil,newwil));   // add new words after LF, giving old LF new
   AM(wil)=savam+scanstart;  // set total # words in combined list, not including final comment
   wv=CAV(w); nw=AS(wil)[0]; wilv=voidAV(wil);  // refresh pointer to word indexes, and length
  }

  // We have found an innermost DD, running from ddbgnx to ddendx.  Convert it to ( m : 'string' ) form
  //  create a faux block for the input to enqueue, so we can insert AM
  A ddwds; fauxblockINT(ddfaux,0,3); fauxINT(ddwds,ddfaux,0,3)  AS(ddwds)[0]=ddendx-ddbgnx-1; AS(ddwds)[1]=2; AS(ddwds)[2]=1; AN(ddwds)=2*(ddendx-ddbgnx-1);
  AK(ddwds)=(C*)(wilv+ddbgnx+1)-(C*)ddwds;  // point to data after the DDBGN
// obsolete   A qwds; RZ(qwds=enqueue(ddwds,w,env&3)); I opflags=xop(qwds)|1;  // see what args are given - if no args at all, use y  scaf
// obsolete   I deftype; CTLZI(opflags,deftype); deftype=(0x2143>>(deftype<<2))&0xf;  // digit number for the definition scaf let cx do it
  A ddstg; RZ(ddstg=unwordil(ddwds,w,2+1));  // create string form: ask for enclosing quotes and 4 bytes of extra space.. Result is always writable
  // Add (m:) to the string so we can refer to it
  I ddstglen=AN(ddstg); AN(ddstg)=AS(ddstg)[0]=ddstglen+4; C* suffv=CAV(ddstg)+ddstglen; suffv[0]='('; suffv[1]='9'; suffv[2]=':'; suffv[3]=')';
  // append the new chars to w
  I ddstgbgn=AN(w); w=jtapip(jtinplace,w,ddstg);   // remember where new string starts in combined string; add on to the character list
  // replace the words of the DD with 5 word slots.  Transfer comment-at-end status to the new wordlist
  I commentdiff=AS(wil)[0]-AM(wil);  // remember comment status, 0 or 1
  RZ(wil=over(take(sc(ddbgnx+5),wil),drop(sc(ddendx+1),wil))); makewritable(wil);  // replace DD with 5 slots
  AM(wil)=AS(wil)[0]-commentdiff;  // restore comment status to new wordlist
  wv=CAV(w); nw=AS(wil)[0]; wilv=voidAV(wil);  // refresh pointer to word indexes, and length
  // install pointers to the DD: ( m : string )
  wilv[ddbgnx+3][0]=ddstgbgn; ddstglen+=ddstgbgn; wilv[ddbgnx+3][1]=ddstglen;  // word: 'string'
  wilv[ddbgnx][0]=ddstglen; wilv[ddbgnx][1]=ddstglen+1;  // word: (
  wilv[ddbgnx+1][0]=ddstglen+1; wilv[ddbgnx+1][1]=ddstglen+2;  // word: m
  wilv[ddbgnx+2][0]=ddstglen+2; wilv[ddbgnx+2][1]=ddstglen+3;  // word: :
  wilv[ddbgnx+4][0]=ddstglen+3; wilv[ddbgnx+4][1]=ddstglen+4;  // word: )

  // We have replaced one DD with its equivalent explicit definition.  Rescan the line, starting at the first location where DDBGN was seen
  for(;firstddbgnx<nw;++firstddbgnx){US ch2=*(US*)(wv+wilv[firstddbgnx][0]); ASSERT(!(ch2==DDEND&&(wilv[firstddbgnx][1]-wilv[firstddbgnx][0]==2)),EVCTRL) if(ch2==DDBGN&&(wilv[firstddbgnx][1]-wilv[firstddbgnx][0]==2))break; }
 }
 // All DDs replaced. convert word list back to either text form or enqueued form
 w=(env&8)?unwordil(wil,w,0):enqueue(wil,w,env&3);  // the word list is where everything is.  Convert as required
 EPILOG(w);
}
