"""Strict CPU-client exclusion qualification; never a hardware/range grant."""
import re
FIELDS='v selected acquired idle blocked snapshot preserved missing managed_pages pte_writes aperture_mappings released complete hardware_ownership'.split()
def handoff_result(text):
    rows=[]
    for line in text.splitlines():
        if not line.startswith('intel-handoff-'):continue
        if not line.startswith('intel-handoff-summary: '):raise ValueError('unknown handoff receipt')
        row={}
        for token in line.split(': ',1)[1].split():
            k,sep,v=token.partition('=')
            if not sep or k in row or not re.fullmatch(r'[0-9]{1,3}',v):raise ValueError('handoff field encoding/duplicate')
            row[k]=int(v)
        if set(row)!=set(FIELDS) or row['v']!=1:raise ValueError('handoff schema')
        rows.append(row)
    if not rows:return None
    if len(rows)!=1:raise ValueError('duplicate handoff receipt')
    s=rows[0]
    if any(s[k]>1 for k in ('selected','acquired','idle','snapshot','preserved','released','complete')):raise ValueError('handoff boolean')
    if s['managed_pages'] or s['pte_writes'] or s['aperture_mappings'] or s['hardware_ownership']:raise ValueError('handoff unsupported authority')
    if s['blocked']>127 or s['missing'] not in (31,95):raise ValueError('handoff refusal mask')
    if s['acquired']>s['selected'] or s['idle']>s['acquired'] or s['snapshot']>s['idle'] or s['preserved']>s['snapshot'] or s['released']>s['acquired']:raise ValueError('handoff ordering')
    if not s['idle'] and s['blocked']:raise ValueError('handoff operations without exclusion')
    if (s['missing']==31)!=bool(s['preserved']):raise ValueError('handoff admission attempt mismatch')
    complete=all(s[k] for k in ('selected','acquired','idle','snapshot','preserved','released')) and s['blocked']==127 and s['missing']==31
    if s['complete']!=int(complete):raise ValueError('handoff completion mismatch')
    return dict(qualification_complete=complete,native_qualification_complete=False,hardware_ownership=False,managed_range=False,physical_tearfree=False,summary=s)

def native_handoff_result(text):
    result=handoff_result(text)
    if not result or not result['qualification_complete']:return result
    claims=[line for line in text.splitlines() if line.startswith("intel-boot-claim:")]
    if claims!=["intel-boot-claim: v=1 policy=2 client=2 immutable=1 hardware_ownership=0 managed_pages=0"]:raise ValueError("handoff native software claim")
    from intel_ggtt_window import native_window_result
    window=native_window_result(text)
    if not window or not window['native_observation_complete']:raise ValueError('handoff native prerequisites')
    result['native_qualification_complete']=True
    return result
