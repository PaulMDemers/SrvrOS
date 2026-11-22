"""v133 bounded native page reuse;descriptor addresses never wrap."""
from wifi_provider5_log import check as parent
from wifi_power5_log import fields
from wifi_held_log import require
PROGRESS='wifi-reuse5-progress: original owner;four RX/four TX pages;validated native completions;copy/authenticate/release before reuse;no descriptor wrap'
def check(log):
 r=parent(log,reuse=True);lines=log.splitlines();rows=[x for x in lines if x.startswith('wifi-reuse5')]
 require(len(rows)==2 and rows[0]==PROGRESS and rows[1].startswith('wifi-reuse5:'),'missing native reuse lifecycle')
 s=fields(rows[1],'wifi-reuse5');fixed=dict(complete=1,initialized=1,tx_pages=4,rx_pages=4,usable_trbs=63,tx_posts=11,tx_reused=7,early_release_refused=1,events_drained=1,retired=1,faults=0,descriptor_wraps=0,retained=1,ordinary_rx=0)
 require(set(s)==set(fixed)|{'rx_posts','completed','borrowed','released','rx_reused','drain_events'} and all(s[k]==v for k,v in fixed.items()),'incomplete or unsafe native reuse')
 retire=fields(next(x for x in lines if x.startswith('wifi-retire5:')),'wifi-retire5');posts=retire['posts'];copied=retire['forward']+retire['drained']
 require(s['rx_posts']==posts and 5<=posts<=48 and s['borrowed']==s['released']==copied and s['rx_reused']==posts-4 and 11+copied<=s['completed']<=11+posts and 0<=s['drain_events']<32,'wrong reusable completion/page/borrow accounting')
 wpa=next(x for x in lines if x.startswith('wifi-wpa5-progress:'));rf=next(x for x in lines if x.startswith('wifi-channel-vco5:'));provider=next(x for x in lines if x.startswith('wifi-provider5:'))
 require(lines.index(rows[0])<lines.index(wpa)<lines.index(rf)<lines.index(rows[1])<lines.index(provider),'reuse outside original full-parent lifetime')
 r.update(reuse5_complete=True,reuse5_pages_per_direction=4,reuse5_tx_reused=7,reuse5_rx_reused=s['rx_reused'],reuse5_ledger_retired=True,reuse5_events_drained=True,reuse5_descriptor_wrap=False,reuse5_ordinary_rx=False)
 r['scope']='bounded real USB page reuse with original CCMP owner,six encrypted IP requests and full late ledger/provider/hardware retirement;no descriptor wrap or ordinary provider RX/TCP/HTTP acceptance'
 return r
