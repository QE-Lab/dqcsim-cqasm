# WORK IN PROGRESS

# DQCsim cQASM frontend

[![PyPi](https://badgen.net/pypi/v/dqcsim-cqasm)](https://pypi.org/project/dqcsim-cqasm/)

See also: [DQCsim](https://github.com/mbrobbel/dqcsim) and
[libqasm](https://github.com/QE-Lab/libqasm/).

This repository contains some glue code to allow DQCsim to run cQASM files
(file extension `*.cq`).

Temporary one-liner for running a basic test:

    sudo pip3 uninstall dqcsim-cqasm -y && rm -rf target/python/dist && python3 setup.py build && python3 setup.py bdist_wheel && sudo pip3 install target/python/dist/* && dqcsim test.cq qx
