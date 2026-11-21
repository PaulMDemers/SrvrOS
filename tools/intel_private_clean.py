"""Optional private CPU writeback receipt. Never a GPU/DMA/cache ownership grant."""
import base64,struct
def private_clean(text,buffers,outer_begin,audit_begin,complete,inventory):
 rows=[l for l in text.splitlines() if l.startswith('intel-private-clean:')]
 if not rows:return None
 if len(rows)!=1 or not rows[0].startswith('intel-private-clean: v=2 b='):raise ValueError('private clean schema/coverage')
 encoded=rows[0].split('b=',1)[1]
 try:raw=base64.b64decode(encoded,validate=True)
 except ValueError as e:raise ValueError('private clean encoding') from e
 if len(raw)!=72 or base64.b64encode(raw).decode()!=encoded:raise ValueError('private clean canonical size')
 count,reason,fences,line=struct.unpack_from('<4B',raw);begin,end,signature,ebx,edx=struct.unpack_from('<2Q3I',raw,4)
 if count>2 or reason>7 or fences>4 or fences<2*count or line!=64:raise ValueError('private clean header')
 invalidations=list(struct.unpack_from('<2I',raw,64))
 spans=[]
 for n in range(2):
  physical,pages,lines=struct.unpack_from('<Q2I',raw,32+16*n);s=dict(physical=physical,pages=pages,lines=lines);spans.append(s)
  if pages>4096 or invalidations[n]>pages or lines>pages*64 or physical&4095 or physical>2**64-pages*4096:raise ValueError('private clean span bounds')
  if n>count and (lines or invalidations[n]):raise ValueError('private clean unread suffix')
  if n<count and (pages!=1356 or lines!=pages*64 or invalidations[n]!=pages or not physical):raise ValueError('private clean incomplete buffer')
 scope=bool(count==2 and not reason)
 if scope:
  if fences!=4 or signature!=0x306d4 or not edx&(1<<19) or ebx>>8&255!=8 or not 0<begin<end<=begin+100000:raise ValueError('private clean features/fences/deadline')
  if not inventory or not inventory['scope_complete'] or not complete or not 0<end<outer_begin<audit_begin:raise ValueError('private clean original audit/order')
  for s,b in zip(spans,buffers):
   if (s['physical'],s['pages'])!=(b['physical'],b['pages']):raise ValueError('private clean original backing')
 if complete and not scope:raise ValueError('private clean missing completed scope')
 return dict(v=2,invalidations=invalidations,cpu_translation_refreshed=scope,count=count,reason=reason,fences=fences,line_bytes=line,begin=begin,end=end,signature=signature,ebx=ebx,edx=edx,spans=spans,scope_complete=scope,cpu_writeback=scope,ownership=False,gpu_cache_completion=False,dma_completion=False,managed_pages=0)
