"""Bounded DHCP comparisons, not admission, radio proof or native acceptance."""
from wifi_http5_log import canonical_decimal
from http_dhcp_log import check_row as dhcp_row
PREFIX='wifi-dhcp-identity5'
KEYS='version epoch tx_discover tx_request tx_request_mask tx_invalid rx_client rx_self_discover rx_self_request rx_client_mismatch rx_client_mask rx_offer rx_ack rx_server_other rx_server_mask rx_malformed observation_only'

def observe(text):
    lines=text.splitlines();owned=[(i,s) for i,s in enumerate(lines) if s.startswith(PREFIX)]
    if not owned:return dict(present=False,native_accepted=False)
    if any(not s.startswith(PREFIX+': ') for _,s in owned):raise ValueError('unknown DHCP identity companion')
    dhcp=[(i,dhcp_row(s)) for i,s in enumerate(lines) if s.startswith('wifi-http5-dhcp:')]
    dhcp=[(i,d) for i,d in dhcp if d['version']]
    if len(owned)!=len(dhcp) or not 1<=len(owned)<=2:raise ValueError('missing/duplicate DHCP identity binding')
    rows=[]
    for generation,((at,s),(end,d)) in enumerate(zip(owned,dhcp),1):
        t=canonical_decimal(s,PREFIX,KEYS)
        if not at<end or (rows and at<=dhcp[generation-2][0]):raise ValueError('reversed DHCP identity binding')
        if t['version']!=1 or t['epoch']!=generation or t['observation_only']!=1:raise ValueError('invalid DHCP identity epoch/version')
        if any(t[k]>1023 for k in ('tx_request_mask','rx_client_mask','rx_server_mask')):raise ValueError('invalid DHCP identity mask')
        if not (t['tx_discover']<=d['discovers'] and t['tx_request']<=d['requests'] and
                t['tx_invalid']<=d['discovers']+d['requests']):raise ValueError('DHCP identity TX accounting differs')
        if not t['tx_invalid'] and (t['tx_discover']!=d['discovers'] or t['tx_request']!=d['requests'] or
                (t['tx_request'] and t['tx_request_mask']!=1023)):raise ValueError('missing valid DHCP construction witness')
        if not t['tx_request'] and t['tx_request_mask']:raise ValueError('phantom Request mask')
        if t['rx_self_discover']+t['rx_self_request']+t['rx_client_mismatch']!=t['rx_client']:raise ValueError('DHCP client comparison accounting differs')
        received=t['rx_client']+t['rx_offer']+t['rx_ack']+t['rx_server_other']
        if received>d['frames'] or t['rx_malformed']>received:raise ValueError('DHCP RX comparison accounting differs')
        if not t['rx_client'] and t['rx_client_mask']:raise ValueError('phantom client mask')
        if not t['rx_offer']+t['rx_ack']+t['rx_server_other'] and t['rx_server_mask']:raise ValueError('phantom server mask')
        rows.append(t)
    return dict(present=True,rows=rows,native_accepted=False,radio_or_server_receipt_proved=False,
                scope='Current-transaction field/checksum comparisons only; client echoes are not server replies or cryptographic packet identities; original protocol/native proofs remain required')
