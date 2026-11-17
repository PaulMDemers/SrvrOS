"""Explicit aggregate policies; individual hardware/source limits are separate."""
ORIGINAL=dict(prepare=150000,gt_phase=100000,gt=300000,hda=400000,outer=500000)
SECONDS=dict(prepare=3000000,gt_phase=3000000,gt=4000000,hda=5000000,outer=6000000)
PRACTICAL=dict(prepare=250000,gt_phase=100000,gt=500000,hda=750000,outer=1000000)

def policy(*,seconds=False,practical=False):
    if type(seconds) is not bool or type(practical) is not bool or seconds and practical:
        raise ValueError('Choose one explicit GT timing policy')
    return PRACTICAL if practical else SECONDS if seconds else ORIGINAL
