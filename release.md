# Release process

The release process is as follows:

 - Update the version number in `setup.py`.
 - Update the version number in `src/main.cpp`.
 - Run `release.sh`. This will build the Python wheel in a manylinux docker
   container and stick it in `dist/*.whl`.
 - Install the wheel locally.
 - Test using `nosetests dqcsim_cqasm`
 - Push the wheel to PyPI with twine:

```
python3 -m pip install --user --upgrade twine
python3 -m twine upload dist/*
```

## Auditwheel notes

We're doing some pretty shifty stuff in the release script to have PyPI accept
the fact that we're dynamically linking against `libdqcsim.so`.

According to PEP-513, all wheels must be self-contained; that is, they should
include all but some very basic, very ancient C library stuff. The problem with
this, aside from the obvious (large file sizes, load times, missing security
patches because everyone would have to redistribute when a vulnerability is
found, ...), is two-fold:

 - If a bug is found in DQCsim, all plugins need to be rebuilt to link against
   the new version for the bug to be fixed.
 - If DQCsim's IPC protocol needs to be updated, old plugins would stop working
   with arcane errors.

Neither of these are acceptable within the context of a research problems.
Note that we're not the only ones having issues with this stuff (pyarrow is
another, rather big player).

So what we're doing is patching PyPa's packaging module that pulls in
dependencies (`auditwheel`) to whitelist `libdqcsim.so`. Basically, lying. It
ain't pretty, but that's the way it is right now.

While we were at it, we had to fix a couple other wheel-related stuff though
`auditwheel` patches:

 - The dependencies for binaries in the `.data` section of the wheel were not
   placed in the wheel correctly.
 - The correct tag for this kind of wheel (`libdqcsim.so` lies aside) is
   `py3-none-manylinux2010_x86_64`. However, `pip` is absolutely retarded in
   its compatibility check, and doesn't understand this *combination* of tags.
   The next best combination (for Python 3.6) is
   `py36-none-manylinux2010_x86_64`. So, to be able to install this package on
   various different Python versions, we need to make identical wheels with
   slightly different filenames.
 - We need to put sleeps in between the generation of aforementioned identical
   wheels to make their hashes different, otherwise PyPi rejects the identical
   file contents.
