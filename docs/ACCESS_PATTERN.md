# OrbitQueue: Memory Access Pattern Specification
<!-- OWNER: Nidhi - replace this stub with the real implementation. Do not edit if you are not the owner. -->

## Overview
This document specifies the sequence of memory region accesses performed by the router pipeline for each packet processing step.

## TODO (Nidhi)
- [ ] Define the logical access sequence across CODE, MODEL_PARAMS, INPUT_BUFFER, OUTPUT_LOG, and STACK.
- [ ] Document offsets accessed during packet arrival, classification lookup, Banker's check, and downlink log dispatch.
- [ ] Cross-reference region layouts with `include/mem_trace.h`.
