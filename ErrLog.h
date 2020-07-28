#ifndef _ErrLog_h_
#define _ErrLog_h_

#define ERRLOG(msg) ErrLog_log(ERRLOG_ID, msg)
#define ERRLOG1(msg, a1) ErrLog_log(ERRLOG_ID, msg, a1)
#define ERRLOG2(msg, a1, a2) ErrLog_log(ERRLOG_ID, msg, a1, a2)
#define ERRLOG3(msg, a1, a2, a3) ErrLog_log(ERRLOG_ID, msg, a1, a2, a3)
#define ERRLOG4(msg, a1, a2, a3, a4) ErrLog_log(ERRLOG_ID, msg, a1, a2, a3, a4)
#define ERRLOG5(msg, a1, a2, a3, a4, a5) ErrLog_log(ERRLOG_ID, msg, a1, a2, a3, a4, a5)

void ErrLog_log(const char* id, const char* msg, ...);

#endif // _ErrLog_h_
