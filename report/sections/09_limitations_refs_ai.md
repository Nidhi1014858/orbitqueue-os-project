# Section 9: Limitations, References, Privacy, and AI Acknowledgement

## 9.1 Limitations

- The packet and training data are synthetic policy examples, not SomaiyaSat telemetry. Results cannot be generalized to operational spacecraft traffic.
- CPU slots, buffer units, and transmitter slots are abstract integer resources. They omit timing, hardware failures, radio regulations, and detailed power budgets.
- Paging and segmentation are simulated teaching models, not a real memory-management unit or flight-computer address space.
- The depth-5 decision tree is intentionally small and explainable. It cannot represent every possible interaction or changing contact condition.
- No real radio, modem, antenna, ground station, orbital pass, or link-budget calculation is connected to the project.
- Approximately 5% synthetic label noise models uncertainty but is not calibrated from real operator disagreements.
- A safe Banker's state depends on correct maximum claims. Incorrect Max values weaken the guarantee.

## 9.2 Privacy and responsible use

The repository contains generated educational values only and should not contain personal data, credentials, access tokens, private frequencies, or real operational telemetry. Future real-data work would require authorization, access controls, retention rules, and removal of sensitive identifiers before model training.

The ML action is advisory. The integrated router keeps critical TT&C and HOUSEKEEPING protection as explicit code rather than trusting a learned prediction.

## 9.3 Possible improvements

- Validate the policy on authorized, anonymized, expert-reviewed scenarios.
- Calibrate thresholds against an engineering link budget and measured power consumption.
- Compare the tree with cross-validation and additional interpretable models.
- Model resource duration and task arrival times instead of static integer claims.
- Add fault injection for lost links, corrupt frames, process crashes, and resource timeouts.
- Connect the simulation to a software-defined radio test environment without transmitting on unauthorized frequencies.

## 9.4 References

1. Silberschatz, A., Galvin, P. B., and Gagne, G. *Operating System Concepts*, 10th ed., Wiley, 2018.
2. Dijkstra, E. W. “Cooperating Sequential Processes,” in *Programming Languages*, Academic Press, 1968.
3. scikit-learn developers. “Decision Trees” and `DecisionTreeClassifier` documentation. https://scikit-learn.org/
4. Python Software Foundation. Python 3 documentation: `random`, `csv`, and `argparse`. https://docs.python.org/3/
5. IEEE and The Open Group. POSIX.1-2017. https://pubs.opengroup.org/onlinepubs/9699919799/

## 9.5 AI acknowledgement

AI tools were used to help plan the project, review experiment answers, and draft code structure. Surbhi's loaders, ML scripts, and Banker's Algorithm components were built and tested in an Ubuntu VM; other team-owned components must still be verified by their owners before submission. Every member is responsible for understanding and explaining the work. AI-generated text and code were reviewed against the project interfaces; no execution output, metric, or screenshot is claimed without a real run.
