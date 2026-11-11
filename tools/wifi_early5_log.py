"""Full selected161 parent/calibration ledger for the isolated early-RX trial."""
from wifi_cal5_log import check as calibration


def check(log):
    return calibration(log, early=True)
