                    /* ========================== *\
                    ** ==       M.A.R.S.       == **
                    ** == Сравнение с образцом == **
                    \* ========================== */

#include "FilesDef"

/*
 * Match name with pattern. Returns 1 on success.
 * Pattern may contain wild symbols:
 *
 * %       - any string
 * _       - any sequence of spaces
 * ?       - any symbol
 * [abc]   - symbol from the set
 */

boolean Match (char *name, short nlen, char *pat, short plen)
{
  int ok;

  for (;;) {
    if (plen == 0)
      return (nlen ? FALSE : TRUE);
    --plen;
    switch (*pat++) {
    default:
      if (! nlen || *name++ != pat[-1]) return (FALSE);
      --nlen;
      break;
    case '_':
      while (nlen && *name == ' ') ++name, --nlen;
      while (plen && *pat == '_') ++pat, --plen;
      break;
    case '?':
      if (! nlen--) return (FALSE);
      ++name;
      break;
    case '%':
      while (plen && *pat == '%') ++pat, --plen;
      if (! plen) return (TRUE);
      if (*pat=='_' || *pat=='?' || *pat=='[') {
	for (; nlen; ++name, --nlen)
	  if (Match (name, nlen, pat, plen)) return (TRUE);
      } else if (plen == 1)
	return (nlen>0 && *pat==name[nlen-1] ? TRUE : FALSE);
      else
	for (; nlen; ++name, --nlen)
	  if (*pat==*name && Match (name+1, nlen-1, pat+1, plen-1))
	    return (TRUE);
      return (FALSE);
    case '[':
      if (! nlen) return (FALSE);
      for (ok=0; plen && *pat != ']'; ++pat, --plen)
	if (*pat == *name) ok = 1;
      if (! ok) return (FALSE);
      if (plen) ++pat, --plen;
      ++name, --nlen;
      break;
    }
  }
}
