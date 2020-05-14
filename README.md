# DQCsim cQASM frontend

[![PyPi](https://badgen.net/pypi/v/dqcsim-cqasm)](https://pypi.org/project/dqcsim-cqasm/)

See also: [DQCsim](https://github.com/mbrobbel/dqcsim) and
[libqasm](https://github.com/QE-Lab/libqasm/).

This repository contains some glue code to allow DQCsim to run cQASM files
(file extension `*.cq`).

## Status

The implementation is currently **incomplete**. The following things are
missing:

 - The error model specified in the cQASM file is ignored. You'll have to
   configure stuff related to error modeling yourself through the respective
   simulation backend and/or operators.

 - `measure_parity` is not implemented.

## Install

You can install using `pip` using `pip install dqcsim-cqasm` or equivalent.
If you're installing with `--user`, make sure that the path Python installs
the executables into is in your system path, otherwise DQCsim will not be
able to find the plugin. A simple way to see where the files are installed
is to run `pip uninstall dqcsim-cqasm`; it shows which files it's about to
delete before querying for confirmation.

## Building/installing from source

 - Make sure all git submodules are checked out:
   `git submodule update --init --recursive`

 - Build the wheel file locally:
   `python3 setup.py build bdist_wheel`

 - Install the wheel file you just built:
   `pip install target/python/dist/*` (or equivalent)

Don't push a build like this to PyPI or attempt to distribute this wheel; it
will likely only work locally due to hardcoded paths and the likes. Refer to
`release.md` for more info.

## Usage

Once the plugin is installed, DQCsim will parse any file with a `.cq` file
extension as a cQASM file through this plugin, so something like
`dqcsim test.cq qx` should work (assuming `dqcsim-qx` is installed and
`test.cq` exists).

The frontend ignores any arguments passed to its `run` callback and doesn't
have anything useful to return (it just echoes back what you passed it).

As with QX, the `display_binary` "gate" is used to print simulation results
to the terminal by means of printing the most recent measurement state. The
frontend also outputs the measurement register contents at the end of the
program as the ArbData returned by the `run()` call, in the following format:

```json
{
    "qubits": [
        {  /* data for qubit 0 */
            "value": <value in qubit measurement register as 0 or 1>,
            "average": <measurement averaging register value>,
            "raw": <raw value returned by backend; 0 or 1, or null for undefined>,
            "json": <JSON part of the optional ArbData associated with the measurement>,
            "binary": [
                [<bytes of binary string 0 of ArbData associated with measurement>],
                [<bytes of binary string 1>],
                <etc., empty list if no ArbData>
            ]
        },
        {<data for qubit 1>},
        {<data for qubit 2>},
        <etc. for all qubits>
    ]
}
```

The `raw`, `json`, and `binary` keys are not present when the qubit was never
measured at all. `value` defaults to 0 in that case. `average` is only present
when the qubit has been measured since the latest `reset_averaging` operation.

The `display` "gate" doesn't provide any additional information over
`display_binary`, as DQCsim frontends don't know what the actual quantum state
is; this information is private to the backend. Refer to the documentation of
the backend for ways to get additional information, if any. Be aware that
DQCsim's qubit numbering starts at one, so the cQASM frontend adds 1 to the
zero-referenced cQASM qubit indices to convert.

There is currently no way to attach arbs to gates or to the qubits themselves.
If you need to do this, you can write an operator and insert it immediately
behind the cQASM frontend.
