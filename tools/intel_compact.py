"""Strict v2 positional DWORD rows; normalized keys retain all raw values."""
import re
def compact_row(line,prefix,indices,words):
 pattern=re.escape(prefix)+r' v=2'+''.join(' '+re.escape(short)+r'=([0-9]{1,10})' for short,_ in indices)+r' d=([0-9]{1,10}(?:,[0-9]{1,10}){'+str(len(words)-1)+r'})'
 m=re.fullmatch(pattern,line)
 if not m:raise ValueError('compact receipt encoding')
 out={'v':2}
 for i,(_,name) in enumerate(indices):out[name]=int(m[i+1])
 for key,value in zip(words,m[len(indices)+1].split(',')):
  out[key]=int(value)
  if out[key]>=1<<32:raise ValueError('compact receipt DWORD')
 return out
