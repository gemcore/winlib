#if !defined(wassert_h)
#define wassert_h

#undef	assert

#ifdef NDEBUG

#define Assert(exp)	((void)0)

#else
void _wassert (char* szExpr, char* szFile, int line);
#define Assert(exp) (void)( (exp) || (_wassert(#exp, __FILE__, __LINE__), 0) )

#endif	/* NDEBUG */
#endif // wassert_h
