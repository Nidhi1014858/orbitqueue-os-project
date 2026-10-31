<!-- OWNER: Surbhi - replace this stub with the real implementation. Do not edit if you are not the owner. -->
# Section 3: Synthetic Data and Machine Learning Router

## 3.1 Data Disclaimer and Generation
All data used in OrbitQueue is synthetic and designed for educational evaluation of OS concepts. Scenarios model realistic satellite telemetry parameters including packet urgency, link quality, battery level, and payload size.

## 3.2 Decision Tree Classifier vs Baseline Rule
A compact decision tree (depth 4-5) predicts transmission scheduling (`TRANSMIT`, `DEFER`, `REJECT`), evaluated against a baseline threshold heuristic.
