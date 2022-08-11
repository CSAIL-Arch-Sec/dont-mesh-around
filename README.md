# Don't Mesh Around

This repository contains the code to reproduce the experiments from [*Don't Mesh Around: Side-Channel Attacks and Mitigations on Mesh Interconnects*](https://www.usenix.org/conference/usenixsecurity22/presentation/dai) **(USENIX 2022)**.

This code was tested on a bare-metal server with a 24-core Intel Xeon Gold 5220R (Cascade Lake) processor.
Because some details of the attack are specific to each individual processor, some reverse engineering will be needed before the experiments will work on your machine.
Additionally, if you are running on a different processor, additional work may be needed to port the code to your processor.

## Materials

This repository contains the following experiments:

1. `01-noc-reverse-engineering`: this code demonstrates the lane scheduling and priority arbitration policies by reproducing the two case studies discussed in the paper.
2. `02-covert-channel`: this code implements our covert channel and benchmarks its channel capacity.
3. `03-side-channel`: this code demonstrates extracting secrets from vulnerable cryptographic code (ECDSA and RSA). It includes the code used to train the classifier to distinguish between 0 and 1 bits.
4. `04-analytical-model`: this code contains the analytical model we build using the results of our reverse engineering. It also includes steps to validate the model and to support the discussion of our proposed software mitigation.

## Setup

There are a few setup steps required to set up the repository before running any experiments.

### Python Virtual Environment

In the `dont-mesh-around` directory, set up the virtual environment using `venv`.

```console
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
deactivate
```

## Citation

```bibtex
@inproceedings {dai2022mesh,
    title = {Don't Mesh Around: Side-Channel Attacks and Mitigations on Mesh Interconnects},
    author={Miles Dai and Riccardo Paccagnella and Miguel Gomez-Garcia and John McCalpin and Mengjia Yan},
    booktitle = {Proceedings of the USENIX Security Symposium (USENIX Security)},
    year = {2022},
}
```
