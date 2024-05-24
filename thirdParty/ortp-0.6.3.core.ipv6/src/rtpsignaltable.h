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

/*DDN: Changed from 5 to 10*/
#define RTP_CALLBACK_TABLE_MAX_ENTRIES	10

typedef void (*RtpCallback)(RtpSession *, ...);

struct _RtpSignalTable
{
	RtpCallback callback[RTP_CALLBACK_TABLE_MAX_ENTRIES];
	gpointer	user_data[RTP_CALLBACK_TABLE_MAX_ENTRIES];
	RtpSession *session;
	gint count;
};

typedef struct _RtpSignalTable RtpSignalTable;

void rtp_signal_table_init(RtpSignalTable *table,RtpSession *session);

int rtp_signal_table_add(RtpSignalTable *table,RtpCallback cb, gpointer user_data);

void rtp_signal_table_emit(RtpSignalTable *table);

/* emit but with a second arg */
void rtp_signal_table_emit2(RtpSignalTable *table, gpointer arg);

int rtp_signal_table_remove_by_callback(RtpSignalTable *table,RtpCallback cb);
