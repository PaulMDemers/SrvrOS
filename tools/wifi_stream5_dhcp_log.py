"""Explicit v136 bounded Discover-retry stream proof;no ordinary-stack claim."""
from wifi_stream5_log import check as stream_check,boot_identity as stream_boot,PROGRESS_DHCP

def boot_identity(log,release=None):
    return stream_boot(log,release,discover_retry=True)

def check(log,release=None):
    return stream_check(log,release,discover_retry=True)
