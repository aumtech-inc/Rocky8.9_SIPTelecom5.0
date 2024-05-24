/*
  The oRTP library is an RTP (Realtime Transport Protocol - rfc1889) stack.
  Copyright (C) 2001  Simon MORLAT simon.morlat@linphone.org

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "rtpport.h"

#ifndef TARGET_IS_HPUXKERNEL

#include "rtp.h"
#include "str_utils.h"

void mblk_init(mblk_t *mp)
{
	mp->b_cont=mp->b_prev=mp->b_next=NULL;
	mp->b_rptr=mp->b_wptr=NULL;
}

mblk_t *allocb(int size, int pri)
{
	mblk_t *mp;
	dblk_t *datab;
	gchar *buf;
	
	mp=g_malloc(sizeof(mblk_t));
	mblk_init(mp);
	datab=g_malloc(sizeof(dblk_t));
	
	buf=g_malloc(size);

	datab->db_base=buf;
	datab->db_lim=buf+size;
	datab->ref_count=1;
	
	mp->b_datap=datab;
	mp->b_rptr=mp->b_wptr=buf;
	mp->b_next=mp->b_prev=mp->b_cont=NULL;
	return mp;
}
	
void freeb(mblk_t *mp)
{
	g_return_if_fail(mp->b_datap!=NULL);
	g_return_if_fail(mp->b_datap->db_base!=NULL);
	
	mp->b_datap->ref_count--;
	if (mp->b_datap->ref_count==0)
	{
		g_free(mp->b_datap->db_base);
		g_free(mp->b_datap);
	}
	g_free(mp);
}	

void freemsg(mblk_t *mp)
{
	mblk_t *tmp1,*tmp2;
	tmp1=mp;
	while(tmp1!=NULL)
	{
		tmp2=tmp1->b_cont;
		freeb(tmp1);
		tmp1=tmp2;
	}
}

mblk_t *dupb(mblk_t *mp)
{
	mblk_t *newm;
	g_return_val_if_fail(mp->b_datap!=NULL,NULL);
	g_return_val_if_fail(mp->b_datap->db_base!=NULL,NULL);
	
	mp->b_datap->ref_count++;
	newm=g_malloc(sizeof(mblk_t));
	mblk_init(newm);
	newm->b_datap=mp->b_datap;
	newm->b_rptr=mp->b_rptr;
	newm->b_wptr=mp->b_wptr;
	return newm;
}

/* duplicates a complex mblk_t */
mblk_t	*dupmsg(mblk_t* m)
{
	mblk_t *newm=NULL,*mp,*prev;
	prev=newm=dupb(m);
	m=m->b_cont;
	while (m!=NULL){
		mp=dupb(m);
		prev->b_cont=mp;
		prev=mp;
		m=m->b_cont;
	}
	return newm;
}

void putq(queue_t *q,mblk_t *mp)
{
	mblk_t *oldlast=q->q_last;
	g_return_if_fail(mp!=NULL);
	q->q_last=mp;
	mp->b_prev=oldlast;
	mp->b_next=NULL;
	if (oldlast!=NULL) oldlast->b_next=mp;
	else q->q_first=mp;
	q->q_mcount++;
}

mblk_t *getq(queue_t *q)
{
	mblk_t *oldfirst;
	oldfirst=q->q_first;
	if (oldfirst==NULL) return NULL;  /* empty queue */
	
	q->q_first=oldfirst->b_next;
	if (oldfirst->b_next!=NULL)
	{
		oldfirst->b_next->b_prev=NULL;
	}
	else
	{	/* it was the only element of the q */
		q->q_last=NULL;
	}
	oldfirst->b_prev=oldfirst->b_next=NULL;
	q->q_mcount--;
	return oldfirst;
}

/* insert mp in q just before emp */
void insq(queue_t *q,mblk_t *emp, mblk_t *mp)
{
	mblk_t *m;
	
	g_return_if_fail(mp!=NULL);
	q->q_mcount++;
	if (q->q_first==NULL) {
		q->q_first=q->q_last=mp;
	    return;
	}
	if (emp==NULL){ /* insert it at the bottom of the q */
		m=q->q_last;
		q->q_last=mp;
		mp->b_prev=m;
		m->b_next=mp;
	}
	else
	{
		m=emp->b_prev;
		mp->b_prev=m;
		mp->b_next=emp;
		emp->b_prev=mp;
		if (m!=NULL){
			m->b_next=mp;
		} else q->q_first=mp;
	}
}

/* remove and free all messages in the q */
void flushq(queue_t *q, int how)
{
	mblk_t *mp;
	
	while ((mp=getq(q))!=NULL)
	{
		freemsg(mp);
	}
}

gint msgdsize(mblk_t *mp)
{
	gint msgsize=0;
	while(mp!=NULL){
		msgsize+=mp->b_wptr-mp->b_rptr;
		mp=mp->b_cont;
	}
	return msgsize;
}

mblk_t * msgpullup(mblk_t *mp,int len)
{
	mblk_t *newm;
	gint msgsize=msgdsize(mp);
	gint rlen;
	gint mlen;
	
	
	if ((len==-1) || (len>msgsize)) len=msgsize;
	rlen=len;
	newm=allocb(len,BPRI_MED);

	while(mp!=NULL){
		mlen=mp->b_wptr-mp->b_rptr;
		if (rlen>=mlen)
		{
			memcpy(newm->b_wptr,mp->b_rptr,mlen);
			rlen-=mlen;
			newm->b_wptr+=mlen;
		}
		else /* rlen < mlen */
		{
			memcpy(newm->b_wptr,mp->b_rptr,rlen);
			newm->b_wptr+=rlen;
			
			/* put the end of the original message at the end of the new */
			newm->b_cont=dupmsg(mp);
			newm->b_cont->b_rptr+=rlen;
			return newm;
		}
		mp=mp->b_cont;
	}
	return newm;
}


mblk_t *copyb(mblk_t *mp)
{
	mblk_t *newm;
	gint len=mp->b_wptr-mp->b_rptr;
	newm=allocb(len,BPRI_MED);
	memcpy(newm->b_wptr,mp->b_rptr,len);
	newm->b_wptr+=len;
	return newm;
}

mblk_t *copymsg(mblk_t *mp)
{
	mblk_t *newm=0,*m;
	m=newm=copyb(mp);
	mp=mp->b_cont;
	while(mp!=NULL){
		m->b_cont=copyb(mp);
		m=m->b_cont;
		mp=mp->b_cont;
	}
	return newm;
}

#endif
