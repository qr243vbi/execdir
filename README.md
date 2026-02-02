# execdir

A tool that lets you run a command in a specific directory. It supports shell commands and path aliases. **execdir** will try to get an alias if the path doesn't exist.

There is also a symlink called **x**, so you can type less using it.

## Examples

Run a command in a specific directory:

```text
$ execdir ~/Fedora/SCM/nq git status
On branch rawhide
Your branch is up to date with 'origin/rawhide'.

nothing to commit, working tree clean
```

Run a shell command in a specific directory:

```text
$ execdir -s ~/Desktop echo \$PWD
/home/xfgusta/Desktop
```

Create an alias for a path:

```text
$ execdir -n nq ~/Fedora/SCM/nq
```

Use an alias:

```text
$ execdir -aa nq pwd
/home/xfgusta/Fedora/SCM/nq
```

List all aliases:

```text
$ execdir -l
nq    /home/xfgusta/Fedora/SCM/nq
```

Delete an alias:

```text
$ execdir -r nq
```

## Installation

### From source

Use cmake to build and install execdir

```text
cmake -S . -B build
cmake --build build
sudo cmake --install build
```

## License

Copyright (c) 2022 Gustavo Costa. Distributed under the MIT license.
