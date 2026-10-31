# OrbitQueue: ML Ground Truth Label Policy
<!-- OWNER: Surbhi - replace this stub with the real implementation. Do not edit if you are not the owner. -->

## Overview
This document defines the synthetic labeling rules used by `ml/gen_data.py` to assign `TRANSMIT`, `DEFER`, or `REJECT` actions to training scenarios.

## TODO (Surbhi)
- [ ] Detail threshold rules for battery level, link quality, packet urgency, and data size.
- [ ] Document the ~5% label noise injection mechanism and borderline scenario distributions.
- [ ] Explain why the safety override for TT&C / HOUSEKEEPING is enforced at the router pipeline level rather than relying on ML inference alone.
